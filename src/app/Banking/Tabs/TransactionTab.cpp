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

#include <QtCore/QThread>
#include <QtCore/QTimer>
#include <QtCore/QDebug>

#include "TransactionTab.h"

#include "core/Storage/Storage.h"
#include "core/Storage/Account/Account.h"
#include "core/Banking/Banking.h"
#include "core/Logger/Logger.h"

#include "app/Components/FilterWidget.h"

using namespace olbaflinx::app::banking::tabs;

using namespace olbaflinx::core::storage;
using namespace olbaflinx::core::banking;
using namespace olbaflinx::core::logger;

using namespace olbaflinx::app::components;
using namespace olbaflinx::app::banking;

Account *selectedAccount = nullptr;

TransactionTab::TransactionTab(QWidget *parent)
    : QWidget(parent)
{
    setupUi(this);

    connect(
        filterWidget, &FilterWidget::searchTextChanged,
        this, &TransactionTab::searchTextChanged
    );
    connect(
        filterWidget, &FilterWidget::dateTimePeriodChanged,
        this, &TransactionTab::dateTimePeriodChanged
    );
    connect(
        filterWidget, &FilterWidget::dateChanged,
        this, &TransactionTab::dateChanged
    );
    connect(
        pushButtonRefreshTransactions, &QPushButton::clicked,
        this, &TransactionTab::refreshTransactions
    );
}

TransactionTab::~TransactionTab()
{
    if (Banking::instance()->thread()->isRunning()) {
        LOG("Banking Backend Thread is running  ...");
        Banking::instance()->thread()->quit();
        LOG("Banking Backend Thread is running  ... [stopped]");
    }

    if (Storage::instance()->thread()->isRunning()) {
        LOG("Storage Backend Thread is running  ...");
        Storage::instance()->thread()->quit();
        LOG("Storage Backend Thread is running  ... [stopped]");
    }
}

void TransactionTab::searchTextChanged(const QString &searchText, const bool isRegularExpression)
{
    // @todo
}

void TransactionTab::dateTimePeriodChanged(const QDate &from, const QDate &to)
{

}

void TransactionTab::dateChanged(const QDate &date)
{
    // @todo
}

void TransactionTab::receiveStorageTransactions()
{
    auto thread = qobject_cast<QThread *>(sender());

    disconnect(Storage::instance(), &Storage::transactionsReceived, nullptr, nullptr);
    connect(
        Storage::instance(),
        &Storage::transactionsReceived,
        [=](const TransactionList &transactions)
        {
            LOG(QString("receiveStorageTransaction: Found %1 transaction for %2 account ...")
                            .arg(transactions.size())
                            .arg(selectedAccount->iban()));

            if (transactions.isEmpty()) {
                LOG("receiveStorageTransactions: Stopping worker thread ...");
                thread->quit();
                thread->wait();
                LOG("receiveStorageTransactions: Stopping worker thread ... [done]");
                return;
            }

            LOG("Stopping worker thread ...");
            thread->quit();
            thread->wait();
            LOG("Stopping worker thread ... [done]");
        }
    );

    Storage::instance()->receiveTransactions(selectedAccount->uniqueId());

}

void TransactionTab::receiveOnlineTransactions()
{
    auto thread = qobject_cast<QThread *>(sender());

    disconnect(Banking::instance(), &Banking::transactionsReceived, nullptr, nullptr);
    connect(
        Banking::instance(),
        &Banking::transactionsReceived,
        [=](const TransactionList &transactions, const QString &balance)
        {
            if (transactions.isEmpty()) {
                LOG("receiveOnlineTransactions: Stopping worker thread ...");
                thread->quit();
                thread->wait();
                LOG("receiveOnlineTransactions: Stopping worker thread ... [done]");
                return;
            }

            LOG("receiveOnlineTransactions: Storing transaction ...");
            Storage::instance()->storeTransactions(selectedAccount->uniqueId(), transactions);
            LOG("receiveOnlineTransactions: Storing transaction ... [finished]");

            LOG("receiveOnlineTransactions: Stopping worker thread ...");
            thread->quit();
            thread->wait();

            LOG("receiveOnlineTransactions: Stopping worker thread ... [done]");
        }
    );

    Banking::instance()->receiveTransactions(
        selectedAccount,
        filterWidget->fromDate(),
        filterWidget->toDate()
    );
}

void TransactionTab::accountChanged(const quint32 accountId)
{
    if (accountId == 0) {
        return;
    }

    if (selectedAccount != nullptr) {
        delete selectedAccount;
        selectedAccount = nullptr;
    }

    selectedAccount = Banking::instance()->account(accountId);

    auto transactionWorker = new QThread;
    QObject::moveToThread(Storage::instance()->thread());

    connect(transactionWorker, &QThread::started,
            this, &TransactionTab::receiveStorageTransactions
    );

    connect(
        transactionWorker, &QThread::finished,
        transactionWorker, &QThread::deleteLater
    );

    transactionWorker->start();

}

void TransactionTab::refreshTransactions()
{
    if (selectedAccount == nullptr) {
        return;
    }

    auto transactionWorker = new QThread;
    QObject::moveToThread(Banking::instance()->thread());

    connect(
        transactionWorker, &QThread::started,
        this, &TransactionTab::receiveOnlineTransactions
    );

    connect(
        transactionWorker, &QThread::finished,
        transactionWorker, &QThread::deleteLater
    );

    transactionWorker->start();
}
