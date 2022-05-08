/**
* Copyright (C) 2022, Alexander Saal <developer@olbaflinx.chm-projects.de>
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

#ifndef OLBAFLINX_VAULT_STORAGE_H
#define OLBAFLINX_VAULT_STORAGE_H

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>

#include "core/Container.h"
#include "core/Singleton.h"

namespace olbaflinx::core::storage {

using namespace account;
using namespace transaction;

class VaultStorage : public QObject, public Singleton<VaultStorage>
{
    Q_OBJECT
    friend class Singleton<VaultStorage>;

public:
    ~VaultStorage() override;

    void setDatabaseKey(const QString &dbFileName, const QString &key);
    void initialize(bool initializeSchema = false);
    bool isStorageValid() const;
    bool changeKey(const QString &oldKey, const QString &newKey) const;
    QString storagePath() const;
    void close();

    void storeSetting(const QString &key, const QVariant &value, const QString &group = QString());
    QVariant setting(const QString &key,
                     const QString &group = QString(),
                     const QVariant &defaultValue = QVariant()) const;

    void addAccount(const Account *account);
    void addAccounts(const AccountList &accounts);
    AccountList accounts();
    AccountIds accountIds();

    void addTransaction(const quint32 &accountId, const Transaction *transaction);
    void addTransactions(const quint32 &accountId, const TransactionList &transactions);
    TransactionList transactions(const quint32 &accountId,
                                 const qint32 &limit = 50,
                                 const qint32 &offset = 0);

Q_SIGNALS:
    void progress(const qreal progress);

protected:
    class Private;
    QScopedPointer<Private> d_ptr;

    VaultStorage();
    Q_DISABLE_COPY(VaultStorage)
};

} // namespace olbaflinx::core::storage

#endif //OLBAFLINX_VAULT_STORAGE_H
