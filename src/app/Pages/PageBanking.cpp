/**
* Copyright (C) 2022, Alexander Saal <developer@olbaflinx.chm-projects.de>
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

#include <QtCore/QDebug>
#include <QtCore/QVariant>

#include <QtWidgets/QAction>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QTreeWidgetItem>

#include "core/MaterialDesign/MaterialDesign.h"
#include "core/MaterialDesign/MaterialDesignNames.h"
#include "core/Storage/VaultStorage.h"

#include "PageBanking.h"
#include "PageBasePrivate.h"

using namespace olbaflinx::core;
using namespace olbaflinx::core::material::design;
using namespace olbaflinx::core::material::design::names;
using namespace olbaflinx::core::storage;
using namespace olbaflinx::app::pages;

PageBanking::PageBanking(QWidget *parent)
    : QWidget(parent)
    , d_ptr(new PageBasePrivate())
    , m_typeIds({})
{ }

PageBanking::~PageBanking() { }

void PageBanking::initialize(QMainWindow *mainWindow)
{
    d_ptr->setMainWindow(mainWindow);
    m_typeIds << qRegisterMetaType<const AccountItem *>();

    initializeMenuBar();
    initializeToolbar();
    initializeStatusBar();

    auto accounts = VaultStorage::instance()->accounts();
    if (accounts.isEmpty()) {
        return;
    }

    const auto app = d_ptr->app();

    connect(app->treeWidgetBankingAccounts,
            &QTreeWidget::itemSelectionChanged,
            this,
            &PageBanking::accountChanged);

    for (const auto account : accounts) {
        auto item = new QTreeWidgetItem();

        const auto accountItem = new AccountItem();
        accountItem->title = account->toString();
        accountItem->balance = account->balance();
        accountItem->id = account->uniqueId();

        item->setText(0, account->toString());
        item->setData(0, Qt::UserRole, QVariant::fromValue(accountItem));

        app->treeWidgetBankingAccounts->addTopLevelItem(item);
    }

    // app->tabTransactions
    // app->tabStandingOrder
    // app->tabDocuments
    // app->tabJobs

    qDeleteAll(accounts);
    accounts.clear();
}

void PageBanking::deInitialize()
{
    const auto app = d_ptr->app();

    for (const auto typeId : m_typeIds) {
        QMetaType::unregisterType(typeId);
    }
    m_typeIds.clear();

    app->treeWidgetBankingAccounts->clear();
    app->tabTransactions->reset();
    app->tabStandingOrders->reset();
    app->appToolBar->clear();
    app->appToolBar->hide();
    app->appStatusBar->clearMessage();
    app->appStatusBar->hide();
}

void PageBanking::accountChanged()
{
    const auto app = d_ptr->app();
    auto items = app->treeWidgetBankingAccounts->selectedItems();

    if (items.isEmpty()) {
        return;
    }

    const auto item = items.at(0);
    const auto itemData = item->data(0, Qt::UserRole);
    if (itemData.isValid() && !itemData.isNull()) {
        const auto accountItem = itemData.value<AccountItem *>();
        app->tabTransactions->setAccountId(accountItem->id);
        app->tabStandingOrders->setAccountId(accountItem->id);
    }

    items.clear();
}

void PageBanking::initializeMenuBar() { }

void PageBanking::initializeToolbar()
{
    const auto app = d_ptr->app();
    app->appToolBar->show();
    app->appToolBar->addAction(MaterialDesign::icon(MaterialDesignNames::Close), "Close", [&]() {
        Q_EMIT vaultClosed();
    });
}

void PageBanking::initializeStatusBar()
{
    const auto app = d_ptr->app();
    app->appStatusBar->show();
}
