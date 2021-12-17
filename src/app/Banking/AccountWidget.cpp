/**
 * Copyright (C) 2021, Alexander Saal <developer@olbaflinx.chm-projects.de>
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

#include <QtCore/QTimer>

#include "AccountWidget.h"
#include "AccountWidgetItem.h"

#include "core/Storage/Account/Account.h"

using namespace olbaflinx::core::storage::account;
using namespace olbaflinx::app::banking;

int metaTypeId = 0;

AccountWidget::AccountWidget(QWidget *parent)
    : QTreeWidget(parent)
{
    metaTypeId = qRegisterMetaType<AccountWidgetItem *>();
}

AccountWidget::~AccountWidget()
{
    QMetaType::unregisterType(metaTypeId);
}

void AccountWidget::setAccounts(const AccountList &accounts)
{
    if (accounts.isEmpty()) {
        return;
    }

    // ToDo Alexander Saal: Add AccountWidgetItem as item widget
    for (const auto account: accounts) {
        const auto accountItem = new QTreeWidgetItem();
        accountItem->setText(0, account->toString());
        accountItem->setData(0, Qt::UserRole, account->uniqueId());
        addTopLevelItem(accountItem);
    }

    disconnect(this, &QTreeWidget::itemClicked, nullptr, nullptr);
    connect(
        this,
        &QTreeWidget::itemClicked,
        [=](QTreeWidgetItem *item, int column)
        {
            // ToDo Alexander Saal: Get AccountWidgetItem and than account uniqueId
            Q_EMIT accountChanged(item->data(column, Qt::UserRole).toUInt());
        }
    );

    const auto item = topLevelItem(0);
    if (item != nullptr) {
        setCurrentItem(item, 0);
        Q_EMIT itemClicked(item, 0);
    }
}
