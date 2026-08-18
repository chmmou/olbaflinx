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
 * Maps the transactions reported by core onto columns and display roles.
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
     * The columns of the view, in the order it shows them.
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
     * Every field of a transaction, whether a column shows it or not.
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
     * Orders the whole holding of the account by one column.
     *
     * The order lies in the query and not above the rows that are loaded, so it
     * reaches every transaction of the account and not only the page on screen.
     * A result of the previous order that arrives afterwards is discarded.
     *
     * The rows that stand are left on screen until the first page under the new
     * order is here, and are replaced by it. A read that brings nothing takes
     * them off, and so does one that fails: they belong to an order the header
     * no longer shows.
     *
     * A column outside Column is ignored.
     */
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

    [[nodiscard]] Column sortColumn() const;
    [[nodiscard]] Qt::SortOrder sortOrder() const;

    /**
     * Whether there is anything left to fetch.
     *
     * When to ask is for the view to decide; it knows its visible area, which
     * nothing here does. This answers only whether asking would bring anything:
     * not while the holding is through, not while a request is running, and not
     * after one came back with a failure.
     */
    [[nodiscard]] bool canFetchMore(const QModelIndex &parent) const override;

    /**
     * Asks for the next page and appends it.
     *
     * Appends rather than replaces: a reset would take the view its position and
     * the user his selection. The call returns before the rows arrive.
     */
    void fetchMore(const QModelIndex &parent) override;

    /**
     * Whether the holding is loaded completely.
     *
     * A run that ended with a failure does not set this, however many rows it
     * left standing. A part of the holding shown as the whole would mislead.
     */
    [[nodiscard]] bool atEnd() const;

    /**
     * Whether a read this model asked for is still going.
     *
     * It tells a failure that belongs to this model from one that belongs to
     * another reader. The storage names no owner on its read signals, and a
     * second reader of the same file is not ruled out.
     */
    [[nodiscard]] bool isReading() const;

    /**
     * The storage the model reads from.
     *
     * Externally owned and has to outlive the model.
     *
     * The rows and the state of a running request go with the storage they
     * belong to: a read that was going is cut off in this very call and its end
     * will never arrive here, and a model that kept waiting for it would queue
     * every later request instead of sending it. The model is therefore empty
     * afterwards, whether nullptr or another storage was passed, and refresh()
     * is what fills it again.
     */
    void setStorage(olbaflinx::core::storage::Storage *storage);

    /**
     * What the filter bar restricts the transactions by.
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
         * Whether the filter takes anything away at all.
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
     * Restricts the transactions that are shown.
     *
     * Like a change of account it drops the rows that stand and starts over: the
     * rows of one condition must not be read under another. A filter outlives a
     * change of account and applies to the next one.
     */
    void setFilter(const Filter &filter);

    [[nodiscard]] Filter filter() const;

    /**
     * How many transactions satisfy the condition.
     *
     * The whole holding of the account under the filter, not the rows that are
     * loaded. It comes from the storage with the result of the read.
     */
    [[nodiscard]] int totalRows() const;

    /**
     * Shows the transactions of one account.
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
     * The account is named by the identifier the institution assigns. 0 stands
     * for no account and empties the model without asking for anything.
     */
    void setAccountId(quint32 accountId);

    [[nodiscard]] quint32 accountId() const;

    /**
     * Reads the holding of the account again, from the top.
     *
     * What a fetch needs afterwards: the rows it stored are in the file and
     * nothing here knows of them. The account, the order and the filter stay as
     * they are, and the rows that stand are replaced by what the storage now
     * holds.
     *
     * Without an account it does nothing. The call returns before the rows
     * arrive.
     */
    void refresh();

public Q_SLOTS:
    /**
     * Takes over the reported records.
     *
     * Records that are not a transaction are skipped.
     */
    void setItems(const olbaflinx::core::banking::BankingItems &items);

Q_SIGNALS:
    /**
     * What totalRows() answers from now on.
     */
    void totalRowsChanged(int totalRows);

    /**
     * The model orders by something else from now on.
     *
     * A click on a header is not the only way the order changes: giving up the
     * account puts it back to the default, and a header indicator that was set
     * once at setup would then show a column the rows no longer stand under.
     * Whoever draws the indicator hangs it on this.
     */
    void sortChanged(int column, Qt::SortOrder order);

    /**
     * The storage turned a request down. The reason is technical and not for
     * the screen.
     *
     * A refusal reaches the caller through the return value alone, so none of
     * the signals of the read path carries it and whoever shows failures to the
     * user would never learn of this one. The view falls back on the words for
     * an account without transactions otherwise, which is not merely silence
     * but the wrong answer.
     */
    void readRefused(olbaflinx::core::ErrorCode code, const QString &reason);

private:
    /**
     * What the view opens with, and what it returns to once no account is shown.
     * The booking a user looks for first is the one that came in last, so the
     * newest stands at the top until he says otherwise.
     */
    static constexpr Column DefaultSortColumn = DateColumn;
    static constexpr Qt::SortOrder DefaultSortOrder = Qt::DescendingOrder;

    /** Whether a fresh read leaves the rows that stand on screen or takes them off. */
    enum class PreviousRows { Drop, KeepUntilReplaced };

    void applySort(Column column, Qt::SortOrder order);
    void startOver(PreviousRows previousRows = PreviousRows::Drop);
    void dropPreviousRows();
    void requestItems();
    void appendItems(const olbaflinx::core::banking::BankingItems &items);
    void takeResult(const olbaflinx::core::banking::BankingItems &items);
    void takeCount(int count);
    void takeError(olbaflinx::core::ErrorCode code);
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

    /**
     * Whether a request was put off because the storage had a read going that
     * this model did not start. Told apart from m_queued, which stands for a
     * request that overtook one of this model's own: this one waits for the end
     * of a run that belongs to somebody else.
     */
    bool m_deferred = false;

    /**
     * How many rows the storage has handed over, which is where the next page
     * starts. Counted separately from the rows the model holds: a record the
     * mapping cannot build would otherwise move the offset back and have the
     * next page repeat what the last one brought.
     */
    int m_loadedRows = 0;

    bool m_atEnd = false;

    /**
     * Whether the rows on screen are the ones of the order before, waiting for
     * the first page of the new one to take their place. Every way out of that
     * read clears it, so no result of a later run is taken for the replacement.
     */
    bool m_showingPreviousRows = false;

    /**
     * Whether the last run ended with a failure. It blocks further fetching
     * until the account, the order or the filter changes; without that the view
     * would ask again at once and meet the same failure.
     */
    bool m_failed = false;
};

} // namespace olbaflinx::ui::models
