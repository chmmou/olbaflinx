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
#include <QtCore/QFile>
#include <QtCore/QStringList>
#include <QtCore/QSettings>
#include <QtCore/QStandardPaths>

#include <QtSql/QSqlDriver>
#include <QtSql/QSqlError>
#include <QtSql/QSqlField>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlRecord>
#include <QtSql/QSqlResult>

#include "SingleApplication/SingleApplication.h"

#include "Storage.h"
#include "Private/StoragePrivate.h"

using namespace olbaflinx::core;
using namespace olbaflinx::core::storage;

QSettings *mSettings;

Storage::Storage()
    : QObject(nullptr),
      d_ptr(new StoragePrivate(this))
{
    mSettings = new QSettings(
        QSettings::IniFormat,
        QSettings::UserScope,
        SingleApplication::organizationName(),
        SingleApplication::applicationName(),
        this
    );
}

Storage::~Storage()
{
    if (mSettings != nullptr) {
        mSettings->sync();
        mSettings->deleteLater();
    }

    if (!d_ptr.isNull()) {
        d_ptr->deleteLater();
    }
}

void Storage::setUser(const StorageUser *storageUser)
{
    d_ptr->setUser(storageUser);
}

bool Storage::initialize()
{
    const auto conn = d_ptr->storageConnection();
    if (!conn->isOpen()) {
        Q_EMIT errorOccurred(conn->lastErrorMessage(), Storage::ConnectionError);
        return false;
    }

    QFile storageFile(":/app/olbaflinx-storage");
    if (!storageFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        Q_EMIT errorOccurred(storageFile.errorString(), Storage::StorageNotOpen);
        return false;
    }

    QStringList sqlStatements = QTextStream(&storageFile).readAll().split(';');

    bool ok = d_ptr->setupTables(conn, sqlStatements);

    sqlStatements.clear();

    return ok;
}

bool Storage::isInitialized()
{
    QFile storageFile(d_ptr->storageUser()->filePath());
    if (storageFile.size() == 0 || !storageFile.exists()) {
        return false;
    }

    const auto conn = d_ptr->storageConnection();

    QStringList queries;
    queries << d_ptr->pragmaKey();
    queries << "SELECT migrated FROM migrations;";

    QSqlQuery dbQuery(conn->database());

    QStringList tables;
    bool success = true;
    for (const auto &query: qAsConst(queries)) {
        if (query.trimmed().isEmpty()) {
            continue;
        }

        success &= dbQuery.exec(query);
        if (query.contains("migrations")) {
            while (dbQuery.next()) {
                success &= dbQuery.value(0).toBool();
            }

        }
        dbQuery.finish();
    }

    return success;
}

bool Storage::checkIntegrity()
{
    const auto conn = d_ptr->storageConnection();
    QSqlQuery dbQuery(conn->database());

    bool success = dbQuery.exec(d_ptr->pragmaKey());

    success &= dbQuery.exec("PRAGMA integrity_check;");
    if (!success) {
        Q_EMIT errorOccurred(dbQuery.lastError().text(), Storage::IntegrityError);
        return false;
    }

    success &= dbQuery.exec("PRAGMA foreign_key_check;");
    if (!success) {
        Q_EMIT errorOccurred(dbQuery.lastError().text(), Storage::ForeignKeyError);
        return false;
    }

    return success;
}

bool Storage::compress()
{
    const auto conn = d_ptr->storageConnection();
    QSqlQuery dbQuery(conn->database());

    bool success = dbQuery.exec(d_ptr->pragmaKey());
    success &= dbQuery.exec("VACUUM;");

    if (!success) {
        Q_EMIT errorOccurred(dbQuery.lastError().text(), Storage::IntegrityError);
        return false;
    }

    return success;
}

QString Storage::storagePath() const
{
    QString path = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    return QString("%1/%2").arg(path, SingleApplication::organizationName());
}

bool Storage::changePassword(const QString &oldPassword, const QString &newPassword)
{
    const auto conn = d_ptr->storageConnection();

    // Check old password
    bool ok = d_ptr->checkPassword(conn, oldPassword);
    if (!ok) {
        Q_EMIT errorOccurred(
            tr("The entered password is not correct!"),
            Storage::PasswordError
        );
        return false;
    }

    if (oldPassword == newPassword) {
        Q_EMIT errorOccurred(
            tr("The new password cannot be the same as the old one."),
            Storage::PasswordError
        );
        return false;
    }

    QSqlQuery dbQuery(conn->database());
    bool success = dbQuery.exec(
        QString("PRAGMA key='%1';").arg(StoragePrivate::quotePassword(oldPassword))
    );

    if (!success) {
        Q_EMIT errorOccurred(dbQuery.lastError().text(), Storage::PasswordError);
        return false;
    }

    success = dbQuery.exec(
        QString("PRAGMA rekey='%1';").arg(
            StoragePrivate::quotePassword(newPassword)
        )
    );
    if (!success) {
        Q_EMIT errorOccurred(dbQuery.lastError().text(), Storage::PasswordError);
        return false;
    }

    // If the new password working?
    success = d_ptr->checkPassword(conn, newPassword);
    if (!success) {
        Q_EMIT errorOccurred(
            tr("The entered password is not correct!"),
            Storage::PasswordError
        );
        return false;
    }

    d_ptr->storageUser()->setPassword(newPassword);

    return success;
}

void Storage::receiveAccount(const quint32 accountId)
{
    if (accountId <= 0) {
        Q_EMIT errorOccurred(
            tr("Given unique account id is wrong."),
            Storage::AccountError
        );
        return;
    }

    const auto conn = d_ptr->storageConnection();

    QSqlQuery dbQuery(conn->database());
    bool success = dbQuery.exec(d_ptr->pragmaKey());

    dbQuery.prepare("SELECT * from accounts WHERE unique_id = :unique_id;");
    dbQuery.bindValue(":unique_id", accountId);
    success &= dbQuery.exec();

    if (!success) {
        Q_EMIT errorOccurred(
            dbQuery.lastError().text(),
            Storage::StatementError
        );
        return;
    }

    if (dbQuery.record().count() == 0) {
        Q_EMIT errorOccurred(
            tr("No accounts found with unique account id."),
            Storage::AccountError
        );
        return;
    }

    dbQuery.next();

    const QMap<QString, QVariant> map = d_ptr->queryToAccountMap(dbQuery);

    AccountList accounts;
    accounts << Account::create(map);

    Q_EMIT accountsReceived(accounts);

    qDeleteAll(accounts);
    accounts.clear();
}

void Storage::receiveAccounts()
{
    AccountList accounts = {};

    const auto conn = d_ptr->storageConnection();

    QSqlQuery dbQuery(conn->database());
    dbQuery.exec(d_ptr->pragmaKey());

    bool success = dbQuery.exec("SELECT * from accounts;");
    if (!success) {
        Q_EMIT errorOccurred(
            dbQuery.lastError().text(),
            Storage::AccountError
        );
        return;
    }

    while (dbQuery.next()) {
        const QMap<QString, QVariant> map = d_ptr->queryToMap(dbQuery, StoragePrivate::AccountMap);
        accounts << Account::create(map);
    }
    dbQuery.finish();

    Q_EMIT accountsReceived(accounts);

    qDeleteAll(accounts);
    accounts.clear();
}

void Storage::receiveTransactions(quint32 accountId)
{
    TransactionList transactions = {};

    const auto conn = d_ptr->storageConnection();

    QSqlQuery dbQuery(conn->database());
    bool success = dbQuery.exec(d_ptr->pragmaKey());

    dbQuery.prepare("SELECT * from transactions WHERE account_id = :account_id;");
    dbQuery.bindValue(":account_id", accountId);
    success &= dbQuery.exec();

    if (!success) {
        Q_EMIT errorOccurred(
            dbQuery.lastError().text(),
            Storage::TransactionError
        );
        return;
    }

    while (dbQuery.next()) {
        const QMap<QString, QVariant> map = d_ptr->queryToMap(dbQuery, StoragePrivate::TransactionMap);
        transactions << Transaction::create(map);
    }
    dbQuery.finish();

    Q_EMIT transactionsReceived(transactions);
}

bool Storage::addBalanceToAccount(qint32 balance, quint32 accountId)
{
    auto conn = d_ptr->storageConnection();

    QSqlQuery dbQuery(conn->database());
    dbQuery.exec(d_ptr->pragmaKey());

    if (accountId < 1) {
        return false;
    }

    bool success = true;
    const bool exists = d_ptr->accountExists(accountId, dbQuery);

    if (exists) {
        dbQuery.prepare("UPDATE accounts SET balance = :balance WHERE unique_id = :unique_id");
        dbQuery.bindValue(":balance", balance);
        dbQuery.bindValue(":unique_id", accountId);

        success &= dbQuery.exec();

        dbQuery.finish();
    }

    return success;
}

void Storage::storeAccounts(const AccountList &accounts)
{
    if (accounts.empty()) {
        Q_EMIT errorOccurred(
            tr("Given accounts can't be empty"),
            Storage::AccountError
        );
        return;
    }

    auto conn = d_ptr->storageConnection();

    QSqlQuery dbQuery(conn->database());
    dbQuery.exec(d_ptr->pragmaKey());

    for (const auto account: accounts) {
        dbQuery.prepare(
            "REPLACE INTO accounts (type, unique_id, backend_name, owner_name, account_name, currency, memo, iban, bic, country, bank_code, bank_name, branch_id, account_number, sub_account_number) "
            "VALUES (:type, :unique_id, :backend_name, :owner_name, :account_name, :currency, :memo, :iban, :bic, :country, :bank_code, :bank_name, :branch_id, :account_number, :sub_account_number);"
        );
        dbQuery = d_ptr->accountToQuery(account, dbQuery);
        dbQuery.exec();
        dbQuery.finish();
    }

    checkIntegrity();
    compress();
}

void Storage::storeTransactions(const quint32 &accountId, const TransactionList &transactions)
{
    if (transactions.empty()) {
        Q_EMIT errorOccurred(
            tr("Given transaction can't be empty"),
            Storage::TransactionError
        );
        return;
    }

    auto conn = d_ptr->storageConnection();
    bool success = true;

    QSqlQuery dbQuery(conn->database());
    success &= dbQuery.exec(d_ptr->pragmaKey());

    for (const auto transation: transactions) {
        dbQuery.prepare(
            "REPLACE INTO transactions (account_id, `type`, sub_type, command, status, unique_account_id, unique_id, ref_unique_id, id_for_application, string_id_for_application, session_id, group_id, fi_id, local_iban, local_bic, local_country, local_bank_code, local_branch_id, local_account_number, local_suffix, local_name, remote_country, remote_bank_code, remote_branch_id, remote_account_number, remote_suffix, remote_iban, remote_bic, remote_name, `date`, valuta_date, value, currency, fees, transaction_code, transaction_text, transaction_key, text_key, primanota, purpose, `category`, customer_reference, bank_reference, end_to_end_reference, creditor_scheme_id, originator_id, mandate_id, mandate_date, mandate_debitor_name, original_creditor_scheme_id, original_mandate_id, original_creditor_name, `sequence`, charge, remote_addr_street, remote_addr_zipcode, remote_addr_city, remote_addr_phone, period, `cycle`, execution_day, first_date, last_date, next_date, unit_id, unit_id_name_space, ticker_symbol, units, unit_price_value, unit_price_date, commission_value, memo, `hash`) "
            "VALUES (:account_id, :type, :sub_type, :command, :status, :unique_account_id, :unique_id, :ref_unique_id, :id_for_application, :string_id_for_application, :session_id, :group_id, :fi_id, :local_iban, :local_bic, :local_country, :local_bank_code, :local_branch_id, :local_account_number, :local_suffix, :local_name, :remote_country, :remote_bank_code, :remote_branch_id, :remote_account_number, :remote_suffix, :remote_iban, :remote_bic, :remote_name, :date, :valuta_date, :value, :currency, :fees, :transaction_code, :transaction_text, :transaction_key, :text_key, :primanota, :purpose, :category, :customer_reference, :bank_reference, :end_to_end_reference, :creditor_scheme_id, :originator_id, :mandate_id, :mandate_date, :mandate_debitor_name, :original_creditor_scheme_id, :original_mandate_id, :original_creditor_name, :sequence, :charge, :remote_addr_street, :remote_addr_zipcode, :remote_addr_city, :remote_addr_phone, :period, :cycle, :execution_day, :first_date, :last_date, :next_date, :unit_id, :unit_id_name_space, :ticker_symbol, :units, :unit_price_value, :unit_price_date, :commission_value, :memo, :hash);"
        );
        dbQuery = d_ptr->transactionToQuery(accountId, transation, dbQuery);
        success &= dbQuery.exec();

        if (!success) {
            Q_EMIT errorOccurred(
                dbQuery.lastError().text(),
                Storage::TransactionError
            );
            dbQuery.finish();
            return;
        }

        dbQuery.finish();
    }

    checkIntegrity();
    compress();
}

void Storage::storeSetting(const QString &key, const QVariant &value, const QString &group)
{
    if (!group.isEmpty()) {
        mSettings->beginGroup(group);
    }

    mSettings->setValue(key, value);

    if (!group.isEmpty()) {
        mSettings->endGroup();
    }

    mSettings->sync();
}

QVariant Storage::setting(
    const QString &key,
    const QString &group,
    const QVariant &defaultValue
) const
{
    if (!group.isEmpty()) {
        mSettings->beginGroup(group);
    }

    QVariant value = mSettings->value(key, defaultValue);

    if (!group.isEmpty()) {
        mSettings->endGroup();
    }

    return value;
}
