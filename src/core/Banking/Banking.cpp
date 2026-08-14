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

#include "core/Banking/Banking.h"

#include "core/Banking/Balance/Balance.h"
#include "core/Banking/Transaction/Transaction.h"
#include "core/Logging.h"

#include <chipcard/client.h>

#include <gwenhywfar/dialog.h>
#include <gwenhywfar/error.h>
#include <gwenhywfar/gui.h>
#include <gwenhywfar/gwendate.h>
#include <gwenhywfar/gwenhywfar.h>

#include <aqbanking/banking.h>
#include <aqbanking/banking_dialogs.h>
#include <aqbanking/banking_online.h>
#include <aqbanking/banking_transaction.h>

#include <aqbanking/error.h>
#include <aqbanking/gui/abgui.h>
#include <aqbanking/types/account_spec.h>
#include <aqbanking/types/balance.h>

#include <QtConcurrent/QtConcurrentRun>

#include <QtCore/QFutureWatcher>
#include <QtCore/QMetaObject>
#include <QtCore/QSet>

#include <memory>
#include <utility>

#ifndef AB_SUCCESS
#define AB_SUCCESS GWEN_SUCCESS
#endif

#ifndef AB_ERROR
#define AB_ERROR GWEN_ERROR_GENERIC
#endif

using namespace olbaflinx::core;
using namespace olbaflinx::core::banking;
using namespace olbaflinx::core::banking::balance;
using namespace olbaflinx::core::banking::transaction;

namespace {

/**
 * The backend AqBanking gives an account that has no online access at all. An
 * account without any backend name is the other half of the same case.
 */
constexpr auto offlineBackendName = QLatin1StringView("aqnone");

/**
 * How far a fetch reaches back behind the holding it already has. A bank
 * corrects bookings after it has reported them, and a fetch that started where
 * the holding ends would never see the correction.
 */
constexpr int fetchLeadDays = 30;

/**
 * The C structures of the backend, held so that every path out of a function
 * releases them: the success, the failure and the abort alike.
 */
struct CommandListDeleter
{
    void operator()(AB_TRANSACTION_LIST2 *list) const { AB_Transaction_List2_freeAll(list); }
};

using CommandListPtr = std::unique_ptr<AB_TRANSACTION_LIST2, CommandListDeleter>;
using ContextPtr = std::unique_ptr<AB_IMEXPORTER_CONTEXT, decltype(&AB_ImExporterContext_free)>;
using GwenDatePtr = std::unique_ptr<GWEN_DATE, decltype(&GWEN_Date_free)>;

/**
 * Hands a date to the banking backend. Ownership stays with the caller, the
 * setters of AB_TRANSACTION duplicate what they are given.
 *
 * A date that is not set answers with an empty handle, which the setters read
 * as "no date", and the bank then delivers what it holds.
 */
GwenDatePtr fromDate(const QDate &date)
{
    if (!date.isValid() || date.isNull()) {
        return {nullptr, &GWEN_Date_free};
    }

    const auto text = date.toString(QStringLiteral("yyyyMMdd")).toLatin1();

    return {GWEN_Date_fromString(text.constData()), &GWEN_Date_free};
}

/**
 * Whether an order ended in a failure of its own.
 *
 * A status the backend never touched is not one: an order that ran through and
 * brought nothing is no failure, an empty result is a result.
 */
bool hasFailed(AB_TRANSACTION_STATUS status)
{
    return status == AB_Transaction_StatusError || status == AB_Transaction_StatusRejected
           || status == AB_Transaction_StatusAborted || status == AB_Transaction_StatusRevoked;
}

/** The accounts whose fetch is to be dropped as a whole. */
QSet<quint32> accountsOfFailedCommands(AB_TRANSACTION_LIST2 *commands)
{
    QSet<quint32> accounts;

    AB_TRANSACTION_LIST2_ITERATOR *iterator = AB_Transaction_List2_First(commands);
    if (iterator == nullptr) {
        return accounts;
    }

    AB_TRANSACTION *command = AB_Transaction_List2Iterator_Data(iterator);
    while (command != nullptr) {
        if (hasFailed(AB_Transaction_GetStatus(command))) {
            accounts.insert(AB_Transaction_GetUniqueAccountId(command));
        }
        command = AB_Transaction_List2Iterator_Next(iterator);
    }

    AB_Transaction_List2Iterator_free(iterator);

    return accounts;
}

/**
 * Whether the first balance is to be preferred over the second: the booked one
 * wins over every other kind whatever date it carries, among equals the more
 * recent one, and among those of the same date the one delivered last.
 *
 * AB_Balance_List_GetLatestByType would answer the first two, but it keeps the
 * earlier of two with the same date, which is the opposite of the third.
 */
bool isPreferredOver(const AB_BALANCE *candidate, const AB_BALANCE *chosen)
{
    const bool candidateIsBooked = AB_Balance_GetType(candidate) == AB_Balance_TypeBooked;
    const bool chosenIsBooked = AB_Balance_GetType(chosen) == AB_Balance_TypeBooked;

    if (candidateIsBooked != chosenIsBooked) {
        return candidateIsBooked;
    }

    const GWEN_DATE *candidateDate = AB_Balance_GetDate(candidate);
    const GWEN_DATE *chosenDate = AB_Balance_GetDate(chosen);

    if (candidateDate == nullptr || chosenDate == nullptr) {
        return chosenDate == nullptr;
    }

    return GWEN_Date_Compare(candidateDate, chosenDate) >= 0;
}

/** The one balance of an account that is kept, or null if the bank sent none. */
const AB_BALANCE *chooseBalance(const AB_BALANCE_LIST *balances)
{
    if (balances == nullptr) {
        return nullptr;
    }

    const AB_BALANCE *chosen = nullptr;

    for (const AB_BALANCE *balance = AB_Balance_List_First(balances); balance != nullptr;
         balance = AB_Balance_List_Next(balance)) {
        if (chosen == nullptr || isPreferredOver(balance, chosen)) {
            chosen = balance;
        }
    }

    return chosen;
}

/**
 * What one run of a session hands back to the thread that started it.
 *
 * It carries the answer already read out of the container: the container and the
 * orders belong to the session and are gone by the time this arrives.
 */
struct SessionResult
{
    FetchOutcome outcome = FetchOutcome::Failed;
    BankingItems items;
    QString reason;
};

} // namespace

class Banking::Private
{
public:
    explicit Private(Banking *banking, ApplicationInfo applicationInfo)
        : gwenGui(nullptr)
        , aqBanking(nullptr)
        , m_isInitialized(false)
        , m_chipCardClient(nullptr)
        , m_applicationInfo(std::move(applicationInfo))
        , q_ptr(banking)
    {}

    ~Private() { finalize(); }

    Error initialize(const QString &name, const QString &version, const QString &key, GWEN_GUI *gui)
    {
        if (name.isEmpty() || version.isEmpty()) {
            return Error(ErrorCode::InvalidInput,
                         QStringLiteral("Banking needs an application name and a version, got "
                                        "\"%1\" and \"%2\"")
                             .arg(name, version));
        }

        // Refused here rather than left to abort the process later. gwenhywfar
        // asserts on a missing interface the moment it takes a file lock, and
        // AB_Banking_Fini takes one, so the failure would surface far from its
        // cause. A caller without a display passes GWEN_Gui_new().
        if (gui == nullptr) {
            return Error(ErrorCode::InvalidInput,
                         QStringLiteral("Banking needs a user interface, GWEN_Gui_new() will do"));
        }

        // We don't initialize AQ Banking & Gwen GUI twice. The flag used to be
        // derived from the two handles, which no longer works: a caller may pass
        // no user interface at all, and then gwenGui stays null on purpose.
        if (m_isInitialized) {
            return Error(ErrorCode::InvalidInput,
                         QStringLiteral("The banking backend is already initialized"));
        }

        int rv = GWEN_Init();
        if (rv != AB_SUCCESS) {
            return Error(ErrorCode::BankingFailure,
                         QStringLiteral("GWEN_Init failed with %1").arg(rv));
        }

        // The interface belongs to the caller. Building it here would drag Qt
        // Widgets into a library that is meant to be usable without a display.
        gwenGui = gui;
        GWEN_Gui_SetGui(gwenGui);

        const QByteArray local8BitName = name.toLocal8Bit();
        aqBanking = AB_Banking_new(local8BitName.data(), nullptr, 0);

        const QByteArray local8BitKey = key.toLocal8Bit();
        AB_Banking_RuntimeConfig_SetCharValue(aqBanking,
                                              "fintsRegistrationKey",
                                              local8BitKey.data());

        const QByteArray local8BitVersion = version.toLocal8Bit();
        AB_Banking_RuntimeConfig_SetCharValue(aqBanking,
                                              "fintsApplicationVersionString",
                                              local8BitVersion.data());

        rv = AB_Banking_Init(aqBanking);
        if (rv != AB_SUCCESS) {
            return Error(ErrorCode::BankingFailure,
                         QStringLiteral("AB_Banking_Init failed with %1").arg(rv));
        }

        AB_Gui_Extend(gwenGui, aqBanking);

        const QByteArray local8BitAppName = m_applicationInfo.name.toLocal8Bit();
        const QByteArray local8BitAppVersion = m_applicationInfo.version.toLocal8Bit();
        m_chipCardClient = LC_Client_new(local8BitAppName.constData(),
                                         local8BitAppVersion.constData());

        LC_Client_Init(m_chipCardClient);

        m_isInitialized = (aqBanking != nullptr);
        if (!m_isInitialized) {
            return Error(ErrorCode::BankingFailure,
                         QStringLiteral("The banking backend did not come up"));
        }

        qCInfo(lcBanking) << "banking backend initialized for" << name << version;

        return {};
    }

    /**
     * The session, run in a thread of its own.
     *
     * Takes over the orders and releases them, the container with them, on every
     * way out. What leaves here are records of the core, which no longer touch
     * the banking backend.
     */
    SessionResult runSession(AB_TRANSACTION_LIST2 *orders, quint32 uniqueAccountId)
    {
        const CommandListPtr commands(orders);
        const ContextPtr context(AB_ImExporterContext_new(), &AB_ImExporterContext_free);

        // The interface of gwenhywfar lives per thread. The one set where
        // initialize ran does not reach this thread, and without one here the
        // library would abort the process instead of reporting a failure.
        GWEN_Gui_SetGui(gwenGui);

        const int rv = AB_Banking_SendCommands(aqBanking, commands.get(), context.get());

        // Detached before anything else happens, so that the thread leaves the
        // interface as it found it whichever way the session ended.
        GWEN_Gui_SetGui(nullptr);

        SessionResult result;
        result.outcome = Banking::outcomeOfSession(rv, commands.get(), uniqueAccountId);

        switch (result.outcome) {
        case FetchOutcome::Received:
            result.items = Banking::itemsFromContext(context.get(), commands.get());
            break;

        case FetchOutcome::Aborted:
            // Nothing of an aborted account is handed over. The user stopped
            // before the answer was complete, and half an answer is worse than
            // none.
            break;

        case FetchOutcome::Failed:
            result.reason = rv != AB_SUCCESS
                                ? QStringLiteral("AB_Banking_SendCommands failed with %1").arg(rv)
                                : QStringLiteral("An order of account %1 was refused")
                                      .arg(uniqueAccountId);
            break;
        }

        return result;
    }

    /** The answer of a session, reported in the thread this object belongs to. */
    void deliverSessionResult()
    {
        const SessionResult result = fetchWatcher.result();

        m_isFetching = false;

        switch (result.outcome) {
        case FetchOutcome::Received:
            qCDebug(lcBanking) << "fetched" << result.items.size() << "records";
            Q_EMIT q_ptr->itemsReceived(result.items);
            break;

        case FetchOutcome::Aborted:
            qCInfo(lcBanking) << "the fetch was aborted by the user";
            Q_EMIT q_ptr->aborted();
            break;

        case FetchOutcome::Failed:
            qCCritical(lcBanking) << result.reason;
            Q_EMIT q_ptr->errorOccurred(ErrorCode::BankingFailure, result.reason);
            break;
        }

        Q_EMIT q_ptr->finished();
    }

    bool isFetching() const { return m_isFetching; }

    void startFetch(AB_TRANSACTION_LIST2 *orders, quint32 uniqueAccountId)
    {
        m_isFetching = true;

        fetchWatcher.setFuture(QtConcurrent::run(
            [this, orders, uniqueAccountId] { return runSession(orders, uniqueAccountId); }));
    }

    void finalize()
    {
        // A session reaches into the banking backend from its own thread.
        // Pulling the backend away under it would leave it writing into freed
        // memory, so the shutdown waits for it. The window keeps this from
        // happening in the first place: it puts off closing while a fetch runs
        // and points at the progress dialog for stopping it.
        fetchWatcher.waitForFinished();
        m_isFetching = false;

        if (isInitialized()) {
            AB_Gui_Unextend(gwenGui);

            int rv = AB_Banking_Fini(aqBanking);
            if (rv == AB_SUCCESS) {
                AB_Banking_free(aqBanking);
            }

            // The interface is detached, not freed. It belongs to whoever passed
            // it to initialize, and freeing it here would release it a second
            // time when that owner goes.
            GWEN_Gui_SetGui(nullptr);
            GWEN_Fini();

            LC_Client_Fini(m_chipCardClient);
            LC_Client_free(m_chipCardClient);

            gwenGui = nullptr;
            aqBanking = nullptr;
            m_chipCardClient = nullptr;
        }

        if (m_chipCardClient != nullptr) {
            LC_Client_Fini(m_chipCardClient);
            LC_Client_free(m_chipCardClient);
            m_chipCardClient = nullptr;
        }
        m_isInitialized = false;
    }

    bool isInitialized() const { return m_isInitialized; }

    int setupAccounts() const
    {
        if (!isInitialized()) {
            return AB_ERROR;
        }

        auto setupDialog = AB_Banking_CreateSetupDialog(aqBanking);
        auto dialogTitle = tr("%1 Account Setup").arg(m_applicationInfo.name).toLocal8Bit();

        GWEN_Dialog_SetCharProperty(setupDialog,
                                    nullptr,
                                    GWEN_DialogProperty_Title,
                                    0,
                                    dialogTitle.constData(),
                                    0);

        GWEN_Dialog_SetWidgetText(setupDialog, nullptr, dialogTitle.constData());

        auto result = GWEN_Gui_ExecDialog(setupDialog, 0);
        GWEN_Dialog_free(setupDialog);

        return result;
    }

    BankingItems accounts(const AB_ACCOUNT_SPEC_LIST *list)
    {
        auto specList = AB_AccountSpec_List_dup(list);
        const auto totalAccounts = AB_AccountSpec_List_GetCount(specList);

        if (totalAccounts == 0) {
            return {};
        }

        quint32 index = 0;
        auto accountList = BankingItems();

        auto accountSpec = AB_AccountSpec_List_First(specList);
        while (accountSpec) {
            accountList.append(std::make_shared<Account>(accountSpec));
            accountSpec = AB_AccountSpec_List_Next(accountSpec);

            const auto percentage = index * 100.0 / totalAccounts;
            Q_EMIT q_ptr->progressValueChanged(percentage);

            ++index;
        }

        AB_AccountSpec_List_free(specList);
        specList = nullptr;

        std::sort(accountList.begin(),
                  accountList.end(),
                  [](const BankingItemPtr &first, const BankingItemPtr &second) {
                      return std::static_pointer_cast<Account>(first)->accountName()
                             < std::static_pointer_cast<Account>(second)->accountName();
                  });

        return accountList;
    }

    GWEN_GUI *gwenGui;
    AB_BANKING *aqBanking;
    QFutureWatcher<SessionResult> fetchWatcher;

private:
    bool m_isFetching = false;
    bool m_isInitialized;
    LC_CLIENT *m_chipCardClient;
    ApplicationInfo m_applicationInfo;

    friend class Banking;
    Banking *q_ptr;
};

Banking::Banking(ApplicationInfo applicationInfo, QObject *parent)
    : QObject(parent)
{
    d_ptr = new Private(this, std::move(applicationInfo));

    // The answer of a session crosses back here, into the thread this object
    // belongs to. Every signal of a fetch leaves from there and none from the
    // session itself.
    connect(&d_ptr->fetchWatcher, &QFutureWatcherBase::finished, this, [this] {
        d_ptr->deliverSessionResult();
    });
}

Banking::~Banking()
{
    // The watcher may still hold a queued end of a session for this object.
    // Cutting the connection before the private part goes keeps it from being
    // delivered into freed memory.
    disconnect(&d_ptr->fetchWatcher, nullptr, this, nullptr);

    delete d_ptr;
}

Error Banking::initialize(const QString &name,
                          const QString &version,
                          const QString &key,
                          GWEN_GUI *gui)
{
    return d_ptr->initialize(name, version, key, gui);
}

void Banking::finalize()
{
    d_ptr->finalize();
}

int Banking::setupAccounts()
{
    return d_ptr->setupAccounts();
}

void Banking::accounts()
{
    const auto reportError = [this](ErrorCode code, const QString &message) {
        qCCritical(lcBanking) << message;

        Q_EMIT errorOccurred(code, message);
        Q_EMIT finished();
    };

    if (!d_ptr->isInitialized()) {
        reportError(ErrorCode::BankingFailure,
                    QStringLiteral("The banking backend is not initialized"));
        return;
    }

    AB_ACCOUNT_SPEC_LIST *specList = nullptr;

    const int rv = AB_Banking_GetAccountSpecList(d_ptr->aqBanking, &specList);

    // A backend that holds no account answers with GWEN_ERROR_NOT_FOUND, which
    // is not a failure of the call. It used to be reported as one, which left
    // the NotFound branch below unreachable and told a user who has not set up
    // an account yet that their banking backend was broken.
    if (rv == GWEN_ERROR_NOT_FOUND) {
        reportError(ErrorCode::NotFound, QStringLiteral("No accounts were found"));
        return;
    }

    if (rv != AB_SUCCESS) {
        reportError(ErrorCode::BankingFailure,
                    QStringLiteral("AB_Banking_GetAccountSpecList failed with %1").arg(rv));
        return;
    }

    auto accounts = d_ptr->accounts(specList);

    AB_AccountSpec_List_free(specList);
    specList = nullptr;

    if (accounts.isEmpty()) {
        reportError(ErrorCode::NotFound, QStringLiteral("No accounts were found"));
        return;
    }

    qCDebug(lcBanking) << "read" << accounts.size() << "accounts";

    Q_EMIT itemsReceived(accounts);

    Q_EMIT finished();
}

AB_TRANSACTION_LIST2 *Banking::buildFetchCommands(const Account &account,
                                                  const QDate &latestStoredDate)
{
    // An account without a stored booking keeps an invalid date, and the order
    // then goes out without a starting point at all.
    const QDate startingPoint = latestStoredDate.isValid()
                                    ? latestStoredDate.addDays(-fetchLeadDays)
                                    : QDate();

    const auto addCommand = [&account, &startingPoint](AB_TRANSACTION_LIST2 *list,
                                                       AB_TRANSACTION_COMMAND kind) {
        AB_TRANSACTION *command = AB_Transaction_new();

        AB_Transaction_SetCommand(command, kind);

        // Fills the identifiers of the local account out of its description, the
        // unique id among them. The backend needs all of them, and it is the one
        // that knows which.
        AB_Banking_FillTransactionFromAccountSpec(command, account.accountSpec());

        // The period travels in FirstDate. The field belongs to the group of
        // standing orders by its name, but the FinTS backend reads it as the day
        // a fetch starts at. No end date is set, so the bank delivers up to what
        // it holds today.
        if (const GwenDatePtr first = fromDate(startingPoint); first) {
            AB_Transaction_SetFirstDate(command, first.get());
        }

        // StringIdForApplication stays untouched here and everywhere else:
        // AB_Transaction_free does not release it.

        AB_Transaction_List2_PushBack(list, command);
    };

    AB_TRANSACTION_LIST2 *commands = AB_Transaction_List2_new();

    addCommand(commands, AB_Transaction_CommandGetTransactions);
    addCommand(commands, AB_Transaction_CommandGetBalance);

    return commands;
}

BankingItems Banking::itemsFromContext(const AB_IMEXPORTER_CONTEXT *context,
                                       AB_TRANSACTION_LIST2 *commands)
{
    const QSet<quint32> failedAccounts = accountsOfFailedCommands(commands);

    BankingItems items = {};

    const AB_IMEXPORTER_ACCOUNTINFO *accountInfo = AB_ImExporterContext_GetFirstAccountInfo(context);

    while (accountInfo != nullptr) {
        const quint32 uniqueAccountId = AB_ImExporterAccountInfo_GetAccountId(accountInfo);

        // All or nothing per account. One failed order takes the other one down
        // with it, so that no half fetched account reaches the storage.
        if (failedAccounts.contains(uniqueAccountId)) {
            qCWarning(lcBanking) << "an order of account" << uniqueAccountId
                                 << "failed, the account is dropped as a whole";

            accountInfo = AB_ImExporterAccountInfo_List_Next(accountInfo);
            continue;
        }

        // Both filters open: every booking the bank sent belongs to the account
        // it was sent for.
        const AB_TRANSACTION *transaction
            = AB_ImExporterAccountInfo_GetFirstTransaction(accountInfo,
                                                           AB_Transaction_TypeNone,
                                                           AB_Transaction_CommandNone);

        while (transaction != nullptr) {
            items.append(std::make_shared<Transaction>(transaction));

            transaction = AB_Transaction_List_FindNextByType(transaction,
                                                             AB_Transaction_TypeNone,
                                                             AB_Transaction_CommandNone);
        }

        if (const AB_BALANCE *balance = chooseBalance(
                AB_ImExporterAccountInfo_GetBalanceList(accountInfo));
            balance != nullptr) {
            if (uniqueAccountId == 0) {
                // A balance carries no account of its own, it belongs to the
                // entry it sits in. Without an id there is nothing to store it
                // against, whereas a booking carries its account itself.
                qCWarning(lcBanking) << "a balance arrived without an account id and is dropped";
            } else {
                items.append(std::make_shared<Balance>(uniqueAccountId, balance));
            }
        }

        accountInfo = AB_ImExporterAccountInfo_List_Next(accountInfo);
    }

    return items;
}

FetchOutcome Banking::outcomeOfSession(int sessionResult,
                                       AB_TRANSACTION_LIST2 *commands,
                                       quint32 uniqueAccountId)
{
    // The user stopping the session is the one non-zero result that is no
    // failure. A session cut in the middle, say because the far end went away,
    // answers with something else and stays a failure.
    if (sessionResult == GWEN_ERROR_USER_ABORTED) {
        return FetchOutcome::Aborted;
    }

    if (sessionResult != AB_SUCCESS) {
        return FetchOutcome::Failed;
    }

    // A session can come back successful and still carry an order the bank
    // refused. Without this the account would answer with an empty list, and a
    // refusal would read like an account with nothing new.
    if (accountsOfFailedCommands(commands).contains(uniqueAccountId)) {
        return FetchOutcome::Failed;
    }

    return FetchOutcome::Received;
}

void Banking::fetchAccount(const Account &account, const QDate &latestStoredDate)
{
    // Reported through the event loop rather than from here, so that every fetch
    // answers after it has returned. A caller that sets its own state after the
    // call would otherwise see the end of a fetch before its start.
    const auto reportLater = [this](auto report) {
        QMetaObject::invokeMethod(this, std::move(report), Qt::QueuedConnection);
    };

    if (d_ptr->isFetching()) {
        const QString reason = QStringLiteral("A fetch is already running");

        qCWarning(lcBanking) << reason;

        // No finished here. It belongs to the fetch that is running, and a
        // second one would declare that one over.
        reportLater([this, reason] { Q_EMIT errorOccurred(ErrorCode::InvalidInput, reason); });
        return;
    }

    if (!d_ptr->isInitialized()) {
        const QString reason = QStringLiteral("The banking backend is not initialized");

        qCCritical(lcBanking) << reason;

        reportLater([this, reason] {
            Q_EMIT errorOccurred(ErrorCode::BankingFailure, reason);
            Q_EMIT finished();
        });
        return;
    }

    // An account without a backend brings down the whole run rather than itself:
    // AqBanking sorts the queues by backend before it sends anything and answers
    // GWEN_ERROR_BAD_DATA for an account that carries none. Passing such an
    // account over is therefore what keeps the accounts beside it running.
    const QString backendName = account.backendName();
    if (backendName.isEmpty() || backendName.compare(offlineBackendName, Qt::CaseInsensitive) == 0) {
        const QString reason = tr("The account has no online access");
        const quint32 uniqueAccountId = account.uniqueId();

        qCInfo(lcBanking) << "account" << uniqueAccountId << "skipped, no online access";

        reportLater([this, uniqueAccountId, reason] {
            Q_EMIT accountSkipped(uniqueAccountId, reason);
            Q_EMIT finished();
        });
        return;
    }

    // Built here and handed over: the orders need nothing of the backend, and
    // the account they are built from belongs to the caller and must not be
    // reached into once this call has returned.
    d_ptr->startFetch(buildFetchCommands(account, latestStoredDate), account.uniqueId());
}
