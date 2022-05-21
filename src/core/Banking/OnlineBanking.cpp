/**
* Copyright (c) 2022, Alexander Saal <alexander.saal@chm-projects.de>
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

#include <QtConcurrent/QtConcurrent>
#include <QtCore/QEventLoop>

#include <gwenhywfar/dialog.h>
#include <gwenhywfar/gui.h>
#include <gwenhywfar/gwenhywfar.h>

#include <gwen-gui-qt5/qt5_gui.hpp>
#include <gwen-gui-qt5/qt5dialogbox.hpp>

#include <aqbanking/banking.h>
#include <aqbanking/banking_dialogs.h>
#include <aqbanking/banking_imex.h>
#include <aqbanking/banking_online.h>

#include <aqbanking/error.h>
#include <aqbanking/gui/abgui.h>
#include <aqbanking/types/account_spec.h>

#ifdef USE_LIBCHIPCARD
#include <chipcard/client.h>
#endif

#ifndef AB_SUCCESS
#define AB_SUCCESS GWEN_SUCCESS
#endif

#ifndef AB_ERROR
#define AB_ERROR GWEN_ERROR_GENERIC
#endif

#include "core/SingleApplication/SingleApplication.h"
#include "core/Utils.h"

#include "OnlineBanking.h"

using namespace olbaflinx::core::banking;

class OnlineBanking::Private
{
public:
    struct l10n
    {
        static QString OnlineBankingNotInitialized()
        {
            return tr("The backend for online banking was not initialized!");
        }
        static QString OnlineBankingNoAccountWithId(quint32 id)
        {
            return tr("No account with the id %1 could be found!").arg(id);
        }
        static QString OnlineBankingAccountError()
        {
            return tr("An internal error occurred when retrieving the account data!");
        }
        static QString OnlineBankingNoAccountsFound() { return tr("No accounts could be found!"); }
        static QString OnlineBankingAccountNotValid()
        {
            return tr("The specified account is not valid!");
        }
        static QString OnlineBankingAccountTransactionLimitNotSupported()
        {
            return tr("The transaction limit for the specified account is not supported!");
        }
        static QString OnlineBankingFromDateHigherAsToDate()
        {
            return tr("The from date is higher than the current date!");
        }
        static QString OnlineBankingDateInvalid() { return tr("The from date is invalid!"); }
    };

    explicit Private()
        : m_qtGui(Q_NULLPTR)
        , m_gwenGui(Q_NULLPTR)
        , m_abBanking(Q_NULLPTR)
        , m_isInitialized(false)
    { }

    ~Private() = default;

    bool initBanking(const QString &name, const QString &key, const QString &version)
    {
        if (name.isEmpty() || key.isEmpty() || version.isEmpty()) {
            return false;
        }

        // We don't initialize AQ Banking & Gwen GUI twice
        m_isInitialized = (m_abBanking != nullptr) && (m_gwenGui != nullptr);
        if (m_isInitialized) {
            return false;
        }

        GWEN_Init();

        m_qtGui = new QT5_Gui();
        m_gwenGui = m_qtGui->getCInterface();
        GWEN_Gui_SetGui(m_gwenGui);

        const QByteArray local8BitName = name.toLocal8Bit();
        m_abBanking = AB_Banking_new(local8BitName.data(), nullptr, 0);

        const QByteArray local8BitKey = key.toLocal8Bit();
        AB_Banking_RuntimeConfig_SetCharValue(m_abBanking,
                                              "fintsRegistrationKey",
                                              local8BitKey.data());

        const QByteArray local8BitVersion = version.toLocal8Bit();
        AB_Banking_RuntimeConfig_SetCharValue(m_abBanking,
                                              "fintsApplicationVersionString",
                                              local8BitVersion.data());

        int rv = AB_Banking_Init(m_abBanking);
        if (rv != AB_SUCCESS) {
            return false;
        }

        AB_Gui_Extend(m_gwenGui, m_abBanking);

#ifdef USE_LIBCHIPCARD
        m_chipCardClient
            = LC_Client_new(SingleApplication::applicationName().toLocal8Bit().constData(),
                            SingleApplication::applicationVersion().toLocal8Bit().constData());

        LC_Client_Init(m_chipCardClient);
#endif

        m_isInitialized = ((m_abBanking != nullptr) && (m_gwenGui != nullptr));

        return m_isInitialized;
    }

    void finiBanking()
    {
        if (m_abBanking != nullptr) {
            int rv = AB_Banking_Fini(m_abBanking);
            if (rv == AB_SUCCESS) {
                AB_Gui_Unextend(m_gwenGui);
                AB_Banking_free(m_abBanking);
            }
        }

        if (m_gwenGui != nullptr) {
            GWEN_Gui_SetGui(nullptr);
            GWEN_Gui_free(m_gwenGui);
            GWEN_Fini();
        }

#ifdef USE_LIBCHIPCARD
        if (m_chipCardClient != nullptr) {
            LC_Client_Fini(m_chipCardClient);
            LC_Client_free(m_chipCardClient);
            m_chipCardClient = nullptr;
        }
#endif

        m_qtGui = nullptr;
        m_gwenGui = nullptr;
        m_abBanking = nullptr;

        m_isInitialized = false;
    }

    AB_BANKING *abBanking() const
    {
        return m_abBanking;
    }

    QT5_Gui *qtGui() const
    {
        return m_qtGui;
    }

    bool isInitialized() const
    {
        return m_isInitialized;
    }

    AB_IMEXPORTER_ACCOUNTINFO *createImExporterAccountInfo(AB_TRANSACTION_COMMAND cmd,
                                                           quint32 uniqueId,
                                                           GWEN_DATE *from,
                                                           GWEN_DATE *to) const
    {
        AB_TRANSACTION_LIST2 *transactionList = AB_Transaction_List2_new();
        AB_TRANSACTION *transaction = AB_Transaction_new();

        AB_Transaction_SetCommand(transaction, cmd);
        AB_Transaction_SetUniqueAccountId(transaction, uniqueId);

        if (from && to) {
            AB_Transaction_SetFirstDate(transaction, from);
            AB_Transaction_SetLastDate(transaction, to);
        } else if (to) {
            AB_Transaction_SetLastDate(transaction, to);
        }

        AB_Transaction_List2_PushBack(transactionList, transaction);

        AB_IMEXPORTER_CONTEXT *imExporterCtx = AB_ImExporterContext_new();
        AB_Banking_SendCommands(m_abBanking, transactionList, imExporterCtx);

        return AB_ImExporterContext_GetFirstAccountInfo(imExporterCtx);
    }

    TransactionList transactions(
        const AB_IMEXPORTER_ACCOUNTINFO *accountInfo,
        const AB_TRANSACTION_COMMAND type,
        const std::function<void(qreal)> &callback = [](qreal) {})
    {
        qApp->setOverrideCursor(Qt::WaitCursor);

        QEventLoop loop;
        QFutureWatcher<TransactionList> transactionWatcher;
        connect(&transactionWatcher, &QFutureWatcher<TransactionList>::finished, &loop, [&]() {
            loop.quit();
            transactionWatcher.cancel();
            transactionWatcher.waitForFinished();

            qApp->restoreOverrideCursor();
        });

        transactionWatcher.setFuture(QtConcurrent::run([accountInfo,
                                                        type,
                                                        callback]() -> TransactionList {
            TransactionList transactions = {};

            auto accountInfoDup = AB_ImExporterAccountInfo_dup(accountInfo);
            while (accountInfoDup) {
                int index = 0;
                auto abTransactionList = AB_ImExporterAccountInfo_GetTransactionList(accountInfoDup);
                int transactionCount = AB_Transaction_List_CountByType(abTransactionList, 0, type);
                if (abTransactionList) {
                    auto abTransaction = AB_Transaction_List_First(abTransactionList);
                    while (abTransaction) {
                        transactions.append(new Transaction(abTransaction));

                        const qreal percentage = index * 100.0 / transactionCount;
                        callback(percentage);
                        ++index;

                        abTransaction = AB_Transaction_List_Next(abTransaction);
                    }
                    AB_Transaction_List_free(abTransactionList);
                }
                accountInfoDup = AB_ImExporterAccountInfo_List_Next(accountInfoDup);
            }
            AB_ImExporterAccountInfo_free(accountInfoDup);

            return transactions;
        }));
        loop.exec();

        return transactionWatcher.result();
    }

    AccountList accounts(
        const AB_ACCOUNT_SPEC_LIST *accountSpecList,
        const std::function<void(qreal)> &callback = [](qreal) {})
    {
        qApp->setOverrideCursor(Qt::WaitCursor);

        QEventLoop loop;
        QFutureWatcher<AccountList> accountWatcher;
        connect(&accountWatcher, &QFutureWatcher<AccountList>::finished, &loop, [&]() {
            loop.quit();
            accountWatcher.cancel();
            accountWatcher.waitForFinished();

            qApp->restoreOverrideCursor();
        });

        accountWatcher.setFuture(QtConcurrent::run([accountSpecList, callback]() -> AccountList {
            const auto accSpecList = AB_AccountSpec_List_dup(accountSpecList);
            const auto totalAccounts = AB_AccountSpec_List_GetCount(accSpecList);

            if (totalAccounts == 0) {
                return {};
            }

            quint32 index = 0;
            AccountList accountList = {};

            auto accountSpec = AB_AccountSpec_List_First(accSpecList);
            while (accountSpec) {
                accountList.append(new Account(accountSpec));

                const qreal percentage = index * 100.0 / totalAccounts;
                callback(percentage);

                accountSpec = AB_AccountSpec_List_Next(accountSpec);
                ++index;
            }

            AB_AccountSpec_List_free(accSpecList);

            std::sort(accountList.begin(),
                      accountList.end(),
                      [](const Account *first, const Account *second) {
                          return first->accountName() < second->accountName();
                      });

            return accountList;
        }));
        loop.exec();

        return accountWatcher.result();
    }

    QString validateAccount(const Account *account, AB_TRANSACTION_COMMAND cmd) const
    {
        if (!isInitialized()) {
            return l10n::OnlineBankingNotInitialized();
        }

        const quint32 uniqueId = account->uniqueId();
        if (uniqueId <= 0 || !account->isValid()) {
            return l10n::OnlineBankingAccountNotValid();
        }

        if (!account->transactionLimitsForCommand(cmd)) {
            return l10n::OnlineBankingAccountTransactionLimitNotSupported();
        }

        return "";
    }

    QString validateDate(const QDate &from, const QDate &to) const
    {
        if (!from.isValid() || !to.isValid()) {
            return l10n::OnlineBankingDateInvalid();
        }

        if (from > to) {
            return l10n::OnlineBankingFromDateHigherAsToDate();
        }

        return "";
    }

private:
    GWEN_GUI *m_gwenGui;
    QT5_Gui *m_qtGui;
    AB_BANKING *m_abBanking;
    bool m_isInitialized;

#ifdef USE_LIBCHIPCARD
    LC_CLIENT *m_chipCardClient;
#endif
};

OnlineBanking::OnlineBanking()
    : QObject(Q_NULLPTR)
    , d_ptr(new Private)
{ }

OnlineBanking::~OnlineBanking() = default;

bool OnlineBanking::initialize(const QString &name, const QString &key, const QString &version)
{
    return d_ptr->initBanking(name, key, version);
}

void OnlineBanking::finalize()
{
    d_ptr->finiBanking();
}

int OnlineBanking::setupAccounts(const QWidget *widget)
{
    if (!d_ptr->isInitialized()) {
        Q_EMIT error(Private::l10n::OnlineBankingNotInitialized());
        return AB_ERROR;
    }

    const auto parentWidget = d_ptr->qtGui()->getParentWidget();
    if (parentWidget == Q_NULLPTR && widget != Q_NULLPTR) {
        d_ptr->qtGui()->pushParentWidget(const_cast<QWidget *>(widget));
    } else if (parentWidget != Q_NULLPTR && widget != Q_NULLPTR) {
        d_ptr->qtGui()->popParentWidget();
        d_ptr->qtGui()->pushParentWidget(const_cast<QWidget *>(widget));
    }

    auto setupDialog = AB_Banking_CreateSetupDialog(d_ptr->abBanking());
    auto result = GWEN_Gui_ExecDialog(setupDialog, 0);
    GWEN_Dialog_free(setupDialog);

    return result;
}

ImExportProfileList OnlineBanking::importExportProfiles(bool import)
{
    if (!d_ptr->isInitialized()) {
        Q_EMIT error(Private::l10n::OnlineBankingNotInitialized());
        return {};
    }

    auto pluginList = AB_Banking_GetImExporterDescrs(d_ptr->abBanking());

    qApp->setOverrideCursor(Qt::WaitCursor);

    QEventLoop loop;
    QFutureWatcher<ImExportProfileList> imExporterWatcher;
    connect(&imExporterWatcher, &QFutureWatcher<ImExportProfileList>::finished, &loop, [&]() {
        loop.quit();
        imExporterWatcher.cancel();
        imExporterWatcher.waitForFinished();

        qApp->restoreOverrideCursor();
    });

    imExporterWatcher.setFuture(QtConcurrent::run([&, pluginList]() -> ImExportProfileList {
        ImExportProfileList imExportProfiles = {};

        auto pluginListDup = GWEN_PluginDescription_List2_dup(pluginList);
        auto pluginListIter = GWEN_PluginDescription_List2_First(pluginListDup);
        auto pluginListCount = GWEN_PluginDescription_List2_GetSize(pluginListDup);

        if (pluginListIter) {
            auto gpDescr = GWEN_PluginDescription_List2Iterator_Data(pluginListIter);
            int index = 0;
            while (gpDescr) {
                auto iep = new ImExportProfile();

                iep->name = GWEN_PluginDescription_GetName(gpDescr);
                iep->type = GWEN_PluginDescription_GetType(gpDescr);
                iep->shortDescr = GWEN_PluginDescription_GetShortDescr(gpDescr);
                iep->author = GWEN_PluginDescription_GetAuthor(gpDescr);
                iep->version = GWEN_PluginDescription_GetVersion(gpDescr);
                iep->longDescr = GWEN_PluginDescription_GetLongDescr(gpDescr);
                iep->fileName = GWEN_PluginDescription_GetFileName(gpDescr);

                auto profiles = AB_Banking_GetImExporterProfiles(d_ptr->abBanking(),
                                                                 iep->name.toLatin1().constData());
                if (profiles) {
                    auto profileGroups = GWEN_DB_GetFirstGroup(profiles);
                    while (profileGroups) {
                        auto iepd = new ImExportProfileData();

                        iepd->name = GWEN_DB_GetCharValue(profileGroups, "name", 0, "");
                        iepd->longDescr = GWEN_DB_GetCharValue(profileGroups, "longDescr", 0, "");
                        iepd->shortDescr = GWEN_DB_GetCharValue(profileGroups, "shortDescr", 0, "");
                        iepd->type = GWEN_DB_GetCharValue(profileGroups, "type", 0, "");
                        iepd->exp = GWEN_DB_GetIntValue(profileGroups, "export", 0, 0);
                        iepd->import = GWEN_DB_GetIntValue(profileGroups, "import", 0, 0);

                        if (import && iepd->import == 1) {
                            iep->profiles.append(iepd);
                        } else if (!import && iepd->exp == 1) {
                            iep->profiles.append(iepd);
                        }

                        profileGroups = GWEN_DB_GetNextGroup(profileGroups);
                    }
                }

                if (!iep->profiles.isEmpty()) {
                    imExportProfiles.append(iep);
                }

                const qreal percentage = index * 100.0 / pluginListCount;
                Q_EMIT progress(percentage);
                ++index;

                gpDescr = GWEN_PluginDescription_List2Iterator_Next(pluginListIter);
            }

            GWEN_PluginDescription_List2Iterator_free(pluginListIter);
        }

        GWEN_PluginDescription_List2_freeAll(pluginListDup);

        return imExportProfiles;
    }));
    loop.exec();

    GWEN_PluginDescription_List2_freeAll(pluginList);

    return imExporterWatcher.result();
}

Account *OnlineBanking::account(quint32 uniqueId)
{
    if (!d_ptr->isInitialized()) {
        Q_EMIT error(Private::l10n::OnlineBankingNotInitialized());
        return Q_NULLPTR;
    }

    AB_ACCOUNT_SPEC *accountSpec = Q_NULLPTR;

    int rv = AB_Banking_GetAccountSpecByUniqueId(d_ptr->abBanking(), uniqueId, &accountSpec);
    if (rv != AB_SUCCESS) {
        Q_EMIT error(Private::l10n::OnlineBankingNoAccountWithId(uniqueId));
        return Q_NULLPTR;
    }

    const auto account = new Account(accountSpec);

    AB_AccountSpec_free(accountSpec);

    return account;
}

AccountIds OnlineBanking::accountIds()
{
    auto accountList = accounts();
    if (accountList.isEmpty()) {
        Q_EMIT error(Private::l10n::OnlineBankingNoAccountsFound());
        return {};
    }

    AccountIds accountIds = {};
    for (const auto account : qAsConst(accountList)) {
        accountIds << account->uniqueId();
    }

    qDeleteAll(accountList);
    accountList.clear();

    return accountIds;
}

AccountList OnlineBanking::accounts()
{
    if (!d_ptr->isInitialized()) {
        Q_EMIT error(Private::l10n::OnlineBankingNotInitialized());
        return {};
    }

    AB_ACCOUNT_SPEC_LIST *accountSpecList = Q_NULLPTR;

    int rv = AB_Banking_GetAccountSpecList(d_ptr->abBanking(), &accountSpecList);
    if (rv != AB_SUCCESS) {
        Q_EMIT error(Private::l10n::OnlineBankingAccountError());
        return {};
    }

    auto accounts = d_ptr->accounts(accountSpecList, [&](qreal value) { Q_EMIT progress(value); });

    AB_AccountSpec_List_free(accountSpecList);

    if (accounts.isEmpty()) {
        Q_EMIT error(Private::l10n::OnlineBankingNoAccountsFound());
        return {};
    }

    return accounts;
}

AccountBalanceList OnlineBanking::accountBalances(const Account *account,
                                                  const QDate &from,
                                                  const QDate &to)
{
    if (!d_ptr->isInitialized()) {
        Q_EMIT error(Private::l10n::OnlineBankingNotInitialized());
        return {};
    }

    auto result = d_ptr->validateAccount(account, AB_Transaction_CommandGetBalance);
    if (!result.isEmpty()) {
        Q_EMIT error(result);
        return {};
    }

    result = d_ptr->validateDate(from, to);
    if (!result.isEmpty()) {
        Q_EMIT error(result);
        return {};
    }

    GWEN_DATE *gwenFromDate = Utils::qDateToGwenDate(from);
    GWEN_DATE *gwenToDate = Utils::qDateToGwenDate(to);

    auto accountInfo = d_ptr->createImExporterAccountInfo(AB_Transaction_CommandGetBalance,
                                                          account->uniqueId(),
                                                          gwenFromDate,
                                                          gwenToDate);

    GWEN_Date_free(gwenFromDate);
    GWEN_Date_free(gwenToDate);

    qApp->setOverrideCursor(Qt::WaitCursor);

    QEventLoop loop;
    QFutureWatcher<AccountBalanceList> accountBalanceWatcher;
    connect(&accountBalanceWatcher, &QFutureWatcher<AccountBalanceList>::finished, &loop, [&]() {
        loop.quit();
        accountBalanceWatcher.cancel();
        accountBalanceWatcher.waitForFinished();

        qApp->restoreOverrideCursor();
    });

    accountBalanceWatcher.setFuture(QtConcurrent::run([&, accountInfo]() -> AccountBalanceList {
        AccountBalanceList balanceList = {};

        auto accountInfoDup = AB_ImExporterAccountInfo_dup(accountInfo);
        while (accountInfoDup) {
            auto accountBalanceList = AB_ImExporterAccountInfo_GetBalanceList(accountInfoDup);
            if (accountBalanceList) {
                int index = 0;
                auto balanceCount = AB_Balance_List_GetCount(accountBalanceList);
                auto balances = AB_Balance_List_First(accountBalanceList);
                if (balances) {
                    while (balances) {
                        balanceList.append(new AccountBalance(balances));

                        const qreal percentage = index * 100.0 / balanceCount;
                        Q_EMIT progress(percentage);
                        ++index;

                        balances = AB_Balance_List_Next(balances);
                    }
                }
                AB_Balance_List_free(accountBalanceList);
            }
            accountInfoDup = AB_ImExporterAccountInfo_List_Next(accountInfoDup);
        }
        AB_ImExporterAccountInfo_free(accountInfoDup);

        return balanceList;
    }));
    loop.exec();

    AB_ImExporterAccountInfo_free(accountInfo);

    return accountBalanceWatcher.result();
}

TransactionList OnlineBanking::transactions(const Account *account,
                                            const QDate &from,
                                            const QDate &to,
                                            const TransactionListType &type)
{
    if (!d_ptr->isInitialized()) {
        Q_EMIT error(Private::l10n::OnlineBankingNotInitialized());
        return {};
    }

    auto result = d_ptr->validateAccount(account, (AB_TRANSACTION_COMMAND) type);
    if (!result.isEmpty()) {
        Q_EMIT error(result);
        return {};
    }

    result = d_ptr->validateDate(from, to);
    if (!result.isEmpty()) {
        Q_EMIT error(result);
        return {};
    }

    GWEN_DATE *gwenFromDate = Utils::qDateToGwenDate(from);
    GWEN_DATE *gwenToDate = Utils::qDateToGwenDate(to);

    auto accountInfo = d_ptr->createImExporterAccountInfo((AB_TRANSACTION_COMMAND) type,
                                                          account->uniqueId(),
                                                          gwenFromDate,
                                                          gwenToDate);

    GWEN_Date_free(gwenFromDate);
    GWEN_Date_free(gwenToDate);

    auto transactionList = d_ptr->transactions(accountInfo,
                                               (AB_TRANSACTION_COMMAND) type,
                                               [&](qreal value) -> void { Q_EMIT progress(value); });

    AB_ImExporterAccountInfo_free(accountInfo);

    return transactionList;
}

TransactionList banking::OnlineBanking::importTransactionsFromFile(const QString &importerName,
                                                                   const QString &profileName,
                                                                   const QString &fileName)
{
    const auto dbProfile = AB_Banking_GetImExporterProfile(d_ptr->abBanking(),
                                                           importerName.toLatin1().constData(),
                                                           profileName.toLatin1().constData());
    if (dbProfile == Q_NULLPTR) {
        return {};
    }

    const auto imExporterCtx = AB_ImExporterContext_new();
    const int result = AB_Banking_ImportFromFile(d_ptr->abBanking(),
                                                 importerName.toLatin1().constData(),
                                                 imExporterCtx,
                                                 fileName.toLatin1().constData(),
                                                 dbProfile);
    if (result != AB_SUCCESS) {
        return {};
    }

    const auto accountInfo = AB_ImExporterContext_GetFirstAccountInfo(imExporterCtx);
    const auto transactionList = d_ptr->transactions(accountInfo,
                                                     AB_Transaction_CommandGetTransactions,
                                                     [&](qreal value) -> void {
                                                         Q_EMIT progress(value);
                                                     });

    AB_ImExporterAccountInfo_free(accountInfo);
    AB_ImExporterContext_free(imExporterCtx);

    GWEN_DB_Group_free(dbProfile);

    return transactionList;
}
