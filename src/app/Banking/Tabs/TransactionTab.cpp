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

#include "TransactionTab.h"

#include "core/Storage/Storage.h"
#include "core/Storage/Account/Account.h"
#include "core/Banking/Banking.h"

#include "app/Components/FilterWidget.h"

using namespace olbaflinx::app::banking::tabs;

using namespace olbaflinx::core;
using namespace olbaflinx::core::storage;
using namespace olbaflinx::core::banking;

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

TransactionTab::~TransactionTab() = default;

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

void TransactionTab::receiveTransactions()
{
    disconnect(Banking::instance(), &Banking::transactionsReceived, nullptr, nullptr);
    connect(
        Banking::instance(),
        &Banking::transactionsReceived,
        [=](const TransactionList &transactions)
        {
            if (transactions.isEmpty()) {
                return;
            }

            Storage::instance()->storeTransactions(transactions);
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
}

void TransactionTab::refreshTransactions()
{
    if (selectedAccount == nullptr) {
        return;
    }

    auto transactionWorker = new QThread(this);
    connect(transactionWorker, &QThread::started,
            this, &TransactionTab::receiveTransactions);
    connect(transactionWorker, &QThread::finished,
            transactionWorker, &TransactionTab::deleteLater);

    Banking::instance()->moveToThread(transactionWorker);
    transactionWorker->start();
}
