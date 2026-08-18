/**
 * Copyright (C) 2022-2026, Alexander Saal <developer@olbaflinx.chm-projects.de>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "ui/AccountFetch.h"

#include "core/Banking/Balance/Balance.h"
#include "core/Banking/Banking.h"
#include "core/Banking/Transaction/Transaction.h"
#include "core/Result.h"
#include "core/Storage/Storage.h"
#include "ui/BankingGui.h"
#include "ui/ErrorMessage.h"
#include "ui/Logging.h"

#include <QtCore/QDate>
#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QMetaObject>
#include <QtCore/QTimer>

#include <utility>

using namespace olbaflinx::core;
using namespace olbaflinx::core::banking;
using namespace olbaflinx::core::banking::account;
using namespace olbaflinx::core::banking::balance;
using namespace olbaflinx::core::banking::transaction;
using namespace olbaflinx::core::storage;
using namespace olbaflinx::ui;

namespace {

/** How long a write waits before it asks a busy storage again. */
constexpr int StoreRetryMs = 50;

} // namespace

class AccountFetch::Private
{
public:
    /**
     * Which of the two writing runs is going. The bookings and the balance
     * cannot travel in one: the storage refuses a second run while one is
     * going, and the balance has to follow the account it hangs on.
     */
    enum class StorePhase { None, Transactions, Balances };

    /**
     * One account waiting for the storage, with what the session brought it.
     *
     * A run stores account by account: the storage takes one writing run at a
     * time, and a balance has to follow the bookings of the account it hangs on.
     */
    struct PendingAccount
    {
        BankingItems bookings;
        BankingItems balances;
    };

    Private(AccountFetch *fetch, ApplicationInfo info, Storage *appStorage)
        : applicationInfo(std::move(info))
        , storage(appStorage)
        , q_ptr(fetch)
    {}

    ~Private()
    {
        // The order matters and is the reason the backend carries no Qt parent.
        // Banking still reaches into the interface while it shuts down, in
        // AB_Gui_Unextend, and the interface would already be gone by then.
        banking.reset();
        gui.reset();
    }

    /** Whether this object is the one the storage is answering right now. */
    [[nodiscard]] bool isStoring() const { return phase != StorePhase::None; }

    void takeSessionResult(const BankingItems &items) { received = items; }

    /** Whether the session brought something that is to be written. */
    [[nodiscard]] bool sessionDelivered() const
    {
        return outcome == Outcome::Received || outcome == Outcome::BalanceOnly;
    }

    /**
     * Sorts what the session brought into one entry per account, in the order
     * the records arrived in.
     *
     * The order is the one the storage is walked in afterwards, so a run that is
     * stopped halfway has written whole accounts and nothing else.
     */
    void queueReceived()
    {
        QList<quint32> order;
        QHash<quint32, PendingAccount> byAccount;

        for (const auto &item : std::as_const(received)) {
            const auto balance = std::dynamic_pointer_cast<Balance>(item);
            const auto booking = balance ? nullptr : std::dynamic_pointer_cast<Transaction>(item);

            if (!balance && !booking) {
                continue;
            }

            const quint32 uniqueAccountId = balance ? balance->uniqueAccountId()
                                                    : booking->uniqueAccountId();

            if (!byAccount.contains(uniqueAccountId)) {
                order.append(uniqueAccountId);
            }

            if (balance) {
                byAccount[uniqueAccountId].balances.append(item);
            } else {
                byAccount[uniqueAccountId].bookings.append(item);
            }
        }

        received.clear();
        pending.clear();

        for (const quint32 uniqueAccountId : std::as_const(order)) {
            pending.append(byAccount.value(uniqueAccountId));
        }
    }

    /**
     * The end of the session, whichever way it went. Only a session that came
     * back with records goes on to the storage.
     */
    void sessionEnded()
    {
        if (!running) {
            return;
        }

        // The banking layer cannot report this one. An abort is smoothed away
        // on the way up through the library and reaches it as a plain success
        // with an empty result, which would tell the user his bank had nothing
        // new. The interface saw the wish and is asked instead. Only where the
        // session claims to have gone through: a failure it did report names
        // its own cause and keeps it.
        if (sessionDelivered() && gui && gui->userAborted()) {
            outcome = Outcome::Aborted;
        }

        if (overAllAccounts) {
            allSessionsEnded();
            return;
        }

        if (!sessionDelivered()) {
            finish(outcome, reason);
            return;
        }

        queueReceived();

        // An account the bank had nothing new for. Nothing is written, and the
        // outcome says so with a count of nought.
        if (pending.isEmpty()) {
            finish(outcome, {});
            return;
        }

        beginNextAccount();
    }

    /**
     * The end of a run over several accounts.
     *
     * Nothing is written before this point, and that is what makes the question
     * after an abort answerable at all: discarding is a matter of not writing
     * rather than of removing rows that already stand.
     */
    void allSessionsEnded()
    {
        if (outcome == Outcome::Aborted) {
            // What the stop cut off was stopped and not turned down. Counting it
            // as failed would tell the user his bank had refused accounts.
            summary.failed = 0;

            queueReceived();

            if (pending.isEmpty()) {
                finishAll(Outcome::Aborted, {});
                return;
            }

            waitingForAbortAnswer = true;

            Q_EMIT q_ptr->abortNeedsAnswer();
            return;
        }

        if (!sessionDelivered()) {
            finishAll(outcome, reason);
            return;
        }

        queueReceived();

        if (pending.isEmpty()) {
            finishAll(outcome, {});
            return;
        }

        beginNextAccount();
    }

    /**
     * Begins the storing of the account at the head of the queue, and ends the
     * run once nothing is left in it.
     */
    void beginNextAccount()
    {
        while (!pending.isEmpty() && pending.constFirst().bookings.isEmpty()
               && pending.constFirst().balances.isEmpty()) {
            pending.removeFirst();
        }

        if (pending.isEmpty()) {
            finishRun(outcome, {});
            return;
        }

        startStoring(pending.constFirst().bookings.isEmpty() ? StorePhase::Balances
                                                             : StorePhase::Transactions);
    }

    void startStoring(StorePhase next)
    {
        storeFailed = false;
        storedBeforeRun = storedCount;

        const auto &account = pending.constFirst();

        const auto error = storage->storeItems(next == StorePhase::Transactions ? account.bookings
                                                                                : account.balances);

        // The phase is set where a run of this class actually started, and only
        // there. It is what tells the three signals of the write path apart from
        // those of a run somebody else has going; carrying it while no run of
        // this one is out would take a foreign count and a foreign end for this
        // fetch.
        if (!error.isError()) {
            phase = next;
            return;
        }

        // No run was started, so no end of one will arrive and the outcome has
        // to be settled here.
        //
        // A write of somebody else holding the way is a moment and not a
        // failure. What this fetch brought in cost minutes on the line and is
        // still here, so it is asked again rather than given up; giving up would
        // have the next fetch bring the same records once more.
        if (error.code() == ErrorCode::Busy) {
            QTimer::singleShot(StoreRetryMs, q_ptr, [this, next] {
                if (running) {
                    startStoring(next);
                }
            });
            return;
        }

        // The bookings are written in a run of their own and are committed by the
        // time the balances are attempted. A phase that never started therefore
        // takes nothing back with it, and only the failure of the first one
        // leaves the holding of this account as it was.
        if (next == StorePhase::Transactions) {
            storedCount = storedBeforeRun;
        }

        phase = StorePhase::None;
        finishRun(Outcome::StoreFailed, {});
    }

    void takeStoredCount(int count)
    {
        if (phase != StorePhase::Transactions) {
            return;
        }

        // The bookings are what the user is told about. A balance is one row
        // whichever way it goes and says nothing about what came in.
        //
        // Added rather than set: a run over several accounts writes once per
        // account, and the figure the user is given is the one over all of them.
        storedCount = storedBeforeRun + count;
    }

    /**
     * The end of a writing run. It arrives on every path, after a failure as
     * well, which is why the outcome is decided here and not at the failure.
     */
    void storeRunEnded()
    {
        if (!isStoring()) {
            return;
        }

        if (storeFailed) {
            // Nothing of this run stayed behind: the storage brackets a run and
            // rolls it back whole. That reaches this run and no further. The
            // bookings go in a run of their own and are committed before the
            // balances are attempted, so a failure of the balances leaves them
            // standing and the count that goes out has to say so. The same holds
            // for the accounts that were written before this one.
            if (phase == StorePhase::Transactions) {
                storedCount = storedBeforeRun;
            }

            phase = StorePhase::None;
            finishRun(Outcome::StoreFailed, {});
            return;
        }

        if (phase == StorePhase::Transactions && !pending.constFirst().balances.isEmpty()) {
            startStoring(StorePhase::Balances);
            return;
        }

        phase = StorePhase::None;
        pending.removeFirst();

        // Whatever the session said it delivered, which is what the user is told
        // apart: a balance alone is not an account without new bookings.
        beginNextAccount();
    }

    /** Ends the run through the way out that belongs to the kind it is. */
    void finishRun(Outcome ended, const QString &endedReason)
    {
        if (overAllAccounts) {
            finishAll(ended, endedReason);
            return;
        }

        finish(ended, endedReason);
    }

    /** What every way out shares, whichever kind of run it ends. */
    void closeRun()
    {
        running = false;
        phase = StorePhase::None;
        waitingForAbortAnswer = false;

        received.clear();
        pending.clear();

        // The span the cached PIN outlives a fetch by starts here, at every way
        // out. Held while the session ran, because a session asks for the PIN
        // once per signed message.
        if (gui) {
            gui->expirePasswordCacheLater();
        }
    }

    void finish(Outcome ended, const QString &endedReason)
    {
        closeRun();

        const int count = storedCount;
        storedCount = 0;

        Q_EMIT q_ptr->ended(ended, count, endedReason);
    }

    void finishAll(Outcome ended, const QString &endedReason)
    {
        closeRun();

        summary.outcome = ended;
        summary.reason = endedReason;
        summary.storedCount = storedCount;

        // What is left over from the three that were counted along the way. An
        // account the bank holds no order for is among them: it has online
        // access and nothing about it failed.
        summary.fetched = qMax(0, attempted - summary.skipped - summary.failed);

        const Summary reported = summary;

        summary = {};
        attempted = 0;
        storedCount = 0;
        overAllAccounts = false;

        Q_EMIT q_ptr->allEnded(reported);
    }

    ApplicationInfo applicationInfo;
    Storage *storage;

    // The interface is declared before the instance that uses it, so that the
    // instance is destroyed first even where the destructor above is not the
    // one that runs.
    std::unique_ptr<BankingGui> gui;
    std::unique_ptr<Banking> banking;

    bool running = false;
    StorePhase phase = StorePhase::None;
    bool storeFailed = false;
    int storedCount = 0;

    /** What the count stood at when the running writing run was started. */
    int storedBeforeRun = 0;

    /** Whether the run covers every account, which decides how it is reported. */
    bool overAllAccounts = false;

    /** Whether the run stands still waiting for the answer to an abort. */
    bool waitingForAbortAnswer = false;

    /** How many accounts a run over all of them was handed. */
    int attempted = 0;

    Summary summary;

    Outcome outcome = Outcome::Received;
    QString reason;

    BankingItems received;
    QList<PendingAccount> pending;

private:
    AccountFetch *q_ptr;
};

AccountFetch::AccountFetch(ApplicationInfo applicationInfo, Storage *storage, QObject *parent)
    : QObject(parent)
    , d_ptr(std::make_unique<Private>(this, std::move(applicationInfo), storage))
{}

AccountFetch::~AccountFetch() = default;

Error AccountFetch::initialize()
{
    if (d_ptr->banking) {
        return {};
    }

    // Checked at runtime and not left to a compiler: the field belongs to an
    // aggregate, and one that names only the fields before it leaves this one
    // empty without a word. The application would then reach a bank without
    // identifying itself.
    if (d_ptr->applicationInfo.registrationKey.isEmpty()) {
        return Error(ErrorCode::InvalidInput,
                     QStringLiteral("A fetch needs the registration key of the application"));
    }

    // The interface belongs here and stays here. The wizard holds one of its
    // own: two banking instances that extend the same interface abort the
    // process on the second extension.
    auto gui = std::make_unique<BankingGui>();
    auto banking = std::make_unique<Banking>(d_ptr->applicationInfo);

    if (const auto error = banking->initialize(d_ptr->applicationInfo.name,
                                               d_ptr->applicationInfo.version,
                                               d_ptr->applicationInfo.registrationKey,
                                               gui->getCInterface());
        error.isError()) {
        return error;
    }

    d_ptr->gui = std::move(gui);
    d_ptr->banking = std::move(banking);

    connect(d_ptr->banking.get(), &Banking::itemsReceived, this, [this](const BankingItems &items) {
        d_ptr->takeSessionResult(items);
    });

    // The four that speak about a single account. A run over one of them says
    // what became of it through its outcome; a run over all of them counts
    // instead, because one outcome cannot stand for twenty accounts.
    connect(d_ptr->banking.get(), &Banking::accountSkipped, this, [this](quint32, const QString &) {
        if (d_ptr->overAllAccounts) {
            ++d_ptr->summary.skipped;
            return;
        }

        d_ptr->outcome = Outcome::Skipped;
    });

    connect(d_ptr->banking.get(), &Banking::noOrderOffered, this, [this](quint32) {
        if (!d_ptr->overAllAccounts) {
            d_ptr->outcome = Outcome::NothingOffered;
        }
    });

    // Arrives before the session and says that no booking can come in for this
    // account. Without it an empty result would read like an account the bank
    // had nothing new for.
    connect(d_ptr->banking.get(), &Banking::transactionsNotOffered, this, [this](quint32) {
        if (!d_ptr->overAllAccounts) {
            d_ptr->outcome = Outcome::BalanceOnly;
        }
    });

    // Only a run over several accounts is reported this way. A run over one has
    // nothing to go on with, so its refusal arrives as a failure of the whole
    // fetch.
    connect(d_ptr->banking.get(), &Banking::accountFailed, this, [this](quint32) {
        ++d_ptr->summary.failed;
    });

    connect(d_ptr->banking.get(), &Banking::aborted, this, [this] {
        d_ptr->outcome = Outcome::Aborted;
    });

    connect(d_ptr->banking.get(),
            &Banking::errorOccurred,
            this,
            [this](ErrorCode code, const QString &technicalReason) {
                // The technical message can name a return value of a foreign
                // library and is not translated. It goes to the log; what the
                // user reads is made from the code alone.
                qCWarning(lcUi) << "error from the banking backend:" << technicalReason;

                d_ptr->outcome = Outcome::Failed;
                d_ptr->reason = userMessage(code);
            });

    connect(d_ptr->banking.get(), &Banking::finished, this, [this] { d_ptr->sessionEnded(); });

    // The three of the write path. A read of another caller may well be going at
    // the same time and ends with signals of its own, so none of these three can
    // be answered by it; the phase is what says whether the run that ends here
    // belongs to this class.
    connect(d_ptr->storage, &Storage::itemsStored, this, [this](int count) {
        d_ptr->takeStoredCount(count);
    });

    connect(d_ptr->storage, &Storage::writeFailed, this, [this](ErrorCode, const QString &) {
        if (d_ptr->isStoring()) {
            d_ptr->storeFailed = true;
        }
    });

    connect(d_ptr->storage, &Storage::writeFinished, this, [this] { d_ptr->storeRunEnded(); });

    return {};
}

void AccountFetch::start(const std::shared_ptr<Account> &account)
{
    if (d_ptr->running || account == nullptr) {
        return;
    }

    d_ptr->running = true;
    d_ptr->overAllAccounts = false;
    d_ptr->outcome = Outcome::Received;
    d_ptr->reason.clear();
    d_ptr->storedCount = 0;
    d_ptr->pending.clear();

    Q_EMIT started();

    // Reported through the event loop rather than from here, so that a fetch
    // answers after it has returned. A caller that sets its own state after the
    // call would otherwise see the end of a fetch before its start.
    const auto failLater = [this](const QString &failure) {
        QMetaObject::invokeMethod(
            this,
            [this, failure] { d_ptr->finish(Outcome::Failed, failure); },
            Qt::QueuedConnection);
    };

    if (const auto error = initialize(); error.isError()) {
        qCCritical(lcUi) << "the banking backend of the window did not come up:" << error.message();

        failLater(userMessage(error.code()));
        return;
    }

    // The mark belongs to one session. Left standing it would end the next one
    // as an abort the user never asked for.
    d_ptr->gui->forgetAbort();
    d_ptr->gui->holdPasswordCache();

    const Result<QDate> latest = d_ptr->storage->latestTransactionDate(account->uniqueId());
    if (!latest.hasValue()) {
        // Without the starting point the session would ask the bank for the
        // whole holding again. That is not what a failed read is to bring
        // about, so nothing is sent.
        qCWarning(lcUi) << "could not read the starting point of a fetch:"
                        << latest.error().message();

        failLater(userMessage(latest.error().code()));
        return;
    }

    d_ptr->banking->fetchAccount(*account, latest.value());
}

bool AccountFetch::isPasswordCacheExpiring() const
{
    return d_ptr->gui && d_ptr->gui->isPasswordCacheExpiring();
}

void AccountFetch::clearPasswordCache()
{
    if (d_ptr->gui) {
        d_ptr->gui->clearPasswordCache();
    }
}

void AccountFetch::startAll(const QList<std::shared_ptr<Account>> &accounts)
{
    if (d_ptr->running || accounts.isEmpty()) {
        return;
    }

    d_ptr->running = true;
    d_ptr->overAllAccounts = true;
    d_ptr->outcome = Outcome::Received;
    d_ptr->reason.clear();
    d_ptr->storedCount = 0;
    d_ptr->summary = {};
    d_ptr->attempted = 0;
    d_ptr->pending.clear();

    Q_EMIT started();

    // Reported through the event loop rather than from here, so that a fetch
    // answers after it has returned. A caller that sets its own state after the
    // call would otherwise see the end of a fetch before its start.
    const auto failLater = [this](const QString &failure) {
        QMetaObject::invokeMethod(
            this,
            [this, failure] { d_ptr->finishAll(Outcome::Failed, failure); },
            Qt::QueuedConnection);
    };

    if (const auto error = initialize(); error.isError()) {
        qCCritical(lcUi) << "the banking backend of the window did not come up:" << error.message();

        failLater(userMessage(error.code()));
        return;
    }

    // The mark belongs to one session. Left standing it would end the next one
    // as an abort the user never asked for.
    d_ptr->gui->forgetAbort();
    d_ptr->gui->holdPasswordCache();

    QList<std::shared_ptr<Account>> reachable;
    QHash<quint32, QDate> startingPoints;
    QString firstFailure;

    for (const auto &account : accounts) {
        if (account == nullptr) {
            continue;
        }

        ++d_ptr->attempted;

        const Result<QDate> latest = d_ptr->storage->latestTransactionDate(account->uniqueId());
        if (!latest.hasValue()) {
            // Without the starting point the session would ask the bank for the
            // whole holding of this account again. It is left out rather than
            // asked for that way, and the accounts beside it still run.
            qCWarning(lcUi) << "could not read the starting point of a fetch:"
                            << latest.error().message();

            if (firstFailure.isEmpty()) {
                firstFailure = userMessage(latest.error().code());
            }

            ++d_ptr->summary.failed;
            continue;
        }

        startingPoints.insert(account->uniqueId(), latest.value());
        reachable.append(account);
    }

    // Not one account is left to ask about, and the reason is a storage that
    // could not be read. Nothing is sent, and the run ends where it stands.
    if (reachable.isEmpty()) {
        failLater(firstFailure);
        return;
    }

    d_ptr->banking->fetchAccounts(reachable, startingPoints);
}

void AccountFetch::answerAbort(bool keep)
{
    if (!d_ptr->waitingForAbortAnswer) {
        return;
    }

    d_ptr->waitingForAbortAnswer = false;
    d_ptr->summary.keptAfterAbort = keep;

    if (!keep) {
        // Not a row of this run stands in the file, so there is nothing to take
        // back. The holding is the one from before the fetch started.
        d_ptr->finishAll(Outcome::Aborted, {});
        return;
    }

    d_ptr->beginNextAccount();
}
