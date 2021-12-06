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

#include "TransactionSqlModel.h"

#include "core/Storage/Storage.h"

using namespace olbaflinx::core::storage;
using namespace olbaflinx::core::storage::connection;

using namespace olbaflinx::app::components::sql;

TransactionSqlModel::TransactionSqlModel(QObject *parent)
    : QSqlQueryModel(parent)
{
}

TransactionSqlModel::~TransactionSqlModel() = default;

QVariant TransactionSqlModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    return QSqlQueryModel::headerData(section, orientation, role);
}

int TransactionSqlModel::rowCount(const QModelIndex &parent) const
{
    return QSqlQueryModel::rowCount(parent);
}

QVariant TransactionSqlModel::data(const QModelIndex &item, int role) const
{
    return QSqlQueryModel::data(item, role);
}

void TransactionSqlModel::fetchMore(const QModelIndex &parent)
{
    QSqlQueryModel::fetchMore(parent);
}

bool TransactionSqlModel::canFetchMore(const QModelIndex &parent) const
{
    return QSqlQueryModel::canFetchMore(parent);
}
