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

#pragma once

#include "core/Banking/BankingItem.h"
#include "core/Banking/Transaction/Transaction.h"
#include "core/Storage/Storage.h"

#include <QtCore/QAbstractTableModel>

#include <memory>

namespace olbaflinx::ui::models {

/**
 * @brief Maps the transactions reported by core onto columns and display roles.
 *
 * Ownership: the model holds the records it receives through setItems. That is
 * not a second copy of the truth, because Storage lets go of them once the
 * signal is emitted and holds none of them itself.
 */
class TransactionTableModel final : public QAbstractTableModel
{
    Q_OBJECT

public:
    /**
     * @brief The columns of the view, in the order it shows them.
     *
     * The other party is one column and not two: the record names it in
     * remote_name whichever way the booking goes, while the own account stands
     * in the fields with the local prefix.
     */
    enum Column : int {
        DateColumn = 0,
        RemoteNameColumn,
        PurposeColumn,
        ValueColumn,
    };
    Q_ENUM(Column)

    static constexpr int ColumnCount = ValueColumn + 1;

    /**
     * @brief Every field of a transaction, whether a column shows it or not.
     *
     * The view shows four of them. The rest stay reachable so that a later view
     * can offer them without the model being rebuilt for it.
     */
    enum Role {
        UniqueIdRole = Qt::UserRole + 1,
        DateRole,
        ValutaDateRole,
        ValueRole,
        CurrencyRole,
        PurposeRole,
        RemoteNameRole,
        RemoteIbanRole,
        CategoryRole,
    };
    Q_ENUM(Role)

    explicit TransactionTableModel(QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    [[nodiscard]] int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    [[nodiscard]] QVariant headerData(int section,
                                      Qt::Orientation orientation,
                                      int role = Qt::DisplayRole) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    /**
     * @brief Orders the whole holding of the account by one column.
     *
     * The order lies in the query and not above the rows that are loaded, so it
     * reaches every transaction of the account and not only the page on screen.
     * Like a change of account it drops what stands and starts over; a result of
     * the previous order that arrives afterwards is discarded.
     *
     * @param column One of Column. Anything outside that range is ignored.
     * @param order Ascending or descending.
     */
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

    /**
     * @brief The storage the model reads from.
     *
     * Externally owned and has to outlive the model. Passing nullptr detaches
     * it; the model then keeps what it holds and asks for nothing.
     */
    void setStorage(olbaflinx::core::storage::Storage *storage);

    /**
     * @brief What the filter bar restricts the transactions by.
     *
     * Every field is optional. The text is looked for in the name of the other
     * party and in the purpose, the dates are inclusive bounds, and a booking of
     * nought counts as neither direction.
     */
    struct Filter
    {
        QString text = {};
        QDate from = {};
        QDate to = {};
        olbaflinx::core::storage::Storage::Direction direction
            = olbaflinx::core::storage::Storage::Direction::Any;

        /**
         * @brief Whether the filter takes anything away at all.
         *
         * It is what tells an account without transactions from an account whose
         * transactions the filter leaves out. The two need different words.
         */
        [[nodiscard]] bool isSet() const
        {
            return !text.isEmpty() || from.isValid() || to.isValid()
                   || direction != olbaflinx::core::storage::Storage::Direction::Any;
        }

        bool operator==(const Filter &other) const = default;
    };

    /**
     * @brief Restricts the transactions that are shown.
     *
     * Like a change of account it drops the rows that stand and starts over: the
     * rows of one condition must not be read under another. A filter outlives a
     * change of account and applies to the next one.
     */
    void setFilter(const Filter &filter);

    [[nodiscard]] Filter filter() const;

    /**
     * @brief How many transactions satisfy the condition.
     *
     * The whole holding of the account under the filter, not the rows that are
     * loaded. It comes from the storage with the result of the read.
     */
    [[nodiscard]] int totalRows() const;

    /**
     * @brief Shows the transactions of one account.
     *
     * The rows of the account that was shown before are dropped at once. They do
     * not belong under the account that is chosen now, and leaving them standing
     * while the new ones are on their way shows a holding to the wrong name.
     *
     * A change while a read is running does not reach the storage right away.
     * The storage refuses a second read, and a change of account is an everyday
     * move rather than a failure, so the request waits for the end of the run.
     * Only the latest one waits: three changes during one run make one request.
     *
     * @param accountId The identifier the institution assigns. 0 stands for no
     *  account and empties the model without asking for anything.
     */
    void setAccountId(quint32 accountId);

    [[nodiscard]] quint32 accountId() const;

public Q_SLOTS:
    /**
     * @brief Takes over the reported records.
     *
     * Records that are not a transaction are skipped.
     */
    void setItems(const olbaflinx::core::banking::BankingItems &items);

Q_SIGNALS:
    /**
     * @brief This signal is emitted when the number under the condition changed.
     *
     * @param totalRows What totalRows() answers from now on.
     */
    void totalRowsChanged(int totalRows);

private:
    /**
     * What the view opens with, and what it returns to once no account is shown.
     * The booking a user looks for first is the one that came in last, so the
     * newest stands at the top until he says otherwise.
     */
    static constexpr Column DefaultSortColumn = DateColumn;
    static constexpr Qt::SortOrder DefaultSortOrder = Qt::DescendingOrder;

    void startOver();
    void requestItems();
    void takeResult(const olbaflinx::core::banking::BankingItems &items);
    void takeCount(int count);
    void setTotalRows(int totalRows);
    void runEnded();

    QList<std::shared_ptr<olbaflinx::core::banking::transaction::Transaction>> m_transactions;

    olbaflinx::core::storage::Storage *m_storage = nullptr;
    quint32 m_accountId = 0;
    Filter m_filter = {};
    int m_totalRows = 0;

    Column m_sortColumn = DefaultSortColumn;
    Qt::SortOrder m_sortOrder = DefaultSortOrder;

    /**
     * Tells the request that is running from the one the user has since asked
     * for. A result that comes back under an older number belongs to an account
     * nobody is looking at any more.
     */
    quint64 m_generation = 0;
    quint64 m_requestGeneration = 0;

    bool m_pending = false;
    bool m_queued = false;
};

} // namespace olbaflinx::ui::models
