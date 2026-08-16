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

using olbaflinx::core::ErrorCode;

namespace {

/**
 * What a booking is written with. Every currency this application deals in
 * carries two, and a value that lost them would round a payment.
 */
constexpr int AmountDecimals = 2;

/**
 * How many rows one page holds. It is the view that pages, so the number stands
 * here and not in the storage: the default of an ItemQuery stays where it is, so
 * that another caller is not moved by this choice.
 *
 * A hundred rows fill more than a window at any size a desktop offers, so the
 * view has rows in hand before the user reaches the end of what he sees. It is
 * well below the thousand a single read may ask for at most.
 */
constexpr int PageSize = 100;

/**
 * The column of the storage a column of the view orders by. Two enumerations
 * rather than one, because the view may drop a column or move it without the
 * storage learning of it.
 */
Storage::SortColumn sortColumnOf(const TransactionTableModel::Column column)
{
    switch (column) {
    case TransactionTableModel::DateColumn:
        return Storage::SortColumn::Date;
    case TransactionTableModel::RemoteNameColumn:
        return Storage::SortColumn::RemoteName;
    case TransactionTableModel::PurposeColumn:
        return Storage::SortColumn::Purpose;
    case TransactionTableModel::ValueColumn:
        return Storage::SortColumn::Value;
    }

    return Storage::SortColumn::None;
}

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

    // The run that was going belonged to the storage that is being given up, and
    // its end will never arrive. A model that stays on m_pending answers
    // canFetchMore with false from here on and queues every request instead of
    // sending it, so it would never ask for a row again.
    ++m_generation;

    m_pending = false;
    m_queued = false;
    m_deferred = false;
    m_loadedRows = 0;
    m_atEnd = false;
    m_failed = false;

    setItems({});
    setTotalRows(0);

    m_storage = storage;

    if (m_storage == nullptr) {
        return;
    }

    connect(m_storage, &Storage::itemsReceived, this, [this](const BankingItems &items) {
        takeResult(items);
    });

    connect(m_storage, &Storage::itemsCounted, this, [this](int count) { takeCount(count); });

    // An end and a failure reach the model through two signals of the read path,
    // told apart by the code alone. Without listening to the failure the model
    // would see both as the same event, namely the end below.
    connect(m_storage, &Storage::readFailed, this, [this](ErrorCode code, const QString &) {
        takeError(code);
    });

    // readFinished arrives on every path of a read, after a failure as well. It
    // is what frees the storage for the next one, so it is what a waiting
    // request waits for. The write path has an end of its own and does not reach
    // this model: a write that ended while this read was going would otherwise
    // free a run that is still on its way.
    connect(m_storage, &Storage::readFinished, this, [this] { runEnded(); });
}

void TransactionTableModel::setAccountId(quint32 accountId)
{
    if (m_accountId == accountId) {
        return;
    }

    m_accountId = accountId;

    // Giving up the account is how the window says that the storage was closed.
    // The order belongs to the storage it was chosen in: it outlives a change of
    // account, it does not outlive the file.
    if (m_accountId == 0) {
        applySort(DefaultSortColumn, DefaultSortOrder);
    }

    startOver();
}

void TransactionTableModel::refresh()
{
    if (m_storage == nullptr || m_accountId == 0) {
        return;
    }

    // The same way the account, the order and the filter take. None of the three
    // changed here, so the rows come back under the very conditions they stood
    // under, with whatever the storage holds now.
    startOver();
}

void TransactionTableModel::sort(int column, Qt::SortOrder order)
{
    if (column < 0 || column >= ColumnCount) {
        return;
    }

    const auto sortColumn = static_cast<Column>(column);
    if (m_sortColumn == sortColumn && m_sortOrder == order) {
        return;
    }

    applySort(sortColumn, order);

    startOver();
}

/**
 * The one place the order is changed. Every way there reports it, so that a
 * header indicator hangs on the state of the model rather than on the one moment
 * it was set up.
 */
void TransactionTableModel::applySort(Column column, Qt::SortOrder order)
{
    if (m_sortColumn == column && m_sortOrder == order) {
        return;
    }

    m_sortColumn = column;
    m_sortOrder = order;

    Q_EMIT sortChanged(m_sortColumn, m_sortOrder);
}

TransactionTableModel::Column TransactionTableModel::sortColumn() const
{
    return m_sortColumn;
}

Qt::SortOrder TransactionTableModel::sortOrder() const
{
    return m_sortOrder;
}

bool TransactionTableModel::canFetchMore(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return false;
    }

    if (m_storage == nullptr || m_accountId == 0) {
        return false;
    }

    return !m_atEnd && !m_pending && !m_failed;
}

void TransactionTableModel::fetchMore(const QModelIndex &parent)
{
    if (!canFetchMore(parent)) {
        return;
    }

    requestItems();
}

bool TransactionTableModel::atEnd() const
{
    return m_atEnd;
}

bool TransactionTableModel::isReading() const
{
    return m_pending;
}

quint32 TransactionTableModel::accountId() const
{
    return m_accountId;
}

void TransactionTableModel::setFilter(const Filter &filter)
{
    if (m_filter == filter) {
        return;
    }

    m_filter = filter;
    startOver();
}

TransactionTableModel::Filter TransactionTableModel::filter() const
{
    return m_filter;
}

int TransactionTableModel::totalRows() const
{
    return m_totalRows;
}

/**
 * What the account, the order and the filter have in common: each of them makes
 * a running request stale, drops what is on screen, and asks again from the top.
 */
void TransactionTableModel::startOver()
{
    ++m_generation;

    setItems({});
    setTotalRows(0);

    m_loadedRows = 0;
    m_atEnd = false;

    // The block a failure put on the fetching is lifted here, and here alone.
    // Each of the three changes that lead through this function is the user
    // asking for something else, which is reason enough to try again.
    m_failed = false;

    requestItems();
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
    m_deferred = false;
    m_requestGeneration = m_generation;

    const auto error = m_storage->receiveItems({.type = Storage::StorageTransaction,
                                                .accountId = m_accountId,
                                                .sort = sortColumnOf(m_sortColumn),
                                                .order = m_sortOrder,
                                                .offset = m_loadedRows,
                                                .limit = PageSize,
                                                .text = m_filter.text,
                                                .from = m_filter.from,
                                                .to = m_filter.to,
                                                .direction = m_filter.direction});

    if (!error.isError()) {
        return;
    }

    // No run was started, so not one of the signals of a read will arrive. The
    // model would otherwise wait for an end that has nobody to send it.
    m_pending = false;

    // A storage with a read already going is not a failure of this request. The
    // way is held for a moment, and the end of that read is what this one asks
    // again on. Taking it for a failure would leave the view claiming the
    // account holds no bookings while they lie untouched in the file.
    if (error.code() == ErrorCode::Busy) {
        m_deferred = true;
        return;
    }

    m_failed = true;

    Q_EMIT readRefused(error.code(), error.message());
}

/**
 * Adds a page to what stands. A reset would be the shorter way and the wrong
 * one: it takes the view its position and the user his selection, and both are
 * his and not the model's to give up while he is scrolling.
 */
void TransactionTableModel::appendItems(const BankingItems &items)
{
    auto transactions = QList<std::shared_ptr<Transaction>>();
    transactions.reserve(items.size());

    for (const auto &item : items) {
        if (auto transaction = std::dynamic_pointer_cast<Transaction>(item)) {
            transactions.append(std::move(transaction));
        }
    }

    if (transactions.isEmpty()) {
        return;
    }

    const int first = static_cast<int>(m_transactions.size());

    beginInsertRows({}, first, first + static_cast<int>(transactions.size()) - 1);
    m_transactions.append(transactions);
    endInsertRows();
}

void TransactionTableModel::takeResult(const BankingItems &items)
{
    // Only an answer to a request of this model, and only while it still belongs
    // to the account on screen. The storage reports every read through the same
    // signal, the one for the accounts among them.
    if (!m_pending || m_requestGeneration != m_generation) {
        return;
    }

    appendItems(items);

    m_loadedRows += static_cast<int>(items.size());

    // The holding is through as soon as as many rows stand as the storage
    // counted. That costs no request of its own: the number travels with every
    // result anyway, and an account of exactly one full page is done with that
    // page.
    //
    // A page that came back short is the second way there, and it is needed:
    // a run whose count failed reports no number, and then the first way has
    // nothing to compare against.
    if ((m_totalRows > 0 && m_loadedRows >= m_totalRows) || items.size() < PageSize) {
        m_atEnd = true;
    }
}

void TransactionTableModel::takeError(ErrorCode code)
{
    if (!m_pending || m_requestGeneration != m_generation) {
        return;
    }

    // A read that found no record is not a failure. It is the end of the
    // holding, and the rows that stand stay where they are.
    if (code == ErrorCode::NotFound) {
        m_atEnd = true;
        return;
    }

    m_failed = true;
}

void TransactionTableModel::takeCount(int count)
{
    // Under the same conditions as the records: it belongs to the run this model
    // started, and only while that run still asks what is on screen.
    if (!m_pending || m_requestGeneration != m_generation) {
        return;
    }

    setTotalRows(count);
}

void TransactionTableModel::setTotalRows(int totalRows)
{
    if (m_totalRows == totalRows) {
        return;
    }

    m_totalRows = totalRows;

    Q_EMIT totalRowsChanged(m_totalRows);
}

void TransactionTableModel::runEnded()
{
    if (!m_pending) {
        // The end of a read this model did not start. What it frees is the way
        // to the storage, and that is exactly what a request put off for a busy
        // one is waiting for.
        if (m_deferred) {
            m_deferred = false;
            requestItems();
        }

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
