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
#include <QtCore/QTimer>
#include <QtCore/QThread>
#include <QtCore/QDebug>

#include "core/Banking/Banking.h"

#include "OptionPage.h"

using namespace olbaflinx::core::banking;
using namespace olbaflinx::app::banking::assistant::page;

QTimer *accountTimer = nullptr;

QThread *accountTimerThread = nullptr;

bool isOptionPageComplete = false;

OptionPage::OptionPage(QWidget *parent)
    : QWizardPage(parent)
{
    setupUi(this);
}

OptionPage::~OptionPage()
{
    accountTimer->stop();

    accountTimerThread->quit();
    accountTimerThread->wait();
    accountTimerThread->deleteLater();

    accountTimer->deleteLater();
}

void OptionPage::initialize()
{
    connect(Banking::instance(), &Banking::accountIdsReceived, this, &OptionPage::checkForAccounts);

    accountTimer = new QTimer(nullptr);
    accountTimer->setInterval(750);
    connect(accountTimer, &QTimer::timeout, Banking::instance(), &Banking::receiveAccountIds);

    accountTimerThread = new QThread(this);
    connect(accountTimerThread, &QThread::started, accountTimer, qOverload<>(&QTimer::start));

    accountTimer->moveToThread(accountTimerThread);
    accountTimerThread->start();
}

bool OptionPage::isComplete() const
{
    return isOptionPageComplete && treeWidgetAccounts->topLevelItemCount() > 0;
}

AccountIds OptionPage::selectedAccounts() const
{
    AccountIds accountIds = {};
    const auto selectedItems = treeWidgetAccounts->selectedItems();
    for (const auto item: selectedItems) {
        const quint32 id = item->data(0, Qt::UserRole).toUInt();
        accountIds << id;
        qDebug() << id;
    }

    return accountIds;
}

void OptionPage::checkForAccounts(const AccountIds &accountIds)
{
    isOptionPageComplete = !accountIds.empty();
    if (isOptionPageComplete) {
        accountTimer->stop();
        accountTimerThread->quit();
        accountTimerThread->wait();
    }
}

void OptionPage::showSetupDialog()
{
    int result = Banking::instance()->showSetupDialog(this);
    if (result == 1) { // Ok
        disconnect(Banking::instance(), &Banking::accountsReceived, nullptr, nullptr);
        connect(
            Banking::instance(),
            &Banking::accountsReceived,
            [=](const AccountList &accounts)
            {
                treeWidgetAccounts->clear();
                for (const auto account: accounts) {
                    if (account->isValid()) {
                        const auto item = new QTreeWidgetItem;
                        item->setText(0, account->toString());
                        item->setData(
                            0,
                            Qt::UserRole,
                            account->uniqueId()
                        );
                        treeWidgetAccounts->addTopLevelItem(item);

                    }
                }

                Q_EMIT completeChanged();
            }
        );

        Banking::instance()->receiveAccounts();
    }
}
