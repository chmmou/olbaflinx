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
#include <QtCore/QDebug>

#include <QtSql/QSqlField>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlRecord>

#include "StoragePrivate.h"

#include "core/Storage/Storage.h"

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

    Q_EMIT q_ptr->userChanged(mStorageUser);

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

QStringList StoragePrivate::defaultQueries() const
{
    QStringList queries;
    queries << pragmaKey();
    queries << "PRAGMA auto_vacuum = FULL;";
    queries << "PRAGMA foreign_keys = ON;";
    queries << "PRAGMA encoding = 'UTF-8';";

    return queries;
}

bool StoragePrivate::checkPassword(StorageConnection *connection, const QString &password) const
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

QMap<QString, QVariant> StoragePrivate::queryToMap(
    const QSqlQuery &query,
    const QueryToMapType &mapType
) const
{
    switch (mapType) {
        case StoragePrivate::AccountMap:
            return queryToAccountMap(query);
        case StoragePrivate::TransactionMap:
            return queryToTransactionMap(query);
        case StoragePrivate::StandingOrderMap:
        default:
            return {};
    }
}

QSqlQuery StoragePrivate::accountToQuery(const Account *account, QSqlQuery &preparedQuery)
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

    return preparedQuery;
}

QSqlQuery StoragePrivate::transactionToQuery(const quint32 &accountId, const Transaction *transaction, QSqlQuery &preparedQuery)
{
    preparedQuery.bindValue(":account_id", accountId);
    preparedQuery.bindValue(":type", (qint32) transaction->type());
    preparedQuery.bindValue(":sub_type", (qint32) transaction->subType());
    preparedQuery.bindValue(":command", (qint32) transaction->command());
    preparedQuery.bindValue(":status", (qint32) transaction->status());
    preparedQuery.bindValue(":unique_account_id", transaction->uniqueAccountId());
    preparedQuery.bindValue(":unique_id", transaction->uniqueId());
    preparedQuery.bindValue(":ref_unique_id", transaction->refUniqueId());
    preparedQuery.bindValue(":id_for_application", transaction->idForApplication());
    preparedQuery.bindValue(":string_id_for_application", transaction->stringIdForApplication());
    preparedQuery.bindValue(":session_id", transaction->sessionId());
    preparedQuery.bindValue(":group_id", transaction->groupId());
    preparedQuery.bindValue(":fi_id", transaction->fiId());
    preparedQuery.bindValue(":local_iban", transaction->localIban());
    preparedQuery.bindValue(":local_bic", transaction->localBic());
    preparedQuery.bindValue(":local_country", transaction->localCountry());
    preparedQuery.bindValue(":local_bank_code", transaction->localBankCode());
    preparedQuery.bindValue(":local_branch_id", transaction->localBranchId());
    preparedQuery.bindValue(":local_account_number", transaction->localAccountNumber());
    preparedQuery.bindValue(":local_suffix", transaction->localSuffix());
    preparedQuery.bindValue(":local_name", transaction->localName());
    preparedQuery.bindValue(":remote_country", transaction->remoteCountry());
    preparedQuery.bindValue(":remote_bank_code", transaction->remoteBankCode());
    preparedQuery.bindValue(":remote_branch_id", transaction->remoteBranchId());
    preparedQuery.bindValue(":remote_account_number", transaction->remoteAccountNumber());
    preparedQuery.bindValue(":remote_suffix", transaction->remoteSuffix());
    preparedQuery.bindValue(":remote_iban", transaction->remoteIban());
    preparedQuery.bindValue(":remote_bic", transaction->remoteBic());
    preparedQuery.bindValue(":remote_name", transaction->remoteName());
    preparedQuery.bindValue(":date", transaction->date());
    preparedQuery.bindValue(":valuta_date", transaction->valutaDate());
    preparedQuery.bindValue(":value", transaction->value());
    preparedQuery.bindValue(":currency", transaction->currency());
    preparedQuery.bindValue(":fees", transaction->fees());
    preparedQuery.bindValue(":transaction_code", transaction->transactionCode());
    preparedQuery.bindValue(":transaction_text", transaction->transactionText());
    preparedQuery.bindValue(":transaction_key", transaction->transactionKey());
    preparedQuery.bindValue(":text_key", transaction->textKey());
    preparedQuery.bindValue(":primanota", transaction->primanota());
    preparedQuery.bindValue(":purpose", transaction->purpose());
    preparedQuery.bindValue(":category", transaction->category());
    preparedQuery.bindValue(":customer_reference", transaction->customerReference());
    preparedQuery.bindValue(":bank_reference", transaction->bankReference());
    preparedQuery.bindValue(":end_to_end_reference", transaction->endToEndReference());
    preparedQuery.bindValue(":creditor_scheme_id", transaction->creditorSchemeId());
    preparedQuery.bindValue(":originator_id", transaction->originatorId());
    preparedQuery.bindValue(":mandate_id", transaction->mandateId());
    preparedQuery.bindValue(":mandate_date", transaction->mandateDate());
    preparedQuery.bindValue(":mandate_debitor_name", transaction->mandateDebitorName());
    preparedQuery.bindValue(":original_creditor_scheme_id", transaction->originalCreditorSchemeId());
    preparedQuery.bindValue(":original_mandate_id", transaction->originalMandateId());
    preparedQuery.bindValue(":original_creditor_name", transaction->originalCreditorName());
    preparedQuery.bindValue(":sequence", (qint32) transaction->sequence());
    preparedQuery.bindValue(":charge", (qint32) transaction->charge());
    preparedQuery.bindValue(":remote_addr_street", transaction->remoteAddrStreet());
    preparedQuery.bindValue(":remote_addr_zipcode", transaction->remoteAddrZipcode());
    preparedQuery.bindValue(":remote_addr_city", transaction->remoteAddrCity());
    preparedQuery.bindValue(":remote_addr_phone", transaction->remoteAddrPhone());
    preparedQuery.bindValue(":period", (qint32) transaction->period());
    preparedQuery.bindValue(":cycle", transaction->cycle());
    preparedQuery.bindValue(":execution_day", transaction->executionDay());
    preparedQuery.bindValue(":first_date", transaction->firstDate());
    preparedQuery.bindValue(":last_date", transaction->lastDate());
    preparedQuery.bindValue(":next_date", transaction->nextDate());
    preparedQuery.bindValue(":unit_id", transaction->unitId());
    preparedQuery.bindValue(":unit_id_name_space", transaction->unitIdNameSpace());
    preparedQuery.bindValue(":ticker_symbol", transaction->tickerSymbol());
    preparedQuery.bindValue(":units", transaction->units());
    preparedQuery.bindValue(":unit_price_value", transaction->unitPriceValue());
    preparedQuery.bindValue(":unit_price_date", transaction->unitPriceDate());
    preparedQuery.bindValue(":commission_value", transaction->commissionValue());
    preparedQuery.bindValue(":memo", transaction->memo());
    preparedQuery.bindValue(":hash", transaction->hash());

    return preparedQuery;
}

StorageUser *StoragePrivate::storageUser() const
{
    return mStorageUser;
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

QMap<QString, QVariant> StoragePrivate::queryToTransactionMap(const QSqlQuery &query) const
{
    QMap<QString, QVariant> map = QMap<QString, QVariant>();

    map["accountId"] = query.record().field("account_id").value();
    map["type"] = query.record().field("type").value();
    map["subType"] = query.record().field("sub_type").value();
    map["command"] = query.record().field("command").value();
    map["status"] = query.record().field("status").value();
    map["uniqueAccountId"] = query.record().field("unique_account_id").value();
    map["uniqueId"] = query.record().field("unique_id").value();
    map["refUniqueId"] = query.record().field("ref_unique_id").value();
    map["idForApplication"] = query.record().field("id_for_application").value();
    map["stringIdForApplication"] = query.record().field("string_id_for_application").value();
    map["sessionId"] = query.record().field("session_id").value();
    map["groupId"] = query.record().field("group_id").value();
    map["fiId"] = query.record().field("fi_id").value();
    map["localIban"] = query.record().field("local_iban").value();
    map["localBic"] = query.record().field("local_bic").value();
    map["localCountry"] = query.record().field("local_country").value();
    map["localBankCode"] = query.record().field("local_bank_code").value();
    map["localBranchId"] = query.record().field("local_branch_id").value();
    map["localAccountNumber"] = query.record().field("local_account_number").value();
    map["localSuffix"] = query.record().field("local_suffix").value();
    map["localName"] = query.record().field("local_name").value();
    map["remoteCountry"] = query.record().field("remote_country").value();
    map["remoteBankCode"] = query.record().field("remote_bank_code").value();
    map["remoteBranchId"] = query.record().field("remote_branch_id").value();
    map["remoteAccountNumber"] = query.record().field("remote_account_number").value();
    map["remoteSuffix"] = query.record().field("remote_suffix").value();
    map["remoteIban"] = query.record().field("remote_iban").value();
    map["remoteBic"] = query.record().field("remote_bic").value();
    map["remoteName"] = query.record().field("remote_name").value();
    map["date"] = query.record().field("date").value();
    map["valutaDate"] = query.record().field("valuta_date").value();
    map["value"] = query.record().field("value").value();
    map["currency"] = query.record().field("currency").value();
    map["fees"] = query.record().field("fees").value();
    map["transactionCode"] = query.record().field("transaction_code").value();
    map["transactionText"] = query.record().field("transaction_text").value();
    map["transactionKey"] = query.record().field("transaction_key").value();
    map["textKey"] = query.record().field("text_key").value();
    map["primanota"] = query.record().field("primanota").value();
    map["purpose"] = query.record().field("purpose").value();
    map["category"] = query.record().field("category").value();
    map["customerReference"] = query.record().field("customer_reference").value();
    map["bankReference"] = query.record().field("bank_reference").value();
    map["endToEndReference"] = query.record().field("end_to_end_reference").value();
    map["creditorSchemeId"] = query.record().field("creditor_scheme_id").value();
    map["originatorId"] = query.record().field("originator_id").value();
    map["mandateId"] = query.record().field("mandate_id").value();
    map["mandateDate"] = query.record().field("mandate_date").value();
    map["mandateDebitorName"] = query.record().field("mandate_debitor_name").value();
    map["originalCreditorSchemeId"] = query.record().field("original_creditor_scheme_id").value();
    map["originalMandateId"] = query.record().field("original_mandate_id").value();
    map["originalCreditorName"] = query.record().field("original_creditor_name").value();
    map["sequence"] = query.record().field("sequence").value();
    map["charge"] = query.record().field("charge").value();
    map["remoteAddrStreet"] = query.record().field("remote_addr_street").value();
    map["remoteAddrZipcode"] = query.record().field("remote_addr_zipcode").value();
    map["remoteAddrCity"] = query.record().field("remote_addr_city").value();
    map["remoteAddrPhone"] = query.record().field("remote_addr_phone").value();
    map["period"] = query.record().field("period").value();
    map["cycle"] = query.record().field("cycle").value();
    map["executionDay"] = query.record().field("execution_day").value();
    map["firstDate"] = query.record().field("first_date").value();
    map["lastDate"] = query.record().field("last_date").value();
    map["nextDate"] = query.record().field("next_date").value();
    map["unitId"] = query.record().field("unit_id").value();
    map["unitIdNameSpace"] = query.record().field("unit_id_name_space").value();
    map["tickerSymbol"] = query.record().field("ticker_symbol").value();
    map["units"] = query.record().field("units").value();
    map["unitPriceValue"] = query.record().field("unit_price_value").value();
    map["unitPriceDate"] = query.record().field("unit_price_date").value();
    map["commissionValue"] = query.record().field("commission_value").value();
    map["memo"] = query.record().field("memo").value();
    map["hash"] = query.record().field("hash").value();

    return map;
}

bool StoragePrivate::accountExists(const quint32 &accountId, QSqlQuery &query) const
{
    query.prepare("SELECT 1 FROM account WHERE unique_id = :unique_id");
    query.bindValue(":unique_id", accountId);
    query.exec();
    query.next();

    return (query.record().count() > 0);
}
