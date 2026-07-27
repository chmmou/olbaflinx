/**
 * Copyright (C) 2021-2026, Alexander Saal <developer@olbaflinx.chm-projects.de>
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

#include "core/Storage/StorageConnection.h"

#include <QtCore/QRandomGenerator>

#include <QtSql/QSqlError>

using namespace olbaflinx::core::storage;

StorageConnection::StorageConnection(const QString &fileName, const QString &driver)
    : m_connectionName("")
{
    if (fileName.isEmpty()) {
        return;
    }

    m_connectionName = QString("OLBAFLINX_STORAGE_%1").arg(QRandomGenerator::system()->generate());
    QSqlDatabase db = QSqlDatabase::addDatabase(driver, m_connectionName);
    db.setDatabaseName(fileName);
    db.open();
}

StorageConnection::~StorageConnection() = default;

QSqlDatabase StorageConnection::database()
{
    return QSqlDatabase::database(m_connectionName);
}

void StorageConnection::close()
{
    database().close();
    QSqlDatabase::removeDatabase(m_connectionName);
}

bool StorageConnection::isValid()
{
    return database().isValid();
}

bool StorageConnection::isOpen()
{
    return isValid() && database().isOpen();
}

bool StorageConnection::beginTransaction()
{
    if (isOpen()) {
        return database().transaction();
    }

    return false;
}

bool StorageConnection::commitTransaction()
{
    if (isOpen()) {
        return database().commit();
    }

    return false;
}

bool StorageConnection::rollbackTransaction()
{
    if (isOpen()) {
        return database().rollback();
    }

    return false;
}

QString StorageConnection::lastErrorMessage()
{
    return database().lastError().text();
}
