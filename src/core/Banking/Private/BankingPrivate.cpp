/**
 * Copyright (c) 2021, Alexander Saal <alexander.saal@chm-projects.de>
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
#include <QtWidgets/QPushButton>

#include <gwenhywfar/dialog.h>
#include <gwenhywfar/gui.h>
#include <gwenhywfar/gwenhywfar.h>

#include <gwen-gui-qt5/qt5_gui.hpp>
#include <gwen-gui-qt5/qt5_gui_dialog.hpp>
#include <gwen-gui-qt5/qt5dialogbox.hpp>

#include <aqbanking/banking.h>
#include <aqbanking/banking_dialogs.h>
#include <aqbanking/banking_online.h>
#include <aqbanking/error.h>
#include <aqbanking/gui/abgui.h>
#include <aqbanking/types/account_spec.h>

#ifdef USE_LIBCHIPCARD
#include <chipcard/client.h>
#endif

#include "Banking/Banking.h"
#include "BankingPrivate.h"

#include "core/SingleApplication/SingleApplication.h"

#ifndef AB_SUCCESS
#define AB_SUCCESS GWEN_SUCCESS
#endif

using namespace olbaflinx::core::banking;
using namespace olbaflinx::core::storage::account;

GWEN_GUI *gwenGui;
QT5_Gui *qtGui;
AB_BANKING *abBanking;

#ifdef USE_LIBCHIPCARD
LC_CLIENT *m_chipCardClient;
#endif

BankingPrivate::BankingPrivate(Banking *banking)
    : QObject(nullptr)
    , q_ptr(banking)
    , mIsInitialized(false)
{
    qtGui = nullptr;
    gwenGui = nullptr;
    abBanking = nullptr;
}

BankingPrivate::~BankingPrivate()
{
    finalizeAqBanking();
}

bool BankingPrivate::initializeAqBanking(const QString &name,
                                         const QString &key,
                                         const QString &version)
{
    if (name.isEmpty() || key.isEmpty() || version.isEmpty()) {
        return false;
    }

    // We don't initialize AQ Banking & Gwen GUI twice
    mIsInitialized = (abBanking != nullptr) && (gwenGui != nullptr);
    if (mIsInitialized) {
        return false;
    }

    GWEN_Init();

    qtGui = new QT5_Gui();
    gwenGui = qtGui->getCInterface();
    GWEN_Gui_SetGui(gwenGui);

    const QByteArray local8BitName = name.toLocal8Bit();
    abBanking = AB_Banking_new(local8BitName.data(), nullptr, 0);

    const QByteArray local8BitKey = key.toLocal8Bit();
    AB_Banking_RuntimeConfig_SetCharValue(abBanking, "fintsRegistrationKey", local8BitKey.data());

    const QByteArray local8BitVersion = version.toLocal8Bit();
    AB_Banking_RuntimeConfig_SetCharValue(abBanking,
                                          "fintsApplicationVersionString",
                                          local8BitVersion.data());

    int rv = AB_Banking_Init(abBanking);
    if (rv != AB_SUCCESS) {
        return false;
    }

    AB_Gui_Extend(gwenGui, abBanking);

    mIsInitialized = ((abBanking != nullptr) && (gwenGui != nullptr));

#ifdef USE_LIBCHIPCARD
    m_chipCardClient
        = LC_Client_new(SingleApplication::applicationName().toLocal8Bit().constData(),
                        SingleApplication::applicationVersion().toLocal8Bit().constData());

    LC_Client_Init(m_chipCardClient);
#endif

    return mIsInitialized;
}

void BankingPrivate::finalizeAqBanking()
{
    if (abBanking != nullptr) {
        int rv = AB_Banking_Fini(abBanking);
        if (rv == AB_SUCCESS) {
            AB_Gui_Unextend(gwenGui);
            AB_Banking_free(abBanking);
        }
    }

    if (gwenGui != nullptr) {
        GWEN_Gui_SetGui(nullptr);
        GWEN_Gui_free(gwenGui);
        GWEN_Fini();
    }

#ifdef USE_LIBCHIPCARD
    if (m_chipCardClient != nullptr) {
        LC_Client_Fini(m_chipCardClient);
        LC_Client_free(m_chipCardClient);
        m_chipCardClient = nullptr;
    }
#endif

    qtGui = nullptr;
    gwenGui = nullptr;
    abBanking = nullptr;

    mIsInitialized = false;
}

Account *BankingPrivate::account(quint32 uniqueId) const
{
    if (!mIsInitialized) {
        return nullptr;
    }

    AB_ACCOUNT_SPEC *accountSpec = nullptr;

    int rv = AB_Banking_GetAccountSpecByUniqueId(abBanking, uniqueId, &accountSpec);
    if (rv != AB_SUCCESS) {
        return nullptr;
    }

    const auto account = new Account(accountSpec);

    AB_AccountSpec_free(accountSpec);
    accountSpec = nullptr;

    return account;
}

void BankingPrivate::receiveAccounts()
{
    AccountList accountList = AccountList();

    if (!mIsInitialized) {
        Q_EMIT q_ptr->accountsReceived(accountList);
        return;
    }

    AB_ACCOUNT_SPEC_LIST *accountSpecList = nullptr;

    int rv = AB_Banking_GetAccountSpecList(abBanking, &accountSpecList);
    if (rv != AB_SUCCESS) {
        Q_EMIT q_ptr->accountsReceived(accountList);
        return;
    }

    quint32 totalAccounts = AB_AccountSpec_List_GetCount(accountSpecList);
    if (totalAccounts == 0) {
        Q_EMIT q_ptr->accountsReceived(accountList);
        return;
    }

    quint32 currentAccount = 0;
    AB_ACCOUNT_SPEC *accountSpec;
    while ((accountSpec = AB_AccountSpec_List_First(accountSpecList))) {
        AB_AccountSpec_List_Del(accountSpec);
        accountList.append(new Account(accountSpec));

        currentAccount = accountList.size() / totalAccounts * 100;
        Q_EMIT q_ptr->progressStatus(currentAccount, totalAccounts);
    }

    AB_AccountSpec_List_free(accountSpecList);

    std::sort(accountList.begin(),
              accountList.end(),
              [=](const Account *first, const Account *second) {
                  return first->accountName() < second->accountName();
              });

    Q_EMIT q_ptr->accountsReceived(accountList);

    if (!accountList.empty()) {
        qDeleteAll(accountList);
        accountList.clear();
    }
}

void BankingPrivate::receiveAccountIds()
{
    disconnect(q_ptr, &Banking::accountsReceived, nullptr, nullptr);
    connect(q_ptr, &Banking::accountsReceived, [=](const AccountList &accounts) {
        if (accounts.empty()) {
            Q_EMIT q_ptr->accountIdsReceived(AccountIds());
            return;
        }

        AccountIds accountIds = AccountIds();

        for (const auto account : qAsConst(accounts)) {
            accountIds.append(account->uniqueId());
        }

        std::sort(accountIds.begin(), accountIds.end());

        Q_EMIT q_ptr->accountIdsReceived(accountIds);

        accountIds.clear();
    });

    receiveAccounts();
}

void BankingPrivate::receiveTransactions(const Account *account, const QDate &from, const QDate &to)
{
    TransactionList transactionList = TransactionList();

    if (!mIsInitialized) {
        Q_EMIT q_ptr->transactionsReceived(transactionList);
        return;
    }

    const quint32 uniqueId = account->uniqueId();
    if (uniqueId <= 0 || !account->isValid()) {
        Q_EMIT q_ptr
            ->errorOccurred(tr("The given account id is invalid and or the account is invalid"),
                            Banking::AccountError);
        return;
    }

    if (!account->transactionLimitsForCommand(AB_Transaction_CommandGetTransactions)) {
        Q_EMIT q_ptr
            ->errorOccurred(tr("The transaction limit for the specified account is not supported!"),
                            Banking::TransactionError);
        return;
    }

    if (from > to) {
        Q_EMIT q_ptr->errorOccurred(tr("The from date is higher than the current date!"),
                                    Banking::TransactionError);
        return;
    }

    const auto fd = from.toString(QString(GwenDateFormat)).toLocal8Bit();
    const auto cd = to.toString(QString(GwenDateFormat)).toLocal8Bit();

    GWEN_DATE *gwenFromDate = GWEN_Date_fromString(fd.constData());
    GWEN_DATE *gwenToDate = GWEN_Date_fromString(cd.constData());

    AB_IMEXPORTER_ACCOUNTINFO *accountInfo
        = abImExporterAccountInfo(AB_Transaction_CommandGetTransactions,
                                  uniqueId,
                                  gwenFromDate,
                                  gwenToDate);

    QString balance = QString();

    while (accountInfo) {
        AB_BALANCE_LIST *balanceList = AB_ImExporterAccountInfo_GetBalanceList(accountInfo);
        AB_BALANCE *abBalance = AB_Balance_List_GetLatestByType(balanceList, AB_Balance_TypeBooked);
        if (abBalance) {
            auto balanceValue = AB_Balance_GetValue(abBalance);
            GWEN_BUFFER *valueBuffer = GWEN_Buffer_new(nullptr, 256, 0, 1);
            AB_Value_toHumanReadableString(balanceValue, valueBuffer, 2, 0);
            balance = QString::fromUtf8(GWEN_Buffer_GetStart(valueBuffer));
            GWEN_Buffer_Reset(valueBuffer);
        }
        AB_Balance_List_free(balanceList);

        AB_TRANSACTION_LIST *abList = AB_ImExporterAccountInfo_GetTransactionList(accountInfo);
        if (abList) {
            AB_TRANSACTION *abTransaction;
            while ((abTransaction = AB_Transaction_List_First(abList))) {
                AB_Transaction_List_Del(abTransaction);
                transactionList.append(new Transaction(abTransaction));
            }

            AB_Transaction_List_free(abList);
        }

        accountInfo = AB_ImExporterAccountInfo_List_Next(accountInfo);
    }

    GWEN_Date_free(gwenFromDate);
    GWEN_Date_free(gwenToDate);
    AB_ImExporterAccountInfo_free(accountInfo);

    Q_EMIT q_ptr->transactionsReceived(transactionList, balance);

    qDeleteAll(transactionList);
    transactionList.clear();
    balance.clear();
}

int BankingPrivate::showSetupDialog(QWidget *parentWidget) const
{
    if (!mIsInitialized) {
        return AB_ERROR_NOT_INIT;
    }

    const auto qt5GuiParentWidget = qtGui->getParentWidget();
    if (qt5GuiParentWidget == nullptr && parentWidget != nullptr) {
        qtGui->pushParentWidget(parentWidget);
    } else if (qt5GuiParentWidget != nullptr) {
        qtGui->popParentWidget();
    }

    auto setupDialog = AB_Banking_CreateSetupDialog(abBanking);
    int result = GWEN_Gui_ExecDialog(setupDialog, 0);
    GWEN_Dialog_free(setupDialog);

    return result;
}

AB_IMEXPORTER_ACCOUNTINFO *BankingPrivate::abImExporterAccountInfo(AB_TRANSACTION_COMMAND cmd,
                                                                   quint32 uniqueId,
                                                                   GWEN_DATE *from,
                                                                   GWEN_DATE *to)
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
    AB_Banking_SendCommands(abBanking, transactionList, imExporterCtx);

    return AB_ImExporterContext_GetFirstAccountInfo(imExporterCtx);
}
