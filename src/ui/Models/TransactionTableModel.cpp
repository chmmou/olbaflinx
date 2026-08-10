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

#include "ui/Models/TransactionTableModel.h"

using namespace olbaflinx::ui::models;

using namespace olbaflinx::core::banking;
using namespace olbaflinx::core::banking::transaction;

TransactionTableModel::TransactionTableModel(QObject *parent)
    : QAbstractListModel(parent)
{}

int TransactionTableModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }

    return static_cast<int>(m_transactions.size());
}

QVariant TransactionTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_transactions.size()) {
        return {};
    }

    const auto &transaction = m_transactions.at(index.row());

    switch (role) {
    case Qt::DisplayRole:
        return transaction->toString();
    case UniqueIdRole:
        return transaction->uniqueId();
    case DateRole:
        return transaction->date();
    case ValutaDateRole:
        return transaction->valutaDate();
    case ValueRole:
        return transaction->value();
    case CurrencyRole:
        return transaction->currency();
    case PurposeRole:
        return transaction->purpose();
    case RemoteNameRole:
        return transaction->remoteName();
    case RemoteIbanRole:
        return transaction->remoteIban();
    case CategoryRole:
        return transaction->category();
    default:
        return {};
    }
}

QHash<int, QByteArray> TransactionTableModel::roleNames() const
{
    return {
        {UniqueIdRole, QByteArrayLiteral("uniqueId")},
        {DateRole, QByteArrayLiteral("date")},
        {ValutaDateRole, QByteArrayLiteral("valutaDate")},
        {ValueRole, QByteArrayLiteral("value")},
        {CurrencyRole, QByteArrayLiteral("currency")},
        {PurposeRole, QByteArrayLiteral("purpose")},
        {RemoteNameRole, QByteArrayLiteral("remoteName")},
        {RemoteIbanRole, QByteArrayLiteral("remoteIban")},
        {CategoryRole, QByteArrayLiteral("category")},
    };
}

void TransactionTableModel::setItems(const BankingItems &items)
{
    beginResetModel();

    m_transactions.clear();
    for (const auto &item : items) {
        if (auto transaction = std::dynamic_pointer_cast<Transaction>(item)) {
            m_transactions.append(std::move(transaction));
        }
    }

    endResetModel();
}
