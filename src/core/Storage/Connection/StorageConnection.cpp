/**
 * Copyright (C) 2021, Alexander Saal <developer@olbaflinx.chm-projects.de>
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

#include <QtCore/QRandomGenerator>
#include <QtSql/QSqlError>

#include "StorageConnection.h"

using namespace olbaflinx::core::storage::connection;

StorageConnection::StorageConnection(const QString &fileName, const QString &driver, QObject *parent)
    : QObject(parent),
      connectionName(QString())
{
    if (fileName.isEmpty()) {
        return;
    }

    connectionName = QString("OLBAFLINX_STORAGE_%1").arg(QRandomGenerator::system()->generate());
    QSqlDatabase db = QSqlDatabase::addDatabase(driver, connectionName);
    db.setDatabaseName(fileName);
    db.open();
}

StorageConnection::~StorageConnection() = default;

QSqlDatabase StorageConnection::database()
{
    return QSqlDatabase::database(connectionName);
}

void StorageConnection::close()
{
    database().close();
    QSqlDatabase::removeDatabase(connectionName);
}

bool StorageConnection::isValid()
{
    return database().isValid();
}

bool StorageConnection::isOpen()
{
    return isValid() && database().isOpen();
}

const QString StorageConnection::lastErrorMessage()
{
    return database().lastError().text();
}
