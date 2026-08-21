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
#include "core/Banking/StandingOrder/StandingOrder.h"
#include "core/Storage/Storage.h"

#include <QtCore/QAbstractTableModel>

#include <memory>

namespace olbaflinx::ui::models {

/**
 * Maps the standing orders of one account onto columns and display roles.
 *
 * Ownership: the model holds the records it receives through setItems. That is
 * not a second copy of the truth, because Storage lets go of them once the
 * signal is emitted and holds none of them itself.
 *
 * The holding of an account arrives in a single read and is ordered here rather
 * than in the query: the sort columns of the storage each name a column of the
 * transaction table and are refused on any other type. What that costs is the
 * bound of a single read, and an account beyond it is reported to the log.
 */
class StandingOrderTableModel final : public QAbstractTableModel
{
    Q_OBJECT

public:
    /**
     * The columns of the view, in the order it shows them.
     *
     * The payee is one column and not two: an order names the other party in
     * remote_name, while the own account stands in the fields with the local
     * prefix and is the account the view is showing anyway.
     */
    enum Column : int {
        RemoteNameColumn = 0,
        PurposeColumn,
        NextDateColumn,
        IntervalColumn,
        ValueColumn,
    };
    Q_ENUM(Column)

    static constexpr int ColumnCount = ValueColumn + 1;

    /**
     * Every field of a standing order, whether a column shows it or not.
     *
     * The view shows five of them. The rest stay reachable so that a later view
     * can offer them without the model being rebuilt for it.
     */
    enum Role {
        UniqueIdRole = Qt::UserRole + 1,
        FiIdRole,
        RemoteNameRole,
        RemoteIbanRole,
        PurposeRole,
        ValueRole,
        CurrencyRole,
        PeriodRole,
        CycleRole,
        ExecutionDayRole,
        FirstDateRole,
        LastDateRole,
        NextDateRole,
    };
    Q_ENUM(Role)

    explicit StandingOrderTableModel(QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    [[nodiscard]] int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    /**
     * The header of the amount column carries the currency of the holding.
     *
     * It is taken from the rows that stand and stays out where they carry more
     * than one, which no institution is expected to send for a single account.
     * A header that named one of several would put every amount into the wrong
     * currency.
     */
    [[nodiscard]] QVariant headerData(int section,
                                      Qt::Orientation orientation,
                                      int role = Qt::DisplayRole) const override;

    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    /**
     * Orders the rows that stand by one column.
     *
     * The whole holding of the account, because a read brings it in one go. The
     * interval orders by the span between two executions and not by the word it
     * shows: an order that runs every quarter belongs behind a monthly one, and
     * the alphabet says otherwise.
     *
     * A column outside Column is ignored.
     */
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

    [[nodiscard]] Column sortColumn() const;
    [[nodiscard]] Qt::SortOrder sortOrder() const;

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
     * will never arrive here. The model is therefore empty afterwards, whether
     * nullptr or another storage was passed.
     */
    void setStorage(olbaflinx::core::storage::Storage *storage);

    /**
     * Shows the standing orders of one account.
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
     * Reads the holding of the account again.
     *
     * What a fetch needs afterwards: the rows it stored are in the file and
     * nothing here knows of them. The account and the order stay as they are,
     * and the rows that stand are replaced by what the storage now holds. A read
     * that fails leaves them where they are; a read that brings nothing takes
     * them off, because then the account holds none.
     *
     * Without an account it does nothing. The call returns before the rows
     * arrive.
     */
    void refresh();

public Q_SLOTS:
    /**
     * Takes over the reported records.
     *
     * Records that are not a standing order are skipped. The rows are ordered by
     * the column the model stands on before they are shown.
     */
    void setItems(const olbaflinx::core::banking::BankingItems &items);

Q_SIGNALS:
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
     * an account without standing orders otherwise, which is not merely silence
     * but the wrong answer.
     */
    void readRefused(olbaflinx::core::ErrorCode code, const QString &reason);

private:
    /**
     * What the view opens with, and what it returns to once no account is shown.
     * The payee is what a user looks a standing order up by, and the template of
     * the window carries the indicator on that column.
     */
    static constexpr Column DefaultSortColumn = RemoteNameColumn;
    static constexpr Qt::SortOrder DefaultSortOrder = Qt::AscendingOrder;

    /** Whether a fresh read leaves the rows that stand on screen or takes them off. */
    enum class PreviousRows { Drop, KeepUntilReplaced };

    void applySort(Column column, Qt::SortOrder order);
    void startOver(PreviousRows previousRows = PreviousRows::Drop);
    void dropPreviousRows();
    void requestItems();
    void sortRows();
    void takeResult(const olbaflinx::core::banking::BankingItems &items);
    void takeCount(int count);
    void takeError(olbaflinx::core::ErrorCode code);
    void runEnded();

    QList<std::shared_ptr<olbaflinx::core::banking::standingorder::StandingOrder>> m_orders;

    olbaflinx::core::storage::Storage *m_storage = nullptr;
    quint32 m_accountId = 0;

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
     * Whether the rows on screen are the ones from before a refresh, waiting for
     * the result that takes their place. Every way out of that read clears it,
     * so no result of a later run is taken for the replacement.
     */
    bool m_showingPreviousRows = false;
};

} // namespace olbaflinx::ui::models
