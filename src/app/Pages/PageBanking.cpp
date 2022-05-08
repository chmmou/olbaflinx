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

#include <QtCore/QVariant>
#include <QtWidgets/QAction>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QTreeWidgetItem>

#include "PageBanking.h"
#include "PageBasePrivate.h"

#include "core/Storage/VaultStorage.h"

using namespace olbaflinx::core;
using namespace olbaflinx::core::storage;
using namespace olbaflinx::app::pages;

PageBanking::PageBanking(QWidget *parent)
    : QWidget(parent)
    , d_ptr(new PageBasePrivate())
{}

PageBanking::~PageBanking() {}

void PageBanking::initialize(QMainWindow *mainWindow)
{
    auto accounts = VaultStorage::instance()->accounts();
    if (accounts.isEmpty()) {
        return;
    }

    d_ptr->setMainWindow(mainWindow);
    qRegisterMetaType<const AccountItem *>();

    const auto app = d_ptr->app();

    initializeMenuBar();
    initializeToolbar();

    for (const auto account : accounts) {
        auto item = new QTreeWidgetItem();

        const auto accountItem = new AccountItem();
        accountItem->title = account->toString();
        accountItem->balance = account->balance();

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

void PageBanking::initializeMenuBar() {}

void PageBanking::initializeToolbar() {}
