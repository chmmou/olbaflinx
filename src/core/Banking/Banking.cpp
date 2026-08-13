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

    void finalize()
    {
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

private:
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
}

Banking::~Banking()
{
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

AB_TRANSACTION_LIST2 *Banking::buildFetchCommands(const Account &account, const QDate &firstDate)
{
    const auto addCommand = [&account, &firstDate](AB_TRANSACTION_LIST2 *list,
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
        if (const GwenDatePtr first = fromDate(firstDate); first) {
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

void Banking::fetchAccount(const Account &account, const QDate &firstDate)
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

    // An account without a backend brings down the whole run rather than itself:
    // AqBanking sorts the queues by backend before it sends anything and answers
    // GWEN_ERROR_BAD_DATA for an account that carries none. Passing such an
    // account over is therefore what keeps the accounts beside it running.
    const QString backendName = account.backendName();
    if (backendName.isEmpty() || backendName.compare(offlineBackendName, Qt::CaseInsensitive) == 0) {
        const QString reason = tr("The account has no online access");

        qCInfo(lcBanking) << "account" << account.uniqueId() << "skipped, no online access";

        Q_EMIT accountSkipped(account.uniqueId(), reason);
        Q_EMIT finished();
        return;
    }

    const CommandListPtr commands(buildFetchCommands(account, firstDate));
    const ContextPtr context(AB_ImExporterContext_new(), &AB_ImExporterContext_free);

    const int rv = AB_Banking_SendCommands(d_ptr->aqBanking, commands.get(), context.get());
    if (rv != AB_SUCCESS) {
        reportError(ErrorCode::BankingFailure,
                    QStringLiteral("AB_Banking_SendCommands failed with %1").arg(rv));
        return;
    }

    // A session can come back successful and still carry an order the bank
    // refused. Without this the account would answer with an empty list, and a
    // refusal would read like an account with nothing new.
    if (accountsOfFailedCommands(commands.get()).contains(account.uniqueId())) {
        reportError(ErrorCode::BankingFailure,
                    QStringLiteral("An order of account %1 was refused").arg(account.uniqueId()));
        return;
    }

    const BankingItems items = itemsFromContext(context.get(), commands.get());

    qCDebug(lcBanking) << "fetched" << items.size() << "records for account" << account.uniqueId();

    // An account with nothing new reports an empty list. That is not an error,
    // and it is where a fetch parts from a read of the storage.
    Q_EMIT itemsReceived(items);

    Q_EMIT finished();
}
