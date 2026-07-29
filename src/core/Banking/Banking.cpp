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

#include <chipcard/client.h>

#include <gwenhywfar/dialog.h>
#include <gwenhywfar/gui.h>
#include <gwenhywfar/gwenhywfar.h>

#include <gwen-gui-qt5/qt5_gui.hpp>

#include <aqbanking/banking.h>
#include <aqbanking/banking_dialogs.h>
#include <aqbanking/banking_online.h>

#include <aqbanking/error.h>
#include <aqbanking/gui/abgui.h>
#include <aqbanking/types/account_spec.h>

#include <utility>

#ifndef AB_SUCCESS
#define AB_SUCCESS GWEN_SUCCESS
#endif

#ifndef AB_ERROR
#define AB_ERROR GWEN_ERROR_GENERIC
#endif

using namespace olbaflinx::core::banking;

class Banking::Private
{
public:
    explicit Private(Banking *banking, ApplicationInfo applicationInfo)
        : gwenGui(nullptr)
        , qtGui(nullptr)
        , aqBanking(nullptr)
        , m_isInitialized(false)
        , m_chipCardClient(nullptr)
        , m_applicationInfo(std::move(applicationInfo))
        , q_ptr(banking)
    {}

    ~Private() { finalize(); }

    bool initialize(const QString &name, const QString &version, const QString &key)
    {
        if (name.isEmpty() || version.isEmpty()) {
            return false;
        }

        // We don't initialize AQ Banking & Gwen GUI twice
        m_isInitialized = (aqBanking != nullptr) && (gwenGui != nullptr);
        if (m_isInitialized) {
            return false;
        }

        int rv = GWEN_Init();
        if (rv != AB_SUCCESS) {
            return false;
        }

        qtGui = new QT5_Gui();
        gwenGui = qtGui->getCInterface();
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
            return false;
        }

        AB_Gui_Extend(gwenGui, aqBanking);

        const QByteArray local8BitAppName = m_applicationInfo.name.toLocal8Bit();
        const QByteArray local8BitAppVersion = m_applicationInfo.version.toLocal8Bit();
        m_chipCardClient = LC_Client_new(local8BitAppName.constData(),
                                         local8BitAppVersion.constData());

        LC_Client_Init(m_chipCardClient);

        m_isInitialized = ((aqBanking != nullptr) && (gwenGui != nullptr));

        return m_isInitialized;
    }

    void finalize()
    {
        if (isInitialized()) {
            AB_Gui_Unextend(gwenGui);
            int rv = AB_Banking_Fini(aqBanking);
            if (rv == AB_SUCCESS) {
                AB_Banking_free(aqBanking);
            }

            GWEN_Gui_SetGui(nullptr);
            GWEN_Gui_free(gwenGui);
            GWEN_Fini();

            LC_Client_Fini(m_chipCardClient);
            LC_Client_free(m_chipCardClient);

            qtGui = nullptr;
            gwenGui = nullptr;
            aqBanking = nullptr;
            m_chipCardClient = nullptr;

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
    QT5_Gui *qtGui;
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

bool Banking::initialize(const QString &name, const QString &version, const QString &key)
{
    return d_ptr->initialize(name, version, key);
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
    if (!d_ptr->isInitialized()) {
        Q_EMIT errorOccurred(AB_ERROR_NOT_INIT, tr("The backend for banking was not initialized!"));
        Q_EMIT finished();
        return;
    }

    AB_ACCOUNT_SPEC_LIST *specList = nullptr;

    int rv = AB_Banking_GetAccountSpecList(d_ptr->aqBanking, &specList);
    if (rv != AB_SUCCESS) {
        Q_EMIT errorOccurred(rv, tr("No account list could be populated!"));
        Q_EMIT finished();
        return;
    }

    auto accounts = d_ptr->accounts(specList);

    AB_AccountSpec_List_free(specList);
    specList = nullptr;

    if (accounts.isEmpty()) {
        Q_EMIT errorOccurred(AB_ERROR_EMPTY, tr("No accounts were found!"));
        Q_EMIT finished();
        return;
    }

    Q_EMIT itemsReceived(accounts);

    Q_EMIT finished();
}
