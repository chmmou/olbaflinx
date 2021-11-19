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
#ifndef OLBAFLINX_STORAGEPRIVATE_H
#define OLBAFLINX_STORAGEPRIVATE_H

#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariant>
#include <QtCore/QPointer>

#include <QtSql/QSqlQuery>

#include "Container.h"
#include "Storage/Connection/StorageConnection.h"

namespace olbaflinx::core::storage
{

using namespace core;
using namespace connection;

class Storage;
class StoragePrivate: public QObject
{
    Q_DISABLE_COPY(StoragePrivate)
    Q_DECLARE_PUBLIC(Storage)

Q_OBJECT

public:
    explicit StoragePrivate(Storage *storage);
    ~StoragePrivate() override;

    void setUser(const StorageUser *storageUser);

    [[nodiscard]] StorageConnection *storageConnection();
    [[nodiscard]] QString pragmaKey() const;
    [[nodiscard]] static QString quotePassword(const QString &string);
    [[nodiscard]] bool setupTables(StorageConnection *connection, QStringList &sqlStatements) const;
    [[nodiscard]] QStringList defaultQueries() const;
    [[nodiscard]] bool checkPassword(
        StorageConnection *connection,
        const QString &password
    ) const;

    [[nodiscard]] QMap<QString, QVariant> queryToAccountMap(const QSqlQuery &query) const;
    void prepareAccountQuery(QSqlQuery &preparedQuery, const Account *account);

    [[nodiscard]] StorageUser *storageUser() const;

private:
    Storage *const q_ptr;
    StorageConnection *mConnection;
    QPointer<StorageUser> mStorageUser;
    bool mIsUserChanged;
};

}

#endif // OLBAFLINX_STORAGEPRIVATE_H
