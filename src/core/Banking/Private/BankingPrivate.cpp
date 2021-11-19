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

#include <gwenhywfar/gwenhywfar.h>
#include <gwenhywfar/gui.h>
#include <gwenhywfar/dialog.h>
#include <gwen-gui-qt5/qt5_gui.hpp>

#include <aqbanking/gui/abgui.h>
#include <aqbanking/types/account_spec.h>
#include <aqbanking/banking.h>
#include <aqbanking/banking_dialogs.h>
#include <aqbanking/banking_online.h>
#include <aqbanking/error.h>

#include "Banking/Banking.h"
#include "BankingPrivate.h"

#include "core/SingleApplication/SingleApplication.h"

#ifndef AB_SUCCESS
#define AB_SUCCESS GWEN_SUCCESS
#endif

using namespace olbaflinx::core::banking;
using namespace olbaflinx::core::storage::account;

GWEN_GUI *gwenGui;

AB_BANKING *abBanking;

BankingPrivate::BankingPrivate(Banking *banking)
    : QObject(nullptr),
      q_ptr(banking),
      mIsInitialized(false)
{
    gwenGui = nullptr;
    abBanking = nullptr;
}

BankingPrivate::~BankingPrivate()
{
    finalizeAqBanking();
}

bool BankingPrivate::initializeAqBanking(
    const QString &name,
    const QString &key,
    const QString &version
)
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

    auto qtGui = new QT5_Gui();
    gwenGui = qtGui->getCInterface();
    GWEN_Gui_SetGui(gwenGui);

    const QByteArray local8BitName = name.toLocal8Bit();
    abBanking = AB_Banking_new(local8BitName.data(), nullptr, 0);

    const char *fintsRegistrationKey = AB_Banking_RuntimeConfig_GetCharValue(
        abBanking,
        "fintsRegistrationKey",
        nullptr
    );

    if (fintsRegistrationKey == nullptr) {
        const QByteArray local8BitKey = key.toLocal8Bit();
        AB_Banking_RuntimeConfig_SetCharValue(
            abBanking,
            "fintsRegistrationKey",
            local8BitKey.data()
        );
    }

    const QByteArray local8BitVersion = version.toLocal8Bit();
    AB_Banking_RuntimeConfig_SetCharValue(
        abBanking,
        "fintsApplicationVersionString",
        local8BitVersion.data()
    );

    int rv = AB_Banking_Init(abBanking);
    if (rv != AB_SUCCESS) {
        return false;
    }

    AB_Gui_Extend(gwenGui, abBanking);

    mIsInitialized = ((abBanking != nullptr) && (gwenGui != nullptr));

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
    if (rv == AB_SUCCESS) {

        AB_ACCOUNT_SPEC *accountSpec;
        while ((accountSpec = AB_AccountSpec_List_First(accountSpecList))) {
            AB_AccountSpec_List_Del(accountSpec);
            accountList.push_back(new Account(accountSpec));
        }

        AB_AccountSpec_List_free(accountSpecList);

        std::sort(
            accountList.begin(),
            accountList.end(),
            [=](const Account *first, const Account *second)
            {
                return first->accountName() < second->accountName();
            }
        );

        Q_EMIT q_ptr->accountsReceived(accountList);

        if (!accountList.empty()) {
            qDeleteAll(accountList);
            accountList.clear();
        }
    }
}

void BankingPrivate::receiveAccountIds()
{
    disconnect(q_ptr, &Banking::accountsReceived, nullptr, nullptr);
    connect(
        q_ptr,
        &Banking::accountsReceived,
        [=](const AccountList &accounts)
        {
            if (accounts.empty()) {
                Q_EMIT q_ptr->accountIdsReceived(AccountIds());
                return;
            }

            AccountIds accountIds = AccountIds();

            for (const auto account: qAsConst(accounts)) {
                accountIds.push_back(account->uniqueId());
            }

            std::sort(accountIds.begin(), accountIds.end());

            Q_EMIT q_ptr->accountIdsReceived(accountIds);

            accountIds.clear();
        }
    );

    receiveAccounts();
}

bool BankingPrivate::createAccount() const
{
    if (!mIsInitialized) {
        return false;
    }

    auto dialog = AB_Banking_CreateSetupDialog(abBanking);
    int dlgResult = GWEN_Gui_ExecDialog(dialog, 0);
    GWEN_Dialog_free(dialog);

    return dlgResult == 1;
}

