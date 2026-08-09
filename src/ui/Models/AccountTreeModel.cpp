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

#include "ui/Models/AccountTreeModel.h"

using namespace olbaflinx::ui::models;

using namespace olbaflinx::core::banking;
using namespace olbaflinx::core::banking::account;

AccountTreeModel::AccountTreeModel(QObject *parent)
    : QAbstractListModel(parent)
{}

int AccountTreeModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }

    return static_cast<int>(m_accounts.size());
}

QVariant AccountTreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_accounts.size()) {
        return {};
    }

    const auto &account = m_accounts.at(index.row());

    switch (role) {
    case Qt::DisplayRole:
        return account->toString();
    case UniqueIdRole:
        return account->uniqueId();
    case AccountNameRole:
        return account->accountName();
    case OwnerNameRole:
        return account->ownerName();
    case BankNameRole:
        return account->bankName();
    case IbanRole:
        return account->iban();
    case BicRole:
        return account->bic();
    case AccountNumberRole:
        return account->accountNumber();
    case CurrencyRole:
        return account->currency();
    case BalanceRole:
        return account->balance();
    default:
        return {};
    }
}

QHash<int, QByteArray> AccountTreeModel::roleNames() const
{
    return {
        {UniqueIdRole, QByteArrayLiteral("uniqueId")},
        {AccountNameRole, QByteArrayLiteral("accountName")},
        {OwnerNameRole, QByteArrayLiteral("ownerName")},
        {BankNameRole, QByteArrayLiteral("bankName")},
        {IbanRole, QByteArrayLiteral("iban")},
        {BicRole, QByteArrayLiteral("bic")},
        {AccountNumberRole, QByteArrayLiteral("accountNumber")},
        {CurrencyRole, QByteArrayLiteral("currency")},
        {BalanceRole, QByteArrayLiteral("balance")},
    };
}

void AccountTreeModel::setItems(const BankingItems &items)
{
    beginResetModel();

    m_accounts.clear();
    for (const auto &item : items) {
        if (auto account = std::dynamic_pointer_cast<Account>(item)) {
            m_accounts.append(std::move(account));
        }
    }

    endResetModel();
}
