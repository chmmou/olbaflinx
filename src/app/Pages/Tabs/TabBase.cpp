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
#include "core/Storage/VaultStorage.h"

#include "TabBase.h"

using namespace olbaflinx::core::storage;
using namespace olbaflinx::core::storage::transaction;
using namespace olbaflinx::app::pages::tabs;

TabBase::TabBase(QWidget *parent, const bool isStandingOrderTab)
    : QWidget(parent)
    , m_accountId(0)
    , m_isStandingOrderTab(isStandingOrderTab)
    , m_transactions({})
{
    setupUi(this);
}

TabBase::~TabBase() = default;

void TabBase::setAccountId(const quint32 id)
{
    m_accountId = id;
    Q_EMIT accountChanged(m_accountId);
}

TransactionList TabBase::transactions()
{
    m_transactions = VaultStorage::instance()->transactions(m_accountId);
    if (m_transactions.isEmpty()) {
        return {};
    }

    qApp->setOverrideCursor(Qt::WaitCursor);

    QEventLoop loop(this);
    QFutureWatcher<TransactionList> transactionWatcher(this);
    connect(&transactionWatcher, &QFutureWatcher<TransactionList>::finished, &loop, [&]() {
        loop.quit();
        transactionWatcher.cancel();
        transactionWatcher.waitForFinished();

        qApp->restoreOverrideCursor();
    });

    transactionWatcher.setFuture(QtConcurrent::filteredReduced<TransactionList>(
        m_transactions,
        [&](const Transaction *t) -> bool {
            return m_isStandingOrderTab ? t->isStandingOrder() : !t->isStandingOrder();
        },
        [](TransactionList &list, const Transaction *t) { list.append(t); },
        QtConcurrent::OrderedReduce));

    loop.exec();

    return transactionWatcher.result();
}
