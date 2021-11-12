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

#ifndef OLBAFLINX_STORAGE_H
#define OLBAFLINX_STORAGE_H

#include <QtCore/QObject>
#include <QtCore/QVariant>

#include "Connection/StorageConnection.h"

#include "core/Container.h"
#include "core/Singleton.h"

namespace olbaflinx::core::storage
{

using namespace core;
using namespace connection;

class Storage: public QObject, public Singleton<Storage>
{

Q_OBJECT
    friend class Singleton<Storage>;

public:
    enum ErrorType
    {
        ConnectionError,
        StatementError,
        TransactionError,
        StorageNotOpen,
        IntegrityError,
        ForeignKeyError,
        PasswordError,
        FileNotFoundError,

        // Last
        UnknownError = -1
    };
    Q_ENUM(ErrorType)

    ~Storage() override;

    void setUser(const StorageUser *storageUser);
    bool initialize();
    bool isInitialized();
    bool checkIntegrity();
    bool compress();
    bool changePassword(const QString &oldPassword, const QString &newPassword);

    void storeSetting(const QString &group, const QString &key, const QVariant &value);
    QVariant setting(const QString &group, const QString &key, const QVariant &defaultValue = QVariant()) const;

Q_SIGNALS:
    void errorOccurred(const QString &message, const Storage::ErrorType errorType);

private:
    StorageConnection *connection();
    const QString pragmaKey() const;
    const QString quote(const QString &string) const;
    bool setupTables(StorageConnection *connection, QStringList &sqlStatements) const;
    const QStringList defaultQueries() const;
    const bool checkPassword(StorageConnection *connection, const QString &password) const;

protected:
    Storage();
    Q_DISABLE_COPY(Storage)

};

}

Q_DECLARE_METATYPE(olbaflinx::core::storage::Storage::ErrorType)

#endif //OLBAFLINX_STORAGE_H
