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

#include "ui/Models/StandingOrderTableModel.h"

#include "ui/Logging.h"

#include <QtCore/QLocale>

#include <algorithm>

using namespace olbaflinx::ui::models;

using namespace olbaflinx::core::banking;
using namespace olbaflinx::core::banking::standingorder;
using namespace olbaflinx::core::storage;

using olbaflinx::core::ErrorCode;

namespace {

/**
 * What an amount is written with. Every currency this application deals in
 * carries two, and a value that lost them would round a payment.
 */
constexpr int AmountDecimals = 2;

/**
 * How the two periods compare in length, so that the interval column can be
 * ordered by how often an order runs. Days of a rough month and of a week; the
 * numbers order the pairs and never reach the screen.
 */
constexpr int DaysPerMonth = 30;
constexpr int DaysPerWeek = 7;

/**
 * A date the way the template of the window shows one: day, month, and a year
 * of four digits, in the field order the locale puts them in.
 *
 * The short format of the locale is not enough on its own. Several locales
 * write the year with two digits there, German among them, and a standing order
 * that names its next execution in 2026 must not read as one from 26.
 */
QString dateText(const QDate &date)
{
    if (!date.isValid()) {
        return {};
    }

    auto format = QLocale().dateFormat(QLocale::ShortFormat);
    if (!format.contains(QLatin1String("yyyy"))) {
        format.replace(QLatin1String("yy"), QLatin1String("yyyy"));
    }

    return QLocale().toString(date, format);
}

/**
 * The word that names period and cycle together, and the fallback for every pair
 * that has none.
 *
 * Which cycles an institution permits is a parameter of that institution, so no
 * complete list exists. The named words cover the usual pairs; everything else
 * says the number, because a cycle that was dropped would turn a quarterly order
 * into a monthly one.
 *
 * A weekly order of cycle four needs no word of its own: the fallback already
 * writes the one that was chosen for it.
 *
 * The plural is not left to a count argument. Without a loaded catalogue Qt
 * hands the source string back with the markers in it, and the fallback never
 * meets the singular anyway: a cycle of one carries a word of its own.
 */
QString intervalText(AB_TRANSACTION_PERIOD period, quint32 cycle)
{
    const auto count = static_cast<int>(cycle);

    if (period == AB_Transaction_PeriodMonthly) {
        switch (count) {
        case 0:
            // The period is known and the cycle is not. Naming the period alone
            // would promise every month, which is a cycle nobody reported.
            return StandingOrderTableModel::tr("Monthly, cycle unknown");
        case 1:
            return StandingOrderTableModel::tr("Monthly");
        case 2:
            return StandingOrderTableModel::tr("Bimonthly");
        case 3:
            return StandingOrderTableModel::tr("Quarterly");
        case 6:
            return StandingOrderTableModel::tr("Semiannually");
        case 12:
            return StandingOrderTableModel::tr("Annually");
        default:
            return StandingOrderTableModel::tr("Every %1 months").arg(count);
        }
    }

    if (period == AB_Transaction_PeriodWeekly) {
        switch (count) {
        case 0:
            return StandingOrderTableModel::tr("Weekly, cycle unknown");
        case 1:
            return StandingOrderTableModel::tr("Weekly");
        case 2:
            return StandingOrderTableModel::tr("Fortnightly");
        default:
            return StandingOrderTableModel::tr("Every %1 weeks").arg(count);
        }
    }

    // The institution reported no period. The cycle it did report stays, so that
    // an order which carries one is told from a row that carries nothing at all.
    if (count <= 0) {
        return StandingOrderTableModel::tr("Unknown");
    }

    return StandingOrderTableModel::tr("Unknown, cycle %1").arg(count);
}

/**
 * How many days lie between two executions, roughly. The interval column orders
 * by this rather than by its text: a quarterly order belongs behind a monthly
 * one, and the alphabet puts it in front.
 *
 * An order without a period has no length to compare and goes to one end.
 */
int intervalSpan(AB_TRANSACTION_PERIOD period, quint32 cycle)
{
    const auto count = static_cast<int>(cycle);

    if (period == AB_Transaction_PeriodMonthly) {
        return DaysPerMonth * count;
    }

    if (period == AB_Transaction_PeriodWeekly) {
        return DaysPerWeek * count;
    }

    return -1;
}

} // namespace

StandingOrderTableModel::StandingOrderTableModel(QObject *parent)
    : QAbstractTableModel(parent)
{}

int StandingOrderTableModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }

    return static_cast<int>(m_orders.size());
}

int StandingOrderTableModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }

    return ColumnCount;
}

QVariant StandingOrderTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_orders.size()) {
        return {};
    }

    const auto &order = m_orders.at(index.row());

    if (role == Qt::TextAlignmentRole) {
        if (index.column() == ValueColumn) {
            return QVariant(Qt::AlignRight | Qt::AlignVCenter);
        }

        return {};
    }

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case RemoteNameColumn:
            return order->remoteName();
        case PurposeColumn:
            return order->purpose();
        case NextDateColumn:
            // An empty cell where the institution named no next execution. Any
            // other date of the order would read as one, and it is not.
            return dateText(order->nextDate());
        case IntervalColumn:
            return intervalText(order->period(), order->cycle());
        case ValueColumn:
            // The currency stands in the header, so the cell carries the number
            // alone. Repeating it in every row would push the amounts apart
            // where they are meant to line up.
            return QLocale().toString(order->value(), 'f', AmountDecimals);
        default:
            return {};
        }
    }

    switch (role) {
    case UniqueIdRole:
        return order->uniqueId();
    case FiIdRole:
        return order->fiId();
    case RemoteNameRole:
        return order->remoteName();
    case RemoteIbanRole:
        return order->remoteIban();
    case PurposeRole:
        return order->purpose();
    case ValueRole:
        return order->value();
    case CurrencyRole:
        return order->currency();
    case PeriodRole:
        return static_cast<int>(order->period());
    case CycleRole:
        return order->cycle();
    case ExecutionDayRole:
        return order->executionDay();
    case FirstDateRole:
        return order->firstDate();
    case LastDateRole:
        return order->lastDate();
    case NextDateRole:
        return order->nextDate();
    default:
        return {};
    }
}

QVariant StandingOrderTableModel::headerData(int section,
                                             Qt::Orientation orientation,
                                             int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return QAbstractTableModel::headerData(section, orientation, role);
    }

    switch (section) {
    case RemoteNameColumn:
        //: The party a standing order pays
        return tr("Payee");
    case PurposeColumn:
        return tr("Purpose");
    case NextDateColumn:
        //: When the order runs next
        return tr("Execution");
    case IntervalColumn:
        return tr("Interval");
    case ValueColumn: {
        auto currency = QString();

        for (const auto &order : m_orders) {
            if (currency.isEmpty()) {
                currency = order->currency();
                continue;
            }

            if (currency != order->currency()) {
                // Two currencies under one account. Naming one of them would put
                // the amounts of the other into a currency they are not in.
                currency.clear();
                break;
            }
        }

        if (currency.isEmpty()) {
            return tr("Amount");
        }

        return tr("Amount (%1)").arg(currency);
    }
    default:
        return {};
    }
}

QHash<int, QByteArray> StandingOrderTableModel::roleNames() const
{
    return {
        {UniqueIdRole, QByteArrayLiteral("uniqueId")},
        {FiIdRole, QByteArrayLiteral("fiId")},
        {RemoteNameRole, QByteArrayLiteral("remoteName")},
        {RemoteIbanRole, QByteArrayLiteral("remoteIban")},
        {PurposeRole, QByteArrayLiteral("purpose")},
        {ValueRole, QByteArrayLiteral("value")},
        {CurrencyRole, QByteArrayLiteral("currency")},
        {PeriodRole, QByteArrayLiteral("period")},
        {CycleRole, QByteArrayLiteral("cycle")},
        {ExecutionDayRole, QByteArrayLiteral("executionDay")},
        {FirstDateRole, QByteArrayLiteral("firstDate")},
        {LastDateRole, QByteArrayLiteral("lastDate")},
        {NextDateRole, QByteArrayLiteral("nextDate")},
    };
}

void StandingOrderTableModel::setStorage(Storage *storage)
{
    if (m_storage == storage) {
        return;
    }

    if (m_storage != nullptr) {
        disconnect(m_storage, nullptr, this, nullptr);
    }

    // The run that was going belonged to the storage that is being given up, and
    // its end will never arrive. A model that stays on m_pending would queue
    // every request instead of sending it, so it would never ask for a row
    // again.
    ++m_generation;

    m_pending = false;
    m_queued = false;
    m_deferred = false;
    m_showingPreviousRows = false;

    setItems({});

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

void StandingOrderTableModel::setAccountId(quint32 accountId)
{
    if (m_accountId == accountId) {
        return;
    }

    m_accountId = accountId;

    // Giving up the account is how the window says that the storage was closed.
    // The order belongs to the storage it was chosen in.
    if (m_accountId == 0) {
        applySort(DefaultSortColumn, DefaultSortOrder);
    }

    startOver();
}

quint32 StandingOrderTableModel::accountId() const
{
    return m_accountId;
}

void StandingOrderTableModel::refresh()
{
    if (m_storage == nullptr || m_accountId == 0) {
        return;
    }

    // The account did not change, so the rows that stand belong to the very
    // holding that is being read again. They stay until the result replaces
    // them; an empty table for that span reads as a loss of the holding rather
    // than as a moment of waiting.
    startOver(PreviousRows::KeepUntilReplaced);
}

void StandingOrderTableModel::sort(int column, Qt::SortOrder order)
{
    if (column < 0 || column >= ColumnCount) {
        return;
    }

    const auto sortColumn = static_cast<Column>(column);
    if (m_sortColumn == sortColumn && m_sortOrder == order) {
        return;
    }

    applySort(sortColumn, order);

    // The rows are here in full, so the new order needs no read. What the view
    // asked for is done by the time the call returns.
    beginResetModel();
    sortRows();
    endResetModel();
}

/**
 * The one place the order is changed. Every way there reports it, so that a
 * header indicator hangs on the state of the model rather than on the one moment
 * it was set up.
 */
void StandingOrderTableModel::applySort(Column column, Qt::SortOrder order)
{
    if (m_sortColumn == column && m_sortOrder == order) {
        return;
    }

    m_sortColumn = column;
    m_sortOrder = order;

    Q_EMIT sortChanged(m_sortColumn, m_sortOrder);
}

StandingOrderTableModel::Column StandingOrderTableModel::sortColumn() const
{
    return m_sortColumn;
}

Qt::SortOrder StandingOrderTableModel::sortOrder() const
{
    return m_sortOrder;
}

bool StandingOrderTableModel::isReading() const
{
    return m_pending;
}

/**
 * Puts the rows into the order the model stands on. Compared in the type behind
 * the column and not in the text it shows: an amount orders numerically, a date
 * chronologically, an interval by its length.
 */
void StandingOrderTableModel::sortRows()
{
    const auto column = m_sortColumn;
    const bool ascending = m_sortOrder == Qt::AscendingOrder;

    std::stable_sort(m_orders.begin(),
                     m_orders.end(),
                     [column, ascending](const std::shared_ptr<StandingOrder> &left,
                                         const std::shared_ptr<StandingOrder> &right) {
                         bool inOrder = false;

                         switch (column) {
                         case RemoteNameColumn:
                             inOrder = QString::localeAwareCompare(left->remoteName(),
                                                                   right->remoteName())
                                       < 0;
                             break;
                         case PurposeColumn:
                             inOrder = QString::localeAwareCompare(left->purpose(), right->purpose())
                                       < 0;
                             break;
                         case NextDateColumn:
                             inOrder = left->nextDate() < right->nextDate();
                             break;
                         case IntervalColumn:
                             inOrder = intervalSpan(left->period(), left->cycle())
                                       < intervalSpan(right->period(), right->cycle());
                             break;
                         case ValueColumn:
                             inOrder = left->value() < right->value();
                             break;
                         }

                         return ascending ? inOrder : !inOrder;
                     });
}

/**
 * What a change of account and a refresh have in common: each of them makes a
 * running request stale and asks again. They differ in what happens to the rows
 * on screen in the meantime.
 */
void StandingOrderTableModel::startOver(PreviousRows previousRows)
{
    ++m_generation;

    // Nothing to hold on to where nothing stands.
    m_showingPreviousRows = previousRows == PreviousRows::KeepUntilReplaced && !m_orders.isEmpty();

    if (!m_showingPreviousRows) {
        setItems({});
    }

    requestItems();
}

/**
 * Takes the rows of the read before off the screen, where nothing replaced them.
 * The account holds none any more.
 */
void StandingOrderTableModel::dropPreviousRows()
{
    if (!m_showingPreviousRows) {
        return;
    }

    m_showingPreviousRows = false;

    setItems({});
}

void StandingOrderTableModel::requestItems()
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

    // The whole holding in one window, because the order is made here and an
    // order over a page would order a part and call it the whole.
    const auto error = m_storage->receiveItems({.type = Storage::StorageStandingOrder,
                                                .accountId = m_accountId,
                                                .limit = Storage::MaxItemsPerQuery});

    if (!error.isError()) {
        return;
    }

    // No run was started, so not one of the signals of a read will arrive. The
    // model would otherwise wait for an end that has nobody to send it.
    m_pending = false;

    // A storage with a read already going is not a failure of this request. The
    // way is held for a moment, and the end of that read is what this one asks
    // again on. Taking it for a failure would leave the view claiming the
    // account holds no standing orders while they lie untouched in the file.
    if (error.code() == ErrorCode::Busy) {
        m_deferred = true;
        return;
    }

    Q_EMIT readRefused(error.code(), error.message());
}

void StandingOrderTableModel::takeResult(const BankingItems &items)
{
    // Only an answer to a request of this model, and only while it still belongs
    // to the account on screen. The storage reports every read through the same
    // signal, the one for the accounts among them.
    if (!m_pending || m_requestGeneration != m_generation) {
        return;
    }

    m_showingPreviousRows = false;

    setItems(items);
}

void StandingOrderTableModel::takeCount(int count)
{
    // Under the same conditions as the records: it belongs to the run this model
    // started, and only while that run still asks what is on screen.
    if (!m_pending || m_requestGeneration != m_generation) {
        return;
    }

    if (count <= Storage::MaxItemsPerQuery) {
        return;
    }

    // The holding is wider than a single read may open, and the order is made
    // over what arrived. What lies beyond it is not shown and not ordered, and
    // silence about it would let the view pass a part off as the whole.
    qCWarning(lcUi) << "account" << m_accountId << "holds" << count
                    << "standing orders and the view shows the first" << Storage::MaxItemsPerQuery;
}

void StandingOrderTableModel::takeError(ErrorCode code)
{
    if (!m_pending || m_requestGeneration != m_generation) {
        return;
    }

    // A read that found no record is not a failure. The account holds no
    // standing order, and rows from before the read would show one it no longer
    // has.
    if (code == ErrorCode::NotFound) {
        dropPreviousRows();
        return;
    }

    // Every other failure leaves the rows where they are. What stood was read
    // from this very account, and taking it away would answer a failure with the
    // notice for an account without standing orders.
    m_showingPreviousRows = false;
}

void StandingOrderTableModel::runEnded()
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

void StandingOrderTableModel::setItems(const BankingItems &items)
{
    beginResetModel();

    m_orders.clear();
    for (const auto &item : items) {
        if (auto order = std::dynamic_pointer_cast<StandingOrder>(item)) {
            m_orders.append(std::move(order));
        }
    }

    sortRows();

    endResetModel();
}
