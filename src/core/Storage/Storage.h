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

#ifndef OLBAFLINX_STORAGE_H
#define OLBAFLINX_STORAGE_H

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>
#include <QtCore/QVariant>

#include "Connection/StorageConnection.h"

#include "core/Container.h"
#include "core/Singleton.h"

namespace olbaflinx::core::storage
{

class StoragePrivate;
class Storage: public QObject, public Singleton<Storage>
{

Q_OBJECT
    friend class Singleton<Storage>;

public:
    enum ErrorType
    {
        AccountError,
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
    QString storagePath() const;
    bool changePassword(const QString &oldPassword, const QString &newPassword);

    void storeSetting(const QString &key, const QVariant &value, const QString &group = QString());
    QVariant setting(
        const QString &key,
        const QString &group = QString(),
        const QVariant &defaultValue = QVariant()
    ) const;

    void receiveAccount(quint32 accountId);
    void receiveTransactions(quint32 accountId = 0);
    bool addBalanceToAccount(qint32 balance, quint32 accountId);

public Q_SLOTS:
    void receiveAccounts();
    void storeAccounts(const AccountList &accounts);
    void storeTransactions(const quint32 &accountId, const TransactionList &transactions);

Q_SIGNALS:
    void userChanged(const StorageUser *storageUser);
    void errorOccurred(const QString &message, const Storage::ErrorType errorType);
    void accountsReceived(const AccountList &accounts);
    void transactionsReceived(const TransactionList &transactions);

private:
    QScopedPointer<StoragePrivate> const d_ptr;

protected:
    Storage();
    Q_DISABLE_COPY(Storage)
};

}

Q_DECLARE_METATYPE(olbaflinx::core::storage::Storage::ErrorType)

#endif //OLBAFLINX_STORAGE_H
