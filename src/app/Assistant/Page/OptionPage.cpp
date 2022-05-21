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
#include <QtCore/QDebug>
#include <QtCore/QThread>
#include <QtCore/QTimer>

#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QTreeWidgetItem>

#include "core/Banking/OnlineBanking.h"

#include "OptionPage.h"

using namespace olbaflinx::core::banking;
using namespace olbaflinx::app::assistant::page;

OptionPage::OptionPage(QWidget *parent)
    : QWizardPage(parent)
    , m_isOptionPageComplete(false)
{
    setupUi(this);
}

OptionPage::~OptionPage() { }

void OptionPage::initialize()
{
    connect(treeWidgetAccounts, &QTreeWidget::itemSelectionChanged, this, [&]() {
        m_isOptionPageComplete = false;
        const bool hasItemsSelected = !treeWidgetAccounts->selectedItems().isEmpty();
        if (hasItemsSelected) {
            m_isOptionPageComplete = true;
        }
        Q_EMIT completeChanged();
    });

    auto accounts = OnlineBanking::instance()->accounts();
    if (accounts.isEmpty()) {
        return;
    }

    addAccounts(accounts);
    qDeleteAll(accounts);
    accounts.clear();
}

bool OptionPage::isComplete() const
{
    return m_isOptionPageComplete && treeWidgetAccounts->topLevelItemCount() > 0;
}

AccountIds OptionPage::selectedAccountIds() const
{
    AccountIds accountIds = {};
    const auto selectedItems = treeWidgetAccounts->selectedItems();
    for (const auto item : selectedItems) {
        accountIds << item->data(0, Qt::UserRole).toUInt();
    }

    return accountIds;
}

void OptionPage::showSetupDialog()
{
    m_isOptionPageComplete = false;

    int result = OnlineBanking::instance()->setupAccounts(this);
    if (result != 1) {
        Q_EMIT completeChanged();
        return;
    }

    auto accounts = OnlineBanking::instance()->accounts();
    if (accounts.isEmpty()) {
        Q_EMIT completeChanged();
        return;
    }

    m_isOptionPageComplete = true;

    addAccounts(accounts);
    qDeleteAll(accounts);
    accounts.clear();
}

void OptionPage::addAccounts(const AccountList &accounts)
{
    treeWidgetAccounts->clear();
    for (const auto account : accounts) {
        if (account->isValid()) {
            const auto item = new QTreeWidgetItem;
            item->setText(0, account->toString());
            item->setData(0, Qt::UserRole, account->uniqueId());
            treeWidgetAccounts->addTopLevelItem(item);
        }
    }

    Q_EMIT completeChanged();
}
