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

class OLBAFLINX_CORE_EXPORT StorageConnection
{
public:
    explicit StorageConnection(const QString &fileName,
                               const QString &driver = QStringLiteral("QSQLCIPHER"));
    ~StorageConnection();

    /**
     * Retrieves the database connection associated with the current StorageConnection instance.
     *
     * @return A QSqlDatabase object representing the database connection.
     */
    [[nodiscard]] QSqlDatabase database() const;

    /**
     * Closes the database connection associated with the current instance of StorageConnection.
     *
     * Note: Invoking this method on an already closed connection or an invalid connection
     * results in no action.
     */
    void close();

    /**
     * Checks if the current storage connection is valid.
     *
     * @return true if the storage connection is valid; false otherwise.
     */
    [[nodiscard]] bool isValid() const;

    /**
     * Tells whether the driver this connection was asked for is registered with
     * Qt. A deployment without the SQLCipher plugin fails to open every file,
     * which without this is indistinguishable from a wrong pass phrase.
     *
     * @return true if the driver is available; false otherwise.
     */
    [[nodiscard]] bool isDriverAvailable() const;

    /**
     * Checks if the storage connection is currently open and usable.
     *
     * @return True if the storage connection is valid and open; otherwise, false.
     */
    [[nodiscard]] bool isOpen() const;

    /**
     * Initiates a database transaction if the connection is open.
     *
     * @return true if the transaction was successfully started, false if the
     *         connection is not open or the transaction initiation failed.
     */
    [[nodiscard]] bool beginTransaction();

    /**
     * Commits the current active transaction in the database.
     *
     * @return true if the transaction was successfully committed; false otherwise.
     */
    [[nodiscard]] bool commitTransaction();

    /**
     * Rolls back the current transaction on the database connection if it is open.
     * If the connection is not open, the rollback operation is not performed.
     *
     * @return True if the transaction was successfully rolled back, false otherwise.
     */
    [[nodiscard]] bool rollbackTransaction();

    /**
     * Retrieves the last error message from the associated database connection.
     * This provides details about the most recent error encountered during
     * database operations.
     *
     * @return A QString containing the last error message. If no errors have occurred,
     *         an empty string is returned.
     */
    [[nodiscard]] QString lastErrorMessage() const;

protected:
    QString m_connectionName;
    bool m_driverAvailable = false;
};

} // namespace olbaflinx::core::storage
