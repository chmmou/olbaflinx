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

#include <QtCore/QCollator>
#include <QtCore/QLocale>

#include <algorithm>
#include <limits>

using namespace olbaflinx::ui::models;

using namespace olbaflinx::core::banking;
using namespace olbaflinx::core::banking::account;

namespace {

// The internal id of an index says which bank an account hangs under. A bank
// itself hangs under nothing, and this value is the one row number that can
// never stand for a bank.
constexpr quintptr NoBank = std::numeric_limits<quintptr>::max();

} // namespace

AccountTreeModel::AccountTreeModel(QObject *parent)
    : QAbstractItemModel(parent)
{}

QModelIndex AccountTreeModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent)) {
        return {};
    }

    if (!parent.isValid()) {
        return createIndex(row, column, NoBank);
    }

    return createIndex(row, column, static_cast<quintptr>(parent.row()));
}

QModelIndex AccountTreeModel::parent(const QModelIndex &child) const
{
    if (!child.isValid() || child.internalId() == NoBank) {
        return {};
    }

    return createIndex(static_cast<int>(child.internalId()), 0, NoBank);
}

int AccountTreeModel::rowCount(const QModelIndex &parent) const
{
    if (!parent.isValid()) {
        return static_cast<int>(m_banks.size());
    }

    if (parent.internalId() != NoBank || parent.row() >= m_banks.size()) {
        return 0;
    }

    return static_cast<int>(m_banks.at(parent.row()).accounts.size());
}

int AccountTreeModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)

    return 1;
}

QVariant AccountTreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return {};
    }

    if (index.internalId() == NoBank) {
        if (role != Qt::DisplayRole || index.row() >= m_banks.size()) {
            return {};
        }

        return m_banks.at(index.row()).name;
    }

    if (index.internalId() >= static_cast<quintptr>(m_banks.size())) {
        return {};
    }

    const auto &accounts = m_banks.at(static_cast<int>(index.internalId())).accounts;
    if (index.row() >= accounts.size()) {
        return {};
    }

    const auto &account = accounts.at(index.row());

    switch (role) {
    case Qt::DisplayRole:
        // The three the entry shows, and no more. An account whose bank reports
        // no IBAN leaves that place empty and stays in the tree.
        return tr("%1 - %2 - %3")
            .arg(account->accountName(),
                 account->iban(),
                 QLocale().toCurrencyString(account->balance(), account->currency()));
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

std::shared_ptr<Account> AccountTreeModel::accountAt(const QModelIndex &index) const
{
    if (!index.isValid() || index.internalId() == NoBank
        || index.internalId() >= static_cast<quintptr>(m_banks.size())) {
        return {};
    }

    const auto &accounts = m_banks.at(static_cast<int>(index.internalId())).accounts;
    if (index.row() >= accounts.size()) {
        return {};
    }

    return accounts.at(index.row());
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

/**
 * Whether the two hold the same accounts under the same banks, in the same
 * places. Only then can the rows keep their indexes, and only the values in
 * them have to be announced as changed.
 */
bool AccountTreeModel::sameShape(const QList<Bank> &left, const QList<Bank> &right)
{
    if (left.size() != right.size()) {
        return false;
    }

    for (int bank = 0; bank < left.size(); ++bank) {
        if (left.at(bank).name != right.at(bank).name
            || left.at(bank).accounts.size() != right.at(bank).accounts.size()) {
            return false;
        }

        for (int row = 0; row < left.at(bank).accounts.size(); ++row) {
            if (left.at(bank).accounts.at(row)->uniqueId()
                != right.at(bank).accounts.at(row)->uniqueId()) {
                return false;
            }
        }
    }

    return true;
}

void AccountTreeModel::setItems(const BankingItems &items)
{
    QList<Bank> banks;

    for (const auto &item : items) {
        auto account = std::dynamic_pointer_cast<Account>(item);
        if (!account || !account->isActive()) {
            continue;
        }

        const auto bankName = account->bankName();

        int bankRow = -1;
        for (int row = 0; row < banks.size(); ++row) {
            if (banks.at(row).name == bankName) {
                bankRow = row;
                break;
            }
        }

        if (bankRow < 0) {
            banks.append(Bank{bankName, {}});
            bankRow = static_cast<int>(banks.size()) - 1;
        }

        banks[bankRow].accounts.append(std::move(account));
    }

    // The order is the one of the language in use. Comparing character values
    // would put every name that starts with an umlaut behind all the others.
    const QCollator order;

    std::sort(banks.begin(), banks.end(), [&order](const Bank &left, const Bank &right) {
        return order.compare(left.name, right.name) < 0;
    });

    for (auto &bank : banks) {
        std::sort(bank.accounts.begin(),
                  bank.accounts.end(),
                  [&order](const std::shared_ptr<Account> &left,
                           const std::shared_ptr<Account> &right) {
                      return order.compare(left->accountName(), right->accountName()) < 0;
                  });
    }

    // The read after a fetch brings the same accounts with new figures. A reset
    // would take the view its selection, the nodes it has open and where it
    // stands, so where every row keeps its place only the values are announced.
    if (sameShape(m_banks, banks)) {
        m_banks = std::move(banks);

        for (int bank = 0; bank < m_banks.size(); ++bank) {
            const QModelIndex bankIndex = index(bank, 0);
            const int rows = static_cast<int>(m_banks.at(bank).accounts.size());

            if (rows > 0) {
                Q_EMIT dataChanged(index(0, 0, bankIndex), index(rows - 1, 0, bankIndex));
            }
        }

        return;
    }

    beginResetModel();

    m_banks = std::move(banks);

    endResetModel();
}
