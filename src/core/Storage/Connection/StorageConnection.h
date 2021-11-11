/**
 * Copyright (c) 2021, Alexander Saal <alexander.saal@chm-projects.de>
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

#ifndef OLBAFLINX_STORAGECONNECTION_H
#define OLBAFLINX_STORAGECONNECTION_H

#include <QtCore/QObject>
#include <QtSql/QSqlDatabase>

namespace olbaflinx::core::storage::connection
{

class StorageConnection: public QObject
{
Q_OBJECT

public:
    explicit StorageConnection(const QString &fileName,
                               const QString &driver = "QSQLCIPHER",
                               QObject *parent = nullptr);

    ~StorageConnection() override;

    QSqlDatabase database();
    void close();

    bool isValid();
    bool isOpen();

    const QString lastErrorMessage();

protected:
    QString connectionName;

};

}

#endif //OLBAFLINX_STORAGECONNECTION_H
