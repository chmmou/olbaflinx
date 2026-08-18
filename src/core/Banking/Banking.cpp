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

/**
 * Puts the interface of this instance into the slot of the running thread and
 * empties the slot again on every way out.
 *
 * gwenhywfar keeps one interface per thread, not one per instance, so two
 * instances that are up at once displace each other there. Whoever reaches into
 * the library sets its own rather than relying on what the slot happens to
 * hold, and leaves it empty afterwards: what stood there before belongs to
 * another instance and is free to go at any moment.
 */
class ThreadGui
{
public:
    explicit ThreadGui(GWEN_GUI *gui) { GWEN_Gui_SetGui(gui); }
    ~ThreadGui() { GWEN_Gui_SetGui(nullptr); }

    ThreadGui(const ThreadGui &) = delete;
    ThreadGui &operator=(const ThreadGui &) = delete;
    ThreadGui(ThreadGui &&) = delete;
    ThreadGui &operator=(ThreadGui &&) = delete;
};

using CommandListPtr = std::unique_ptr<AB_TRANSACTION_LIST2, CommandListDeleter>;
using ContextPtr = std::unique_ptr<AB_IMEXPORTER_CONTEXT, decltype(&AB_ImExporterContext_free)>;
using GwenDatePtr = std::unique_ptr<GWEN_DATE, decltype(&GWEN_Date_free)>;
using AccountSpecPtr = std::unique_ptr<AB_ACCOUNT_SPEC, decltype(&AB_AccountSpec_free)>;

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
 * Puts the orders of one account at the end of a list that may already carry
 * those of others.
 *
 * A fetch over all accounts sends one list, so the orders are appended rather
 * than handed back in a list of their own: a list per account would have to be
 * emptied into the shared one afterwards, and the ownership of an order would
 * then hang on which of the two lists it currently sits in. The list the
 * orders go into owns them.
 *
 * Only what the backend names as offered is built; nothing at all means both
 * orders are built.
 */
void appendFetchCommands(AB_TRANSACTION_LIST2 *commands,
                         const Account &account,
                         const QDate &latestStoredDate,
                         const AB_ACCOUNT_SPEC *offered)
{
    // An account without a stored booking keeps an invalid date, and the order
    // then goes out without a starting point at all.
    const QDate startingPoint = latestStoredDate.isValid()
                                    ? latestStoredDate.addDays(-fetchLeadDays)
                                    : QDate();

    const auto addCommand = [&account, &startingPoint, commands](AB_TRANSACTION_COMMAND kind) {
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

        AB_Transaction_List2_PushBack(commands, command);
    };

    // Only what the account carries. An order the backend cannot build for it
    // never reaches the bank: it is marked as failed while the queue is filled,
    // and that failure counts against the whole account, balance included.
    if (Banking::accountOffers(offered, AB_Transaction_CommandGetTransactions)) {
        addCommand(AB_Transaction_CommandGetTransactions);
    }

    if (Banking::accountOffers(offered, AB_Transaction_CommandGetBalance)) {
        addCommand(AB_Transaction_CommandGetBalance);
    }
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

    /** Whether the backend holds an order for the bookings of this account. */
    bool offersTransactions = true;

    /** Whether it holds none at all, in which case no session was run. */
    bool offersNothing = false;
};

/**
 * What one run over several accounts hands back to the thread that started it.
 *
 * The outcome is the one of the call, not of a single account: the backend
 * answers success whatever a single institution did, and what became of an
 * account stands on its own orders. The three lists say it per account.
 */
struct AllSessionsResult
{
    FetchOutcome outcome = FetchOutcome::Failed;
    BankingItems items;
    QString reason;

    /** Accounts whose orders went out. */
    QList<quint32> sent;

    /** Accounts of those whose orders the bank refused. */
    QList<quint32> failed;

    /** Accounts the backend holds no order of any kind for. Nothing was sent. */
    QList<quint32> nothingOffered;

    /** Accounts the backend holds no order for the bookings of. */
    QList<quint32> transactionsNotOffered;
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

        // We don't initialize AQ Banking & Gwen GUI twice. The flag says so
        // rather than the two handles: a caller may pass no user interface at
        // all, and then gwenGui stays null on purpose.
        if (m_isInitialized) {
            return Error(ErrorCode::InvalidInput,
                         QStringLiteral("The banking backend is already initialized"));
        }

        int rv = GWEN_Init();
        if (rv != AB_SUCCESS) {
            return Error(ErrorCode::BankingFailure,
                         QStringLiteral("GWEN_Init failed with %1").arg(rv));
        }

        // From here on every way out runs finalize, which takes back exactly
        // what has been reached. Each of these flags stands for one step that
        // has its own counterpart, and hanging the shutdown on them rather than
        // on m_isInitialized is what makes a failure halfway through undoable:
        // that one is set at the very end, and a failure before it would leave
        // the backend, the counterpart of GWEN_Init and a thread-global
        // interface behind.
        m_gwenInitialized = true;

        // The interface belongs to the caller. Building it here would drag Qt
        // Widgets into a library that is meant to be usable without a display.
        gwenGui = gui;
        GWEN_Gui_SetGui(gwenGui);

        const QByteArray local8BitName = name.toLocal8Bit();
        aqBanking = AB_Banking_new(local8BitName.data(), nullptr, 0);

        // Asked before it is used, not after. The three calls below take it
        // without looking.
        if (aqBanking == nullptr) {
            finalize();

            return Error(ErrorCode::BankingFailure,
                         QStringLiteral("AB_Banking_new answered with nothing"));
        }

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
            finalize();

            return Error(ErrorCode::BankingFailure,
                         QStringLiteral("AB_Banking_Init failed with %1").arg(rv));
        }

        m_bankingInitialized = true;

        AB_Gui_Extend(gwenGui, aqBanking);
        m_guiExtended = true;

        const QByteArray local8BitAppName = m_applicationInfo.name.toLocal8Bit();
        const QByteArray local8BitAppVersion = m_applicationInfo.version.toLocal8Bit();
        m_chipCardClient = LC_Client_new(local8BitAppName.constData(),
                                         local8BitAppVersion.constData());

        if (m_chipCardClient != nullptr) {
            // A machine without a running smart card service is the everyday
            // case for whoever signs with a PIN, and this failing is no reason
            // to keep the user from their bank. What it must not do is go
            // unnoticed: the call takes its own half-built state down before it
            // returns, so the counterpart below would run on a context that was
            // never established.
            const int rv = LC_Client_Init(m_chipCardClient);
            if (rv != 0) {
                qCInfo(lcBanking) << "no card reader service is available:" << rv;
            } else {
                m_chipCardInitialized = true;
            }
        }

        m_isInitialized = true;

        qCInfo(lcBanking) << "banking backend initialized for" << name << version;

        return {};
    }

    /**
     * The session, run in a thread of its own.
     *
     * Builds the orders here rather than taking them: which orders the account
     * carries is read out of the configuration of the backend, and that read
     * locks a group and reports a progress while it waits. Neither belongs in
     * the thread of a window.
     *
     * What leaves here are records of the core, which no longer touch the
     * banking backend.
     */
    SessionResult runSession(const std::shared_ptr<Account> &account, const QDate &latestStoredDate)
    {
        const quint32 uniqueAccountId = account->uniqueId();

        // The interface of gwenhywfar lives per thread. The one set where
        // initialize ran does not reach this thread, and without one here the
        // library would abort the process instead of reporting a failure. Held
        // for the length of the call, so that the slot of this thread is empty
        // again whichever way the session ends, an exception included; a thread
        // of the pool starts with it empty and is handed on that way.
        const ThreadGui gui(gwenGui);

        AB_ACCOUNT_SPEC *offered = nullptr;
        AB_Banking_GetAccountSpecByUniqueId(aqBanking, uniqueAccountId, &offered);

        const AccountSpecPtr held(offered, &AB_AccountSpec_free);

        SessionResult result;
        result.offersTransactions = Banking::accountOffers(offered,
                                                           AB_Transaction_CommandGetTransactions);

        if (!result.offersTransactions
            && !Banking::accountOffers(offered, AB_Transaction_CommandGetBalance)) {
            result.outcome = FetchOutcome::Received;
            result.offersNothing = true;

            return result;
        }

        const CommandListPtr commands(
            Banking::buildFetchCommands(*account, latestStoredDate, offered));
        const ContextPtr context(AB_ImExporterContext_new(), &AB_ImExporterContext_free);

        const int rv = AB_Banking_SendCommands(aqBanking, commands.get(), context.get());

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

    /**
     * The session over several accounts, run in a thread of its own.
     *
     * One list of orders and one call, which is what the backend expects: it
     * sorts the orders by account and by institution itself and runs one session
     * per institution. Two accounts of the same bank therefore share a session,
     * and what brings that session down brings down both.
     */
    AllSessionsResult runAllSessions(const QList<std::shared_ptr<Account>> &accounts,
                                     const QHash<quint32, QDate> &latestStoredDates)
    {
        const ThreadGui gui(gwenGui);

        AllSessionsResult result;

        const CommandListPtr commands(AB_Transaction_List2_new());

        for (const auto &account : accounts) {
            const quint32 uniqueAccountId = account->uniqueId();

            AB_ACCOUNT_SPEC *offered = nullptr;
            AB_Banking_GetAccountSpecByUniqueId(aqBanking, uniqueAccountId, &offered);

            const AccountSpecPtr held(offered, &AB_AccountSpec_free);

            const bool offersTransactions
                = Banking::accountOffers(offered, AB_Transaction_CommandGetTransactions);

            if (!offersTransactions
                && !Banking::accountOffers(offered, AB_Transaction_CommandGetBalance)) {
                result.nothingOffered.append(uniqueAccountId);
                continue;
            }

            if (!offersTransactions) {
                result.transactionsNotOffered.append(uniqueAccountId);
            }

            appendFetchCommands(commands.get(),
                                *account,
                                latestStoredDates.value(uniqueAccountId),
                                offered);

            result.sent.append(uniqueAccountId);
        }

        // Nothing to ask about. A call with an empty list would still open the
        // progress window of the library and sign on to nothing.
        if (result.sent.isEmpty()) {
            result.outcome = FetchOutcome::Received;

            return result;
        }

        const ContextPtr context(AB_ImExporterContext_new(), &AB_ImExporterContext_free);

        const int rv = AB_Banking_SendCommands(aqBanking, commands.get(), context.get());

        if (rv != AB_SUCCESS && rv != GWEN_ERROR_USER_ABORTED) {
            // Only the two sorting steps answer this way, and they run before
            // anything is sent. A single institution that failed does not: the
            // backend logs it, keeps what it collected and goes on to the next.
            result.outcome = FetchOutcome::Failed;
            result.reason = QStringLiteral("AB_Banking_SendCommands failed with %1").arg(rv);

            return result;
        }

        result.outcome = rv == GWEN_ERROR_USER_ABORTED ? FetchOutcome::Aborted
                                                       : FetchOutcome::Received;

        // Read even where the run was stopped. What the institutions before the
        // stop delivered is in the container, and whether it is kept is the
        // user's answer to give, not this one's.
        const QSet<quint32> refused = accountsOfFailedCommands(commands.get());

        for (const quint32 uniqueAccountId : std::as_const(result.sent)) {
            if (refused.contains(uniqueAccountId)) {
                result.failed.append(uniqueAccountId);
            }
        }

        result.items = Banking::itemsFromContext(context.get(), commands.get());

        return result;
    }

    /** The answer of a run over several accounts, in the thread of this object. */
    void deliverAllSessionsResult()
    {
        AllSessionsResult result;

        const QString failure = exceptionOf([this, &result] { result = allFetchWatcher.result(); });

        m_isFetching = false;

        if (!failure.isEmpty()) {
            const auto reason = QStringLiteral("A fetch over %1 accounts ended in %2")
                                    .arg(QString::number(m_fetchedAccounts), failure);

            qCCritical(lcBanking) << reason;

            Q_EMIT q_ptr->errorOccurred(ErrorCode::BankingFailure, reason);
            Q_EMIT q_ptr->finished();
            return;
        }

        // Said before the answer, because they decide how the answer reads. An
        // account that carries no order for the bookings brings a balance and
        // nothing else, and that is no account without new bookings.
        for (const quint32 uniqueAccountId : std::as_const(result.nothingOffered)) {
            qCInfo(lcBanking) << "account" << uniqueAccountId << "carries no order at all";

            Q_EMIT q_ptr->noOrderOffered(uniqueAccountId);
        }

        for (const quint32 uniqueAccountId : std::as_const(result.transactionsNotOffered)) {
            qCInfo(lcBanking) << "account" << uniqueAccountId
                              << "carries no order for transactions";

            Q_EMIT q_ptr->transactionsNotOffered(uniqueAccountId);
        }

        switch (result.outcome) {
        case FetchOutcome::Received:
            // One by one rather than as a failure of the run: the accounts
            // beside them went through, and the count of a collective fetch
            // needs them apart.
            for (const quint32 uniqueAccountId : std::as_const(result.failed)) {
                qCWarning(lcBanking) << "the bank refused an order of account" << uniqueAccountId;

                Q_EMIT q_ptr->accountFailed(uniqueAccountId);
            }

            qCDebug(lcBanking) << "fetched" << result.items.size() << "records";
            Q_EMIT q_ptr->itemsReceived(result.items);
            break;

        case FetchOutcome::Aborted:
            qCInfo(lcBanking) << "the fetch was aborted by the user";

            // What the institutions before the stop delivered still goes up.
            // Nothing of it is written yet, and the caller has to ask the user
            // whether it is kept at all.
            //
            // The orders that were cut off carry a failure of their own, and
            // they are not reported as refused accounts: they were stopped, not
            // turned down, and the two say something else to whoever is told.
            Q_EMIT q_ptr->itemsReceived(result.items);
            Q_EMIT q_ptr->aborted();
            break;

        case FetchOutcome::Failed:
            qCCritical(lcBanking) << result.reason;
            Q_EMIT q_ptr->errorOccurred(ErrorCode::BankingFailure, result.reason);
            break;
        }

        Q_EMIT q_ptr->finished();
    }

    /**
     * The accounts are copies of their own, for the reason startFetch names:
     * the accounts of the caller must not be reached into once fetchAccounts
     * has returned, and the session outlives that call.
     */
    void startFetchAll(const QList<std::shared_ptr<Account>> &accounts,
                       const QHash<quint32, QDate> &latestStoredDates)
    {
        m_isFetching = true;
        m_fetchedAccounts = accounts.size();

        allFetchWatcher.setFuture(QtConcurrent::run([this, accounts, latestStoredDates] {
            return runAllSessions(accounts, latestStoredDates);
        }));
    }

    /** The answer of a session, reported in the thread this object belongs to. */
    void deliverSessionResult()
    {
        SessionResult result;

        // The session runs in a thread of its own, and an exception it left
        // behind is held in the future until the result is asked for. It is
        // asked for here, in a slot, and an exception leaving a slot travels
        // into the event loop, which does not carry it. A session that never
        // returned an answer is reported as the failure it is.
        const QString failure = exceptionOf([this, &result] { result = fetchWatcher.result(); });

        if (!failure.isEmpty()) {
            m_isFetching = false;

            const auto reason = QStringLiteral("The session of account %1 ended in %2")
                                    .arg(QString::number(m_fetchedAccountId), failure);

            qCCritical(lcBanking) << reason;

            Q_EMIT q_ptr->errorOccurred(ErrorCode::BankingFailure, reason);
            Q_EMIT q_ptr->finished();
            return;
        }

        m_isFetching = false;

        // Said before the answer, because it decides how the answer reads. An
        // account the bank holds no order for the bookings of brings a balance
        // and nothing else, and that is no account without new bookings.
        if (result.offersNothing) {
            qCInfo(lcBanking) << "account" << m_fetchedAccountId << "carries no order at all";

            Q_EMIT q_ptr->noOrderOffered(m_fetchedAccountId);
            Q_EMIT q_ptr->finished();
            return;
        }

        if (!result.offersTransactions) {
            qCInfo(lcBanking) << "account" << m_fetchedAccountId
                              << "carries no order for transactions";

            Q_EMIT q_ptr->transactionsNotOffered(m_fetchedAccountId);
        }

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

    /**
     * The account is a copy of its own. The account of the caller must not be
     * reached into once fetchAccount has returned, and the session outlives
     * that call.
     */
    void startFetch(const std::shared_ptr<Account> &account, const QDate &latestStoredDate)
    {
        m_isFetching = true;
        m_fetchedAccountId = account->uniqueId();

        fetchWatcher.setFuture(QtConcurrent::run(
            [this, account, latestStoredDate] { return runSession(account, latestStoredDate); }));
    }

    void finalize()
    {
        // A session reaches into the banking backend from its own thread.
        // Pulling the backend away under it would leave it writing into freed
        // memory, so the shutdown waits for it. The window keeps this from
        // happening in the first place: it puts off closing while a fetch runs
        // and points at the progress dialog for stopping it.
        //
        // The wait hands on what the session threw. This one is reached from the
        // destructor, which is implicitly noexcept, so an exception passing
        // through here would end the process instead of reporting anything. What
        // follows must run in either case: it is the counterpart of every step
        // initialize reached.
        if (const QString failure = exceptionOf([this] { fetchWatcher.waitForFinished(); });
            !failure.isEmpty()) {
            qCCritical(lcBanking) << "the session ended in" << failure;
        }

        if (const QString failure = exceptionOf([this] { allFetchWatcher.waitForFinished(); });
            !failure.isEmpty()) {
            qCCritical(lcBanking) << "the session over several accounts ended in" << failure;
        }

        m_isFetching = false;

        // The teardown reaches into the interface of this thread, and the slot
        // holds one per thread rather than one per instance. Two instances may
        // be up at once, the wizard and the window, and the second one to come
        // up displaced the first. Whichever of them ends up going first, the
        // other must not have to rely on what the slot happens to hold: this one
        // puts its own back for the length of the shutdown.
        //
        // Without it AB_Banking_Fini below ends the process rather than
        // reporting anything. It takes a file lock, and the lock reads the flags
        // of the current interface without asking whether there is one.
        if (gwenGui != nullptr) {
            GWEN_Gui_SetGui(gwenGui);
        }

        // Every step is undone by the state it actually reached, one flag per
        // step, rather than by m_isInitialized alone: that one is set at the end
        // of initialize, so a failure before it would leave the backend, the
        // counterpart of GWEN_Init and the thread-global interface standing, and
        // the interface would then point at a GWEN_GUI its owner is free to
        // free.
        if (m_guiExtended) {
            AB_Gui_Unextend(gwenGui);
            m_guiExtended = false;
        }

        if (aqBanking != nullptr) {
            if (m_bankingInitialized) {
                const int rv = AB_Banking_Fini(aqBanking);
                if (rv != AB_SUCCESS) {
                    // Noted and freed anyway. Leaving the structure behind on a
                    // failed shutdown puts it out of reach for good, and the run
                    // is ending here whatever the backend says.
                    qCWarning(lcBanking) << "AB_Banking_Fini failed with" << rv;
                }

                m_bankingInitialized = false;
            }

            AB_Banking_free(aqBanking);
            aqBanking = nullptr;
        }

        if (m_chipCardClient != nullptr) {
            if (m_chipCardInitialized) {
                LC_Client_Fini(m_chipCardClient);
                m_chipCardInitialized = false;
            }

            LC_Client_free(m_chipCardClient);
            m_chipCardClient = nullptr;
        }

        // The interface is detached, not freed. It belongs to whoever passed it
        // to initialize, and freeing it here would release it a second time when
        // that owner goes.
        //
        // The slot is left empty rather than filled with what stood there
        // before. What stood there belongs to another instance, and that one is
        // free to go at any moment: a pointer put back here would outlive its
        // owner. Nobody is left without an interface by this, because every
        // instance sets its own at the top of this function.
        if (gwenGui != nullptr) {
            if (GWEN_Gui_GetGui() == gwenGui) {
                GWEN_Gui_SetGui(nullptr);
            }

            gwenGui = nullptr;
        }

        if (m_gwenInitialized) {
            GWEN_Fini();
            m_gwenInitialized = false;
        }

        m_isInitialized = false;
    }

    bool isInitialized() const { return m_isInitialized; }

    int setupAccounts() const
    {
        if (!isInitialized()) {
            return AB_ERROR;
        }

        // The header states the precondition, and a caller outside this project
        // can break it. The dialog walks the same AB_BANKING a running session
        // walks from the pool thread, and aqbanking takes no lock anywhere.
        if (isFetching()) {
            qCCritical(lcBanking) << "a fetch is running; the setup dialog cannot be opened "
                                     "meanwhile";

            return AB_ERROR;
        }

        // The dialog is run through the interface of this thread, and another
        // instance that shut down in the meantime left the slot empty.
        const ThreadGui gui(gwenGui);

        auto setupDialog = AB_Banking_CreateSetupDialog(aqBanking);
        if (setupDialog == nullptr) {
            qCCritical(lcBanking) << "the banking backend built no setup dialog";

            return AB_ERROR;
        }

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

    /**
     * The descriptions stay with the caller, which frees the list; this walks
     * over it and copies what it needs into records of the core.
     *
     * Walked as it stands rather than duplicated first: both list functions
     * take a const list, and a copy would be left behind on the way out that
     * finds no account.
     */
    BankingItems accounts(const AB_ACCOUNT_SPEC_LIST *list)
    {
        const auto totalAccounts = AB_AccountSpec_List_GetCount(list);

        if (totalAccounts == 0) {
            return {};
        }

        quint32 index = 0;
        auto accountList = BankingItems();

        auto accountSpec = AB_AccountSpec_List_First(list);
        while (accountSpec) {
            accountList.append(std::make_shared<Account>(accountSpec));
            accountSpec = AB_AccountSpec_List_Next(accountSpec);

            const auto percentage = index * 100.0 / totalAccounts;
            Q_EMIT q_ptr->progressValueChanged(percentage);

            ++index;
        }

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
    QFutureWatcher<AllSessionsResult> allFetchWatcher;

private:
    bool m_isFetching = false;
    quint32 m_fetchedAccountId = 0;
    int m_fetchedAccounts = 0;
    bool m_isInitialized;

    // One per step of initialize that has a counterpart in finalize. They are
    // what lets a failure halfway through be undone; m_isInitialized says only
    // that every step got through.
    bool m_gwenInitialized = false;
    bool m_bankingInitialized = false;
    bool m_guiExtended = false;
    bool m_chipCardInitialized = false;

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

    connect(&d_ptr->allFetchWatcher, &QFutureWatcherBase::finished, this, [this] {
        d_ptr->deliverAllSessionsResult();
    });
}

Banking::~Banking()
{
    // The watcher may still hold a queued end of a session for this object.
    // Cutting the connection before the private part goes keeps it from being
    // delivered into freed memory.
    disconnect(&d_ptr->fetchWatcher, nullptr, this, nullptr);
    disconnect(&d_ptr->allFetchWatcher, nullptr, this, nullptr);

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

    // The header states the precondition, and a caller outside this project can
    // break it: a session walks the same AB_BANKING from the pool thread, and
    // aqbanking takes no lock anywhere.
    if (d_ptr->isFetching()) {
        reportError(ErrorCode::Busy,
                    QStringLiteral("A fetch is running; the backend cannot be asked meanwhile"));
        return;
    }

    // Reading the account records takes a file lock, and the lock reads the
    // flags of the interface of this thread without asking whether there is
    // one. Another instance that shut down in the meantime left the slot empty.
    const ThreadGui gui(d_ptr->gwenGui);

    AB_ACCOUNT_SPEC_LIST *specList = nullptr;

    const int rv = AB_Banking_GetAccountSpecList(d_ptr->aqBanking, &specList);

    // A backend that holds no account answers with GWEN_ERROR_NOT_FOUND, which
    // is not a failure of the call. Reporting it as one would tell a user who
    // has not set up an account yet that their banking backend is broken.
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

bool Banking::accountOffers(const AB_ACCOUNT_SPEC *offered, AB_TRANSACTION_COMMAND command)
{
    if (offered == nullptr) {
        return true;
    }

    // A list that holds nothing is no statement. The field is documented as one
    // a backend may leave empty or incomplete, and reading its silence as a
    // refusal would stop a fetch the backend would have run.
    const AB_TRANSACTION_LIMITS_LIST *limits = AB_AccountSpec_GetTransactionLimitsList(offered);
    if (limits == nullptr || AB_TransactionLimits_List_GetCount(limits) == 0) {
        return true;
    }

    return AB_AccountSpec_GetTransactionLimitsForCommand(offered, command) != nullptr;
}

AB_TRANSACTION_LIST2 *Banking::buildFetchCommands(const Account &account,
                                                  const QDate &latestStoredDate,
                                                  const AB_ACCOUNT_SPEC *offered)
{
    AB_TRANSACTION_LIST2 *commands = AB_Transaction_List2_new();

    appendFetchCommands(commands, account, latestStoredDate, offered);

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

        // Booked entries only. A bank that sends the noted ones as well puts
        // them into the same list under a type of their own, and a noted entry
        // is not a booking: it may still fall away, and where it is booked the
        // next day it arrives with another date and another reference, so the
        // fingerprint differs and the amount would stand twice.
        //
        // The command stays open. It says which order brought the entry in and
        // does not tell a booking from a note.
        const AB_TRANSACTION *transaction
            = AB_ImExporterAccountInfo_GetFirstTransaction(accountInfo,
                                                           AB_Transaction_TypeStatement,
                                                           AB_Transaction_CommandNone);

        while (transaction != nullptr) {
            // The account comes from the entry. A statement that came over the
            // wire names none of its own: the importer of the backend fills the
            // fields of the booking and leaves that one empty. Only where the
            // entry carries none either is there nothing to store the booking
            // against, and it is dropped rather than written out of reach.
            auto booking = std::make_shared<Transaction>(uniqueAccountId, transaction);

            if (booking->uniqueAccountId() == 0) {
                qCWarning(lcBanking) << "a booking arrived without an account id and is dropped";
            } else {
                items.append(std::move(booking));
            }

            transaction = AB_Transaction_List_FindNextByType(transaction,
                                                             AB_Transaction_TypeStatement,
                                                             AB_Transaction_CommandNone);
        }

        if (const AB_BALANCE *balance = chooseBalance(
                AB_ImExporterAccountInfo_GetBalanceList(accountInfo));
            balance != nullptr) {
            if (uniqueAccountId == 0) {
                // A balance belongs to the entry it sits in, like a booking.
                // Without an id there is nothing to store it against.
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
    //
    // The library does not hand this up today: the backend turns every failure
    // of the sending into a generic one, and the layer above it drops even that
    // and answers success. Whoever needs to tell an abort apart asks the user
    // interface, which is where the wish arrives. The branch stays because it
    // is the right answer to the value, wherever it comes from.
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
    //
    // No interface is put into the slot of this thread for either read below.
    // Both walk the account record this object duplicated when it was built, and
    // a record of the backend is a plain structure: what wants an interface is
    // the file lock, and no path from here takes one.
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

    // A copy of its own, and that is what the session works on: the account of
    // the caller must not be reached into once this call has returned. The
    // orders are built inside the session, because which of them the account
    // carries is read out of the configuration of the backend, and that read
    // locks a group and reports a progress while it waits.
    d_ptr->startFetch(std::make_shared<Account>(account.accountSpec()), latestStoredDate);
}

void Banking::fetchAccounts(const QList<std::shared_ptr<Account>> &accounts,
                            const QHash<quint32, QDate> &latestStoredDates)
{
    const auto reportLater = [this](auto report) {
        QMetaObject::invokeMethod(this, std::move(report), Qt::QueuedConnection);
    };

    if (d_ptr->isFetching()) {
        const QString reason = QStringLiteral("A fetch is already running");

        qCWarning(lcBanking) << reason;

        // No finished here, for the reason fetchAccount gives: it belongs to the
        // fetch that is running.
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

    // An account without a backend brings down the whole call rather than
    // itself. It is therefore kept out of the list here, before anything is
    // sent, which is what lets the accounts beside it run at all.
    QList<std::shared_ptr<Account>> reachable;
    QList<quint32> skipped;

    for (const auto &account : accounts) {
        if (account == nullptr) {
            continue;
        }

        const QString backendName = account->backendName();

        if (backendName.isEmpty()
            || backendName.compare(offlineBackendName, Qt::CaseInsensitive) == 0) {
            qCInfo(lcBanking) << "account" << account->uniqueId() << "skipped, no online access";

            skipped.append(account->uniqueId());
            continue;
        }

        // A copy of its own, for the reason fetchAccount names: the account of
        // the caller must not be reached into once this call has returned.
        reachable.append(std::make_shared<Account>(account->accountSpec()));
    }

    if (!skipped.isEmpty()) {
        const QString reason = tr("The account has no online access");

        reportLater([this, skipped, reason] {
            for (const quint32 uniqueAccountId : skipped) {
                Q_EMIT accountSkipped(uniqueAccountId, reason);
            }
        });
    }

    // Nothing is left to ask about, so no session is started and the end is
    // reported from here. Without it the caller would wait for a finished that
    // no session is going to send.
    if (reachable.isEmpty()) {
        reportLater([this] { Q_EMIT finished(); });
        return;
    }

    d_ptr->startFetchAll(reachable, latestStoredDates);
}
