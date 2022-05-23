/**
 * Copyright (C) 2021-2022, Alexander Saal <developer@olbaflinx.chm-projects.de>
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
#include <QtGui/QFont>

#include "core/Storage/VaultStorage.h"

#include "TransactionViewModel.h"

using namespace olbaflinx::core::storage;
using namespace olbaflinx::core::storage::transaction;

TransactionViewModel::TransactionViewModel(QObject *parent, bool isStandingOrderModel)
    : QAbstractTableModel(parent)
    , m_transactions({})
    , m_accountId(0)
    , m_transactionCount(0)
    , m_isStandingOrderModel(isStandingOrderModel)
{ }

TransactionViewModel::~TransactionViewModel() = default;

int TransactionViewModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_transactionCount;
}

int TransactionViewModel::columnCount(const QModelIndex &parent) const
{
    return Columns::ColumnCount;
}

QVariant TransactionViewModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return {};
    }

    const auto transaction = m_transactions.at(index.row());
    if (role == Qt::ForegroundRole) {
        switch (index.column()) {
        case Columns::ColumnValue:
            if (transaction->value() < 0) {
                return QColor(Qt::red);
            }
            return QColor(Qt::black);
        default:
            break;
        }
    } else if (role == Qt::TextAlignmentRole) {
        switch (index.column()) {
        case Columns::ColumnValue:
            return Qt::AlignCenter;
        default:
            break;
        }
    } else if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case Columns::ColumnValutaDate:
            return transaction->valutaDate().toString("dd.MM.yyyy");
        case Columns::ColumnRemoteName:
            return transaction->remoteName();
        case Columns::ColumnPurpose:
            return transaction->purpose();
        case Columns::ColumnValue:
            return QString::asprintf("%.2f", transaction->value());
        case Columns::ColumnCategory:
            return transaction->category();
        default:
            break;
        }
    }

    return {};
}

QVariant TransactionViewModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal) {
        return {};
    }

    if (role != Qt::DisplayRole) {
        return {};
    }

    switch (section) {
    case Columns::ColumnValutaDate:
        return tr("Date");
    case Columns::ColumnRemoteName:
        return tr("Name");
    case Columns::ColumnPurpose:
        return tr("Purpose");
    case Columns::ColumnValue:
        return tr("Value");
    case Columns::ColumnCategory:
        return tr("Category");
    default:
        break;
    }

    return {};
}

bool TransactionViewModel::canFetchMore(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return false;
    }

    return (m_transactionCount < m_transactions.size());
}

void TransactionViewModel::fetchMore(const QModelIndex &parent)
{
    if (parent.isValid()) {
        return;
    }

    int remainder = m_transactions.size() - m_transactionCount;
    int itemsToFetch = qMin(100, remainder);

    beginInsertRows(QModelIndex(), m_transactionCount, m_transactionCount + itemsToFetch - 1);
    m_transactionCount += itemsToFetch;
    m_transactions << VaultStorage::instance()->transactions(m_accountId, 100, m_transactionCount);
    endInsertRows();
}

void TransactionViewModel::setTransactions(const quint32 accountId,
                                           const TransactionList &transactions)
{
    m_accountId = accountId;
    beginInsertRows(QModelIndex(), 0, transactions.size());
    m_transactionCount = 0;
    m_transactions = transactions;
    endInsertRows();
}

void TransactionViewModel::clear()
{
    if (!m_transactions.isEmpty()) {
        beginRemoveRows(QModelIndex(), 0, m_transactions.size());
        qDeleteAll(m_transactions);
        m_transactions.clear();
        m_transactionCount = 0;
        endRemoveRows();
    }
}
