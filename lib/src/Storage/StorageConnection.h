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

#ifndef OLBAFLINX_CORE_STORAGECONNECTION_H
#define OLBAFLINX_CORE_STORAGECONNECTION_H

#include "OlbaFlinxCore.h"

#include <QtSql/QSqlDatabase>

namespace olbaflinx::core::storage {

class OLBAFLINX_CORE_EXPORT StorageConnection
{
public:
    explicit StorageConnection(const QString &fileName, const QString &driver = "QSQLCIPHER");
    ~StorageConnection();

    /**
     * Retrieves the database connection associated with the current StorageConnection instance.
     *
     * @return A QSqlDatabase object representing the database connection.
     */
    QSqlDatabase database();

    /**
     * Closes the database connection associated with the current instance of StorageConnection.
     *
     * This method performs the following:
     * - Closes the active database connection.
     * - Removes the database connection using the specified connection name.
     *
     * It ensures that the storage connection is properly closed and cleaned up.
     *
     * Note: Invoking this method on an already closed connection or an invalid connection
     * results in no action.
     */
    void close();

    /**
     * Checks if the current storage connection is valid.
     *
     * This function validates the underlying database connection
     * by verifying its consistency and whether it has been properly initialized.
     *
     * @return true if the storage connection is valid; false otherwise.
     */
    bool isValid();

    /**
     * Checks if the storage connection is currently open and usable.
     *
     * This method validates that the storage connection is both valid and open.
     * It first ensures that the database connection is valid using the `isValid` method,
     * and then checks if the database object is currently open.
     *
     * @return True if the storage connection is valid and open; otherwise, false.
     */
    bool isOpen();

    /**
     * Initiates a database transaction if the connection is open.
     *
     * A transaction allows multiple operations to be performed as a single unit of work,
     * ensuring that all operations either succeed or fail together. This method checks
     * if the connection to the database is open and, if so, initiates a transaction
     * using the underlying database.
     *
     * @return true if the transaction was successfully started, false if the
     *         connection is not open or the transaction initiation failed.
     */
    bool beginTransaction();

    /**
     * Commits the current active transaction in the database.
     *
     * This method attempts to commit the transaction on the associated database connection
     * if the connection is currently open. If the connection is not open, the commit operation
     * does not occur, and the method returns false.
     *
     * @return true if the transaction was successfully committed; false otherwise.
     */
    bool commitTransaction();

    /**
     * Rolls back the current transaction on the database connection if it is open.
     * If the connection is not open, the rollback operation is not performed.
     *
     * @return True if the transaction was successfully rolled back, false otherwise.
     */
    bool rollbackTransaction();

    /**
     * Retrieves the last error message from the associated database connection.
     * This provides details about the most recent error encountered during
     * database operations.
     *
     * @return A QString containing the last error message. If no errors have occurred,
     *         an empty string is returned.
     */
    QString lastErrorMessage();

protected:
    QString m_connectionName;
};

} // namespace olbaflinx::core::storage::connection

#endif //OLBAFLINX_CORE_STORAGECONNECTION_H
