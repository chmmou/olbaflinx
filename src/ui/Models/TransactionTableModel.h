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

public Q_SLOTS:
    /**
     * @brief Takes over the reported records.
     *
     * Records that are not a transaction are skipped.
     */
    void setItems(const olbaflinx::core::banking::BankingItems &items);

private:
    QList<std::shared_ptr<olbaflinx::core::banking::transaction::Transaction>> m_transactions;
};

} // namespace olbaflinx::ui::models
