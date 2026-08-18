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

#pragma once

#include "core/OlbaFlinxCore.h"

#include <QtSql/QSqlDatabase>

namespace olbaflinx::core::storage {

/**
 * One named connection to an encrypted storage file.
 *
 * Ownership: the instance belongs to whoever creates it. The destructor
 * removes the connection it registered with Qt, so an instance must not
 * outlive the QSqlDatabase handles taken from database(); those stay valid
 * only as long as this object lives.
 *
 * The connection name is derived per instance, which lets several storage
 * files be open side by side without them sharing a handle.
 */
class OLBAFLINX_CORE_EXPORT StorageConnection
{
public:
    explicit StorageConnection(const QString &fileName,
                               const QString &driver = QStringLiteral("QSQLCIPHER"));
    ~StorageConnection();

    [[nodiscard]] QSqlDatabase database() const;

    /**
     * Closing a connection that is already closed or was never valid does
     * nothing and is not an error.
     */
    void close();

    [[nodiscard]] bool isValid() const;

    /**
     * Tells whether the driver this connection was asked for is registered with
     * Qt. A deployment without the SQLCipher plugin fails to open every file,
     * which without this is indistinguishable from a wrong pass phrase.
     */
    [[nodiscard]] bool isDriverAvailable() const;

    /**
     * Valid and open both have to hold; a valid connection that was never
     * opened answers false.
     */
    [[nodiscard]] bool isOpen() const;

    /**
     * The three transaction calls answer false on a connection that is not
     * open, which is the same answer a failed transaction gives.
     */
    [[nodiscard]] bool beginTransaction();

    [[nodiscard]] bool commitTransaction();

    [[nodiscard]] bool rollbackTransaction();

    [[nodiscard]] QString lastErrorMessage() const;

protected:
    QString m_connectionName;
    bool m_driverAvailable = false;
};

} // namespace olbaflinx::core::storage
