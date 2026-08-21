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

#include "ui/StandingOrderFetch.h"

#include "core/Banking/Banking.h"
#include "core/Banking/StandingOrder/StandingOrder.h"
#include "core/Storage/Storage.h"
#include "ui/BankingGui.h"
#include "ui/BankingSession.h"
#include "ui/ErrorMessage.h"
#include "ui/Logging.h"

#include <QtCore/QHash>
#include <QtCore/QMetaObject>
#include <QtCore/QSet>
#include <QtCore/QTimer>

#include <utility>

using namespace olbaflinx::core;
using namespace olbaflinx::core::banking;
using namespace olbaflinx::core::banking::account;
using namespace olbaflinx::core::banking::standingorder;
using namespace olbaflinx::core::storage;
using namespace olbaflinx::ui;

namespace {

/** How long a write waits before it asks a busy storage again. */
constexpr int StoreRetryMs = 50;

} // namespace

class StandingOrderFetch::Private
{
public:
    /**
     * One account waiting for the storage, with what the session brought it.
     *
     * An account without a single order is in the queue as well: an answer
     * without orders is what marks the whole holding of that account as ended,
     * and skipping it would leave orders standing that the bank no longer has.
     */
    struct PendingAccount
    {
        quint32 accountId = 0;
        BankingItems orders;

        /**
         * Whether the fetch of this account went through. Only then does the
         * write path mark what it did not carry; a run that was cut off says
         * nothing about what the institution still holds.
         */
        bool succeeded = false;
    };

    Private(StandingOrderFetch *fetch, BankingSession *bankingSession, Storage *appStorage)
        : session(bankingSession)
        , storage(appStorage)
        , q_ptr(fetch)
    {}

    /** The interface of the session, or nothing before it came up. */
    [[nodiscard]] BankingGui *gui() const { return session->gui(); }

    /** Whether this object is the one the storage is answering right now. */
    [[nodiscard]] bool isStoring() const { return storing; }

    void takeSessionResult(const BankingItems &items) { received = items; }

    /**
     * Sorts what the session brought into one entry per account, and puts an
     * entry there for every account the session reached without one.
     *
     * The order is the one the accounts were asked in, so a run that is stopped
     * halfway has written whole accounts and nothing else.
     */
    void queueReceived(bool succeeded)
    {
        QHash<quint32, BankingItems> byAccount;

        for (const auto &item : std::as_const(received)) {
            const auto order = std::dynamic_pointer_cast<StandingOrder>(item);
            if (!order) {
                continue;
            }

            byAccount[order->uniqueAccountId()].append(item);
        }

        received.clear();
        pending.clear();

        for (const quint32 accountId : std::as_const(asked)) {
            if (unreachable.contains(accountId)) {
                continue;
            }

            const BankingItems orders = byAccount.value(accountId);

            // A run that did not go through writes what it brought and nothing
            // else. An account it brought nothing for has nothing to write and
            // nothing to say, so it stays out of the queue.
            if (!succeeded && orders.isEmpty()) {
                continue;
            }

            pending.append(PendingAccount{accountId, orders, succeeded});
        }
    }

    /**
     * The end of the session, whichever way it went. Only a session that came
     * back goes on to the storage.
     */
    void sessionEnded()
    {
        if (!running) {
            return;
        }

        // The banking layer cannot report this one. An abort is smoothed away on
        // the way up through the library and reaches it as a plain success with
        // an empty result, which would mark the whole holding as ended. The
        // interface saw the wish and is asked instead. Only where the session
        // claims to have gone through: a failure it did report names its own
        // cause and keeps it.
        if (outcome == Outcome::Received && gui() && gui()->userAborted()) {
            outcome = Outcome::Aborted;
        }

        if (outcome == Outcome::Aborted) {
            if (!overAllAccounts) {
                // Nothing of a stopped fetch over one account is written, so
                // there is nothing to ask about.
                finishRun(Outcome::Aborted, {});
                return;
            }

            // What the stop cut off was stopped and not turned down. Counting it
            // as failed would tell the user his bank had refused accounts.
            summary.failed = 0;

            queueReceived(false);

            if (pending.isEmpty()) {
                finishAll(Outcome::Aborted, {});
                return;
            }

            waitingForAbortAnswer = true;

            Q_EMIT q_ptr->abortNeedsAnswer();
            return;
        }

        if (outcome != Outcome::Received) {
            finishRun(outcome, reason);
            return;
        }

        queueReceived(true);

        // Every account of the run was passed over, refused, or holds no request
        // for standing orders. Nothing is written, and nothing is marked.
        if (pending.isEmpty()) {
            finishRun(outcome, {});
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
        if (pending.isEmpty()) {
            finishRun(outcome, {});
            return;
        }

        startStoring();
    }

    void startStoring()
    {
        storeFailed = false;
        storedBeforeRun = storedCount;

        const auto &account = pending.constFirst();

        const auto error = storage->storeItems(account.orders,
                                               {account.accountId, account.succeeded});

        // The mark is set where a run of this class actually started, and only
        // there. It is what tells the three signals of the write path apart from
        // those of a run somebody else has going.
        if (!error.isError()) {
            storing = true;
            return;
        }

        // No run was started, so no end of one will arrive and the outcome has
        // to be settled here.
        //
        // A write of somebody else holding the way is a moment and not a
        // failure. What this fetch brought in cost minutes on the line and is
        // still here, so it is asked again rather than given up.
        if (error.code() == ErrorCode::Busy) {
            QTimer::singleShot(StoreRetryMs, q_ptr, [this] {
                if (running) {
                    startStoring();
                }
            });
            return;
        }

        qCCritical(lcUi) << "the standing orders of account" << account.accountId
                         << "could not be written:" << error.message();

        finishRun(Outcome::StoreFailed, {});
    }

    void takeStoredCount(int count)
    {
        if (!storing) {
            return;
        }

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
        if (!storing) {
            return;
        }

        storing = false;

        if (storeFailed) {
            // Nothing of this run stayed behind: the storage brackets a run and
            // rolls it back whole, the mark on the orders it did not carry
            // included. That reaches this run and no further, so the accounts
            // written before it stand and the count has to say so.
            storedCount = storedBeforeRun;

            finishRun(Outcome::StoreFailed, {});
            return;
        }

        pending.removeFirst();

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
        storing = false;
        waitingForAbortAnswer = false;

        received.clear();
        pending.clear();
        asked.clear();
        unreachable.clear();

        // The span the cached PIN outlives a fetch by starts here, at every way
        // out. Held while the session ran, because a session asks for the PIN
        // once per signed message.
        if (gui()) {
            gui()->expirePasswordCacheLater();
        }

        // Given back at the very end, so that a second fetch cannot start while
        // the result of this one is still on its way into the storage.
        session->endFetch();
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

        // What is left over from the three that were counted along the way.
        summary.fetched = qMax(0, attempted - summary.skipped - summary.failed - summary.notOffered);

        const Summary reported = summary;

        summary = {};
        attempted = 0;
        storedCount = 0;
        overAllAccounts = false;

        Q_EMIT q_ptr->allEnded(reported);
    }

    /** Begins a run of either kind, up to the point where the orders go out. */
    void beginRun(bool overAll)
    {
        running = true;
        overAllAccounts = overAll;
        outcome = Outcome::Received;
        reason.clear();
        storedCount = 0;
        summary = {};
        attempted = 0;

        received.clear();
        pending.clear();
        asked.clear();
        unreachable.clear();
    }

    /**
     * Notes an account the session will bring nothing for, so that the write
     * path is not reached for it. Its holding stays as it is: no answer of the
     * bank means no statement about what it still holds.
     */
    void noteUnreachable(quint32 accountId) { unreachable.insert(accountId); }

    BankingSession *session;
    Storage *storage;

    /** Whether the signals of the session are listened to yet. */
    bool connected = false;

    bool running = false;
    bool storing = false;
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

    /** The accounts of the run, in the order they were asked in. */
    QList<quint32> asked;

    /** Those among them the session brought no answer for. */
    QSet<quint32> unreachable;

    Summary summary;

    Outcome outcome = Outcome::Received;
    QString reason;

    BankingItems received;
    QList<PendingAccount> pending;

private:
    StandingOrderFetch *q_ptr;
};

StandingOrderFetch::StandingOrderFetch(BankingSession *session, Storage *storage, QObject *parent)
    : QObject(parent)
    , d_ptr(std::make_unique<Private>(this, session, storage))
{}

StandingOrderFetch::~StandingOrderFetch() = default;

Error StandingOrderFetch::initialize()
{
    if (d_ptr->connected) {
        return {};
    }

    if (const auto error = d_ptr->session->initialize(); error.isError()) {
        return error;
    }

    Banking *banking = d_ptr->session->banking();

    // Every kind of fetch listens to the same instance, and each of them hears
    // what the others are told. The mark of a running fetch of this class is
    // what tells them apart: without it a fetch of the bookings would leave its
    // outcome behind here, to be reported by the next fetch of this one.
    connect(banking, &Banking::itemsReceived, this, [this](const BankingItems &items) {
        if (!d_ptr->running) {
            return;
        }

        d_ptr->takeSessionResult(items);
    });

    connect(banking,
            &Banking::accountSkipped,
            this,
            [this](quint32 uniqueAccountId, const QString &) {
                if (!d_ptr->running) {
                    return;
                }

                d_ptr->noteUnreachable(uniqueAccountId);

                if (d_ptr->overAllAccounts) {
                    ++d_ptr->summary.skipped;
                    return;
                }

                d_ptr->outcome = Outcome::Skipped;
            });

    connect(banking, &Banking::noOrderOffered, this, [this](quint32 uniqueAccountId) {
        if (!d_ptr->running) {
            return;
        }

        d_ptr->noteUnreachable(uniqueAccountId);

        if (!d_ptr->overAllAccounts) {
            d_ptr->outcome = Outcome::NothingOffered;
        }
    });

    // Arrives before the session and says that this account carries no request
    // for standing orders. Without it an empty result would read like an account
    // that holds none, and the mark would end every order it does hold.
    connect(banking, &Banking::standingOrdersNotOffered, this, [this](quint32 uniqueAccountId) {
        if (!d_ptr->running) {
            return;
        }

        qCInfo(lcUi) << "account" << uniqueAccountId << "carries no request for standing orders";

        d_ptr->noteUnreachable(uniqueAccountId);

        if (d_ptr->overAllAccounts) {
            ++d_ptr->summary.notOffered;
            return;
        }

        d_ptr->outcome = Outcome::NotOffered;
    });

    // Only a run over several accounts is reported this way. A run over one has
    // nothing to go on with, so its refusal arrives as a failure of the whole
    // fetch.
    connect(banking, &Banking::accountFailed, this, [this](quint32 uniqueAccountId) {
        if (!d_ptr->running) {
            return;
        }

        qCWarning(lcUi) << "the bank refused the standing orders of account" << uniqueAccountId;

        d_ptr->noteUnreachable(uniqueAccountId);

        ++d_ptr->summary.failed;
    });

    connect(banking, &Banking::aborted, this, [this] {
        if (!d_ptr->running) {
            return;
        }

        qCInfo(lcUi) << "the standing order fetch was stopped by the user";

        d_ptr->outcome = Outcome::Aborted;
    });

    connect(banking,
            &Banking::errorOccurred,
            this,
            [this](ErrorCode code, const QString &technicalReason) {
                if (!d_ptr->running) {
                    return;
                }

                // The technical message can name a return value of a foreign
                // library and is not translated. It goes to the log; what the
                // user reads is made from the code alone.
                qCCritical(lcUi) << "error from the banking backend:" << technicalReason;

                d_ptr->outcome = Outcome::Failed;
                d_ptr->reason = userMessage(code);
            });

    connect(banking, &Banking::finished, this, [this] { d_ptr->sessionEnded(); });

    // The three of the write path. A read of another caller may well be going at
    // the same time and ends with signals of its own, so none of these three can
    // be answered by it; the mark is what says whether the run that ends here
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

    d_ptr->connected = true;

    return {};
}

void StandingOrderFetch::start(const std::shared_ptr<Account> &account)
{
    if (account == nullptr || !d_ptr->session->beginFetch()) {
        return;
    }

    d_ptr->beginRun(false);
    d_ptr->asked.append(account->uniqueId());

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
    d_ptr->gui()->forgetAbort();
    d_ptr->gui()->holdPasswordCache();

    d_ptr->session->banking()->fetchStandingOrders(*account);
}

void StandingOrderFetch::startAll(const QList<std::shared_ptr<Account>> &accounts)
{
    if (accounts.isEmpty() || !d_ptr->session->beginFetch()) {
        return;
    }

    d_ptr->beginRun(true);

    Q_EMIT started();

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
    d_ptr->gui()->forgetAbort();
    d_ptr->gui()->holdPasswordCache();

    QList<std::shared_ptr<Account>> reachable;

    for (const auto &account : accounts) {
        if (account == nullptr) {
            continue;
        }

        ++d_ptr->attempted;

        d_ptr->asked.append(account->uniqueId());
        reachable.append(account);
    }

    if (reachable.isEmpty()) {
        failLater({});
        return;
    }

    d_ptr->session->banking()->fetchStandingOrdersForAll(reachable);
}

void StandingOrderFetch::answerAbort(bool keep)
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
