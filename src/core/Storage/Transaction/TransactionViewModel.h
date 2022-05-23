/**
 * Copyright (C) 2021-2022, Alexander Saal <developer@olbaflinx.chm-projects.de>
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

#ifndef OLBAFLINX_TRANSACTIONVIEWMODEL_H
#define OLBAFLINX_TRANSACTIONVIEWMODEL_H

#include <QtCore/QAbstractTableModel>

#include "core/Container.h"

namespace olbaflinx::core::storage::transaction {

class TransactionViewModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Columns {
        ColumnValutaDate = 0,
        ColumnRemoteName,
        ColumnPurpose,
        ColumnCategory,
        ColumnValue,
        ColumnCount
    };

    explicit TransactionViewModel(QObject *parent = Q_NULLPTR, bool isStandingOrderModel = false);
    ~TransactionViewModel() override;

    int rowCount(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    void setTransactions(const quint32 accountId, const TransactionList &transactions);
    void clear();

protected:
    bool canFetchMore(const QModelIndex &parent) const override;
    void fetchMore(const QModelIndex &parent) override;

private:
    TransactionList m_transactions;
    quint32 m_accountId;
    int m_transactionCount;
    bool m_isStandingOrderModel;
};

} // namespace olbaflinx::core::storage::transaction

#endif //OLBAFLINX_TRANSACTIONVIEWMODEL_H
