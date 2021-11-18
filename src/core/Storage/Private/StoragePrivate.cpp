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

#include <QtSql/QSqlField>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlRecord>

#include "Storage/Storage.h"

#include "StoragePrivate.h"

using namespace olbaflinx::core::storage;

StoragePrivate::StoragePrivate(Storage *storage)
    : q_ptr(storage),
      mIsUserChanged(false),
      mStorageUser(nullptr),
      mConnection(nullptr)
{
}

StoragePrivate::~StoragePrivate()
{
    if (mConnection && mConnection->isOpen()) {
        mConnection->close();
        delete mConnection;
        mConnection = nullptr;
    }

    if (!mStorageUser.isNull()) {
        mStorageUser.clear();
        delete mStorageUser;
        mStorageUser = nullptr;
    }
}

void StoragePrivate::setUser(const StorageUser *storageUser)
{
    if (storageUser == nullptr) {
        Q_EMIT q_ptr->errorOccurred(
            tr("Given user object can't be null!"),
            Storage::UnknownError
        );
        return;
    }

    if (storageUser->filePath().isEmpty()) {
        Q_EMIT q_ptr->errorOccurred(
            tr("Storage file can't be empty!"),
            Storage::FileNotFoundError
        );
        return;
    }

    if (storageUser->password().isEmpty()) {
        Q_EMIT q_ptr->errorOccurred(
            tr("Storage password can't be empty!"),
            Storage::PasswordError
        );
        return;
    }

    mIsUserChanged = false;

    if (!mStorageUser.isNull()) {
        mIsUserChanged = (
            mStorageUser->filePath() != storageUser->filePath()
                && mStorageUser->password() != storageUser->password()
        );
        mStorageUser.clear();
        delete mStorageUser;
        mStorageUser = nullptr;
    }

    QPointer<StorageUser> storageUserPtr(const_cast<StorageUser *>(storageUser));
    storageUserPtr.swap(mStorageUser);
    storageUserPtr.clear();

    delete storageUserPtr;
}

StorageConnection *StoragePrivate::storageConnection()
{
    if (mIsUserChanged) {
        if (mConnection != nullptr) {
            mConnection->close();
            delete mConnection;
            mConnection = nullptr;
        }

        mConnection = new StorageConnection(mStorageUser->filePath());
        return mConnection;
    }

    if (mConnection == nullptr) {
        mConnection = new StorageConnection(mStorageUser->filePath());
    }

    return mConnection;
}

QString StoragePrivate::pragmaKey() const
{
    return QString("PRAGMA key='%1';").arg(quotePassword(mStorageUser->password()));
}

QString StoragePrivate::quotePassword(const QString &password)
{
    QString result = {};

    const int stringLength = password.length();
    for (int a = 0; a < stringLength; ++a) {
        const QChar strPart = password.at(a);
        const int accii = (int) strPart.toLatin1();

        bool noEscapeSeq = (strPart != "'" && strPart != "\"" && strPart != '\\');
        bool isValidAscii = ((accii >= 32 && accii <= 126) || (accii >= 128 && accii <= 255));

        if (noEscapeSeq && isValidAscii) {
            result.append(strPart);
        }
        else {
            switch (accii) {
                case 34: // Char = "
                    result.append(QString(strPart).replace(strPart, "\""));
                    break;
                case 39: // Char = \'
                    result.append(QString(strPart).replace(strPart, "''"));
                    break;
                case 92: // Char = "\"
                    result.append(QString(strPart).replace(strPart, "\\"));
                    break;
                default:
                    break;
            }
        }
    }

    return result;
}

bool StoragePrivate::setupTables(StorageConnection *connection, QStringList &sqlStatements) const
{
    QStringList queries = defaultQueries();

    for (auto &query: sqlStatements)
        queries << query.replace("#", ";").trimmed();

    queries << "VACUUM;";

    QSqlQuery dbQuery(connection->database());

    bool success = true;
    for (const auto &query: qAsConst(queries)) {
        if (query.trimmed().isEmpty()) {
            continue;
        }

        success &= dbQuery.exec(query);
        dbQuery.finish();
    }

    return success;
}

const QStringList StoragePrivate::defaultQueries() const
{
    QStringList queries;
    queries << pragmaKey();
    queries << "PRAGMA auto_vacuum = FULL;";
    queries << "PRAGMA foreign_keys = ON;";
    queries << "PRAGMA encoding = 'UTF-8';";

    return queries;
}

const bool StoragePrivate::checkPassword(
    StorageConnection *connection,
    const QString &password
) const
{
    QSqlQuery dbQuery(connection->database());

    bool success = dbQuery.exec(QString("PRAGMA key='%1';").arg(quotePassword(password)));

    // We can't exec a query without a valid password.
    success &= dbQuery.exec("SELECT COUNT(id) AS ID_COUNT FROM migrations;");

    while (dbQuery.next()) {
        const int count = dbQuery.value("ID_COUNT").toInt();
        success &= (count > 0);
    }

    return success;
}

QMap<QString, QVariant> StoragePrivate::queryToAccountMap(const QSqlQuery &query) const
{
    QMap<QString, QVariant> map = QMap<QString, QVariant>();

    map["type"] = query.record().field("type").value();
    map["uniqueId"] = query.record().field("unique_id").value();
    map["backendName"] = query.record().field("backend_name").value();
    map["ownerName"] = query.record().field("owner_name").value();
    map["accountName"] = query.record().field("account_name").value();
    map["currency"] = query.record().field("currency").value();
    map["memo"] = query.record().field("memo").value();
    map["iban"] = query.record().field("iban").value();
    map["bic"] = query.record().field("bic").value();
    map["country"] = query.record().field("country").value();
    map["bankCode"] = query.record().field("bank_code").value();
    map["bankName"] = query.record().field("bank_name").value();
    map["branchId"] = query.record().field("branch_id").value();
    map["accountNumber"] = query.record().field("account_number").value();
    map["subAccountNumber"] = query.record().field("sub_account_number").value();

    return map;
}

void StoragePrivate::prepareAccountQuery(QSqlQuery &preparedQuery, const Account *account)
{
    preparedQuery.bindValue(":type", account->type());
    preparedQuery.bindValue(":unique_id", account->uniqueId());
    preparedQuery.bindValue(":backend_name", account->backendName());
    preparedQuery.bindValue(":owner_name", account->ownerName());
    preparedQuery.bindValue(":account_name", account->accountName());
    preparedQuery.bindValue(":currency", account->currency());
    preparedQuery.bindValue(":memo", account->memo());
    preparedQuery.bindValue(":iban", account->iban());
    preparedQuery.bindValue(":bic", account->bic());
    preparedQuery.bindValue(":country", account->country());
    preparedQuery.bindValue(":bank_code", account->bankCode());
    preparedQuery.bindValue(":bank_name", account->bankName());
    preparedQuery.bindValue(":branch_id", account->branchId());
    preparedQuery.bindValue(":account_number", account->accountNumber());
    preparedQuery.bindValue(":sub_account_number", account->subAccountNumber());
}

StorageUser *StoragePrivate::storageUser() const
{
    return mStorageUser;
}

