/**
 * Copyright (c) 2021, Alexander Saal <developer@olbaflinx.chm-projects.de>
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
#ifndef OLBAFLINX_TRANSACTIONSQLMODEL_H
#define OLBAFLINX_TRANSACTIONSQLMODEL_H

#include <QtSql/QSqlQueryModel>

namespace olbaflinx::app::components::sql
{
class TransactionSqlModel: public QSqlQueryModel
{
Q_OBJECT

public:
    explicit TransactionSqlModel(QObject *parent = nullptr);
    ~TransactionSqlModel();

    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

    int rowCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &item, int role) const override;
    void fetchMore(const QModelIndex &parent) override;
    bool canFetchMore(const QModelIndex &parent) const override;

};
}

#endif // OLBAFLINX_TRANSACTIONSQLMODEL_H
