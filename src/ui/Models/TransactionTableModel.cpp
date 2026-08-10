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

#include <QtCore/QLocale>

using namespace olbaflinx::ui::models;

using namespace olbaflinx::core::banking;
using namespace olbaflinx::core::banking::transaction;
using namespace olbaflinx::core::storage;

namespace {

/**
 * What a booking is written with. Every currency this application deals in
 * carries two, and a value that lost them would round a payment.
 */
constexpr int AmountDecimals = 2;

} // namespace

TransactionTableModel::TransactionTableModel(QObject *parent)
    : QAbstractTableModel(parent)
{}

int TransactionTableModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }

    return static_cast<int>(m_transactions.size());
}

int TransactionTableModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }

    return ColumnCount;
}

QVariant TransactionTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_transactions.size()) {
        return {};
    }

    const auto &transaction = m_transactions.at(index.row());

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case DateColumn:
            // An empty cell for a booking that carries no date. The record
            // allows one, and a placeholder would read like a date of its own.
            return QLocale().toString(transaction->date(), QLocale::ShortFormat);
        case RemoteNameColumn:
            return transaction->remoteName();
        case PurposeColumn:
            return transaction->purpose();
        case ValueColumn: {
            // The sign belongs in the text. A debit that differs from a credit
            // by its colour alone does not reach every reader.
            //
            // Built rather than taken from toCurrencyString, which writes a
            // negative amount in accounting style in several locales: en_US
            // answers "(EUR750,00)" for minus seven hundred and fifty, and a
            // pair of brackets is not a sign anyone has to know to read.
            const auto amount = QLocale().toString(transaction->value(), 'f', AmountDecimals);

            return QStringLiteral("%1 %2").arg(amount, transaction->currency());
        }
        default:
            return {};
        }
    }

    switch (role) {
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

QVariant TransactionTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return QAbstractTableModel::headerData(section, orientation, role);
    }

    switch (section) {
    case DateColumn:
        return tr("Date");
    case RemoteNameColumn:
        //: The other party of a booking, whichever way it goes
        return tr("Counterparty");
    case PurposeColumn:
        return tr("Purpose");
    case ValueColumn:
        return tr("Amount");
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

void TransactionTableModel::setStorage(Storage *storage)
{
    if (m_storage == storage) {
        return;
    }

    if (m_storage != nullptr) {
        disconnect(m_storage, nullptr, this, nullptr);
    }

    m_storage = storage;

    if (m_storage == nullptr) {
        return;
    }

    connect(m_storage, &Storage::itemsReceived, this, [this](const BankingItems &items) {
        takeResult(items);
    });

    // finished arrives on every path, after a failure as well. It is what frees
    // the storage for the next read, so it is what a waiting request waits for.
    connect(m_storage, &Storage::finished, this, [this] { runEnded(); });
}

void TransactionTableModel::setAccountId(quint32 accountId)
{
    if (m_accountId == accountId) {
        return;
    }

    m_accountId = accountId;
    ++m_generation;

    setItems({});
    requestItems();
}

quint32 TransactionTableModel::accountId() const
{
    return m_accountId;
}

void TransactionTableModel::requestItems()
{
    if (m_storage == nullptr || m_accountId == 0) {
        return;
    }

    if (m_pending) {
        m_queued = true;
        return;
    }

    m_pending = true;
    m_queued = false;
    m_requestGeneration = m_generation;

    m_storage->receiveItems({.type = Storage::StorageTransaction, .accountId = m_accountId});
}

void TransactionTableModel::takeResult(const BankingItems &items)
{
    // Only an answer to a request of this model, and only while it still belongs
    // to the account on screen. The storage reports every read through the same
    // signal, the one for the accounts among them.
    if (!m_pending || m_requestGeneration != m_generation) {
        return;
    }

    setItems(items);
}

void TransactionTableModel::runEnded()
{
    if (!m_pending) {
        return;
    }

    m_pending = false;

    if (m_queued) {
        m_queued = false;
        requestItems();
    }
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
