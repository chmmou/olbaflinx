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

#include <QtConcurrent/QtConcurrent>
#include <QtCore/QCryptographicHash>
#include <QtCore/QFile>
#include <QtCore/QFutureWatcher>
#include <QtCore/QList>
#include <QtCore/QTextStream>
#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlRecord>

#include <QtCore/QSettings>

#include "core/SingleApplication/SingleApplication.h"
#include "core/Storage/Connection/StorageConnection.h"
#include "VaultStorage.h"

using namespace olbaflinx::core::storage;
using namespace olbaflinx::core::storage::connection;

class VaultStorage::Private
{
public:
    explicit Private()
        : m_settings(Q_NULLPTR)
        , m_connection(Q_NULLPTR)
        , m_filePath("")
        , m_key("")
    { }

    ~Private()
    {
        m_settings->sync();
        delete m_settings;

        m_connection->close();
        delete m_connection;
    }

    void initialize(const bool initializeSchema = false)
    {
        if (m_connection != Q_NULLPTR) {
            const auto currentDatabaseName = m_connection->database().databaseName();
            if (currentDatabaseName.toLower() != m_filePath.toLower()) {
                if (m_connection->isOpen()) {
                    m_connection->close();
                }
                delete m_connection;
                m_connection = Q_NULLPTR;
            } else {
                if (initializeSchema) {
                    setupTables();
                }
                return;
            }
        }

        m_connection = new StorageConnection(m_filePath);
        if (!m_connection->isOpen()) {
            return;
        }

        if (initializeSchema) {
            setupTables();
        }
    }

    bool isStorageValid()
    {
        if (!m_connection->isOpen()) {
            return false;
        }

        if (m_key.isEmpty()) {
            return false;
        }

        QSqlQuery dbQuery = databaseQuery();
        bool counted = dbQuery.exec("SELECT COUNT(id) AS ID_COUNT FROM migrations;");
        if (!counted) {
            return false;
        }

        while (counted && dbQuery.next()) {
            const int count = dbQuery.value("ID_COUNT").toInt();
            counted &= (count >= 0);
        }

        return counted;
    }

    void close()
    {
        if (m_connection) {
            m_connection->close();
            delete m_connection;
            m_connection = Q_NULLPTR;
        }
    }

    QSqlQuery databaseQuery()
    {
        QSqlQuery dbQuery(m_connection->database());
        dbQuery.exec(QString("PRAGMA key='%1';").arg(escapeKey(m_key)));
        return dbQuery;
    }

    QSqlQuery accountQuery(const Account *account, QSqlQuery &query)
    {
        query.prepare(StorageSqlAccountInsertQuery);
        query.bindValue(":type", account->type());
        query.bindValue(":unique_id", account->uniqueId());
        query.bindValue(":backend_name", account->backendName());
        query.bindValue(":owner_name", account->ownerName());
        query.bindValue(":account_name", account->accountName());
        query.bindValue(":currency", account->currency());
        query.bindValue(":memo", account->memo());
        query.bindValue(":iban", account->iban());
        query.bindValue(":bic", account->bic());
        query.bindValue(":country", account->country());
        query.bindValue(":bank_code", account->bankCode());
        query.bindValue(":bank_name", account->bankName());
        query.bindValue(":branch_id", account->branchId());
        query.bindValue(":account_number", account->accountNumber());
        query.bindValue(":sub_account_number", account->subAccountNumber());

        return query;
    }

    QSqlQuery transactionQuery(const quint32 &accountId,
                               const Transaction *transaction,
                               QSqlQuery &query)
    {
        query.prepare(StorageSqlTransactionInsertQuery);
        query.bindValue(":account_id", accountId);
        query.bindValue(":type", (qint32) transaction->type());
        query.bindValue(":sub_type", (qint32) transaction->subType());
        query.bindValue(":command", (qint32) transaction->command());
        query.bindValue(":status", (qint32) transaction->status());
        query.bindValue(":unique_account_id", transaction->uniqueAccountId());
        query.bindValue(":unique_id", transaction->uniqueId());
        query.bindValue(":ref_unique_id", transaction->refUniqueId());
        query.bindValue(":id_for_application", transaction->idForApplication());
        query.bindValue(":string_id_for_application", transaction->stringIdForApplication());
        query.bindValue(":session_id", transaction->sessionId());
        query.bindValue(":group_id", transaction->groupId());
        query.bindValue(":fi_id", transaction->fiId());
        query.bindValue(":local_iban", transaction->localIban());
        query.bindValue(":local_bic", transaction->localBic());
        query.bindValue(":local_country", transaction->localCountry());
        query.bindValue(":local_bank_code", transaction->localBankCode());
        query.bindValue(":local_branch_id", transaction->localBranchId());
        query.bindValue(":local_account_number", transaction->localAccountNumber());
        query.bindValue(":local_suffix", transaction->localSuffix());
        query.bindValue(":local_name", transaction->localName());
        query.bindValue(":remote_country", transaction->remoteCountry());
        query.bindValue(":remote_bank_code", transaction->remoteBankCode());
        query.bindValue(":remote_branch_id", transaction->remoteBranchId());
        query.bindValue(":remote_account_number", transaction->remoteAccountNumber());
        query.bindValue(":remote_suffix", transaction->remoteSuffix());
        query.bindValue(":remote_iban", transaction->remoteIban());
        query.bindValue(":remote_bic", transaction->remoteBic());
        query.bindValue(":remote_name", transaction->remoteName());
        query.bindValue(":date", transaction->date());
        query.bindValue(":valuta_date", transaction->valutaDate());
        query.bindValue(":value", transaction->value());
        query.bindValue(":currency", transaction->currency());
        query.bindValue(":fees", transaction->fees());
        query.bindValue(":transaction_code", transaction->transactionCode());
        query.bindValue(":transaction_text", transaction->transactionText());
        query.bindValue(":transaction_key", transaction->transactionKey());
        query.bindValue(":text_key", transaction->textKey());
        query.bindValue(":primanota", transaction->primanota());
        query.bindValue(":purpose", transaction->purpose());
        query.bindValue(":category", transaction->category());
        query.bindValue(":customer_reference", transaction->customerReference());
        query.bindValue(":bank_reference", transaction->bankReference());
        query.bindValue(":end_to_end_reference", transaction->endToEndReference());
        query.bindValue(":creditor_scheme_id", transaction->creditorSchemeId());
        query.bindValue(":originator_id", transaction->originatorId());
        query.bindValue(":mandate_id", transaction->mandateId());
        query.bindValue(":mandate_date", transaction->mandateDate());
        query.bindValue(":mandate_debitor_name", transaction->mandateDebitorName());
        query.bindValue(":original_creditor_scheme_id", transaction->originalCreditorSchemeId());
        query.bindValue(":original_mandate_id", transaction->originalMandateId());
        query.bindValue(":original_creditor_name", transaction->originalCreditorName());
        query.bindValue(":sequence", (qint32) transaction->sequence());
        query.bindValue(":charge", (qint32) transaction->charge());
        query.bindValue(":remote_addr_street", transaction->remoteAddrStreet());
        query.bindValue(":remote_addr_zipcode", transaction->remoteAddrZipcode());
        query.bindValue(":remote_addr_city", transaction->remoteAddrCity());
        query.bindValue(":remote_addr_phone", transaction->remoteAddrPhone());
        query.bindValue(":period", (qint32) transaction->period());
        query.bindValue(":cycle", transaction->cycle());
        query.bindValue(":execution_day", transaction->executionDay());
        query.bindValue(":first_date", transaction->firstDate());
        query.bindValue(":last_date", transaction->lastDate());
        query.bindValue(":next_date", transaction->nextDate());
        query.bindValue(":unit_id", transaction->unitId());
        query.bindValue(":unit_id_name_space", transaction->unitIdNameSpace());
        query.bindValue(":ticker_symbol", transaction->tickerSymbol());
        query.bindValue(":units", transaction->units());
        query.bindValue(":unit_price_value", transaction->unitPriceValue());
        query.bindValue(":unit_price_date", transaction->unitPriceDate());
        query.bindValue(":commission_value", transaction->commissionValue());
        query.bindValue(":memo", transaction->memo());
        query.bindValue(":hash", calculateTransactionHash(transaction));

        return query;
    }

    QString calculateTransactionHash(const Transaction *transaction)
    {
        const QString hash = transaction->hash();
        if (!hash.isEmpty()) {
            return hash;
        }

        const QString purpose = transaction->purpose();
        return QString("%1").arg(
            QString(QCryptographicHash::hash(purpose.toUtf8(), QCryptographicHash::Sha1).toHex()));
    }

    QString escapeKey(const QString &key)
    {
        QString result = {};

        const int stringLength = key.length();
        for (int a = 0; a < stringLength; ++a) {
            const QChar strPart = key.at(a);
            const int ascii = (int) strPart.toLatin1();

            const bool noEscapeSeq = (strPart != "'" && strPart != "\"" && strPart != '\\');
            const bool isValidAscii = ((ascii >= 32 && ascii <= 126)
                                       || (ascii >= 128 && ascii <= 255));

            if (noEscapeSeq && isValidAscii) {
                result.append(strPart);
            } else {
                switch (ascii) {
                case 34: /* ascii = " */
                    result.append(QString(strPart).replace(strPart, "\""));
                    break;
                case 39: /* ascii = ' */
                    result.append(QString(strPart).replace(strPart, "''"));
                    break;
                case 92: /* ascii = \ */
                    result.append(QString(strPart).replace(strPart, "\\"));
                    break;
                default:
                    break;
                }
            }
        }

        return result;
    }

    void setFilePath(const QString &path) { m_filePath = path; }
    void setKey(const QString &key) { m_key = key; }

    StorageConnection *databaseConnection() { return m_connection; }

    QSettings *settings()
    {
        if (m_settings == Q_NULLPTR) {
            m_settings = new QSettings(QSettings::IniFormat,
                                       QSettings::UserScope,
                                       SingleApplication::organizationName(),
                                       SingleApplication::applicationName());
        }

        return m_settings;
    }

private:
    QSettings *m_settings;
    StorageConnection *m_connection;
    QString m_filePath;
    QString m_key;

    void setupTables()
    {
        QFile storageFile(":/app/olbaflinx-storage");
        if (!storageFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return;
        }

        QStringList sqlStatements = QTextStream(&storageFile).readAll().split(';');
        QStringList queries = {};

        for (auto &query : sqlStatements) {
            queries << query.replace("#", ";").trimmed();
        }

        QSqlQuery query = databaseQuery();
        for (const auto &sqlStatement : qAsConst(queries)) {
            if (sqlStatement.isEmpty()) {
                continue;
            }

            databaseConnection()->begindTransaction();
            bool success = query.exec(sqlStatement);
            if (!success) {
                databaseConnection()->rollbackTransaction();
                return;
            }
            databaseConnection()->commitTransaction();
        }
    }
};

VaultStorage::VaultStorage()
    : QObject(Q_NULLPTR)
    , d_ptr(new Private())
{ }

VaultStorage::~VaultStorage()
{
    d_ptr.reset();
}

void VaultStorage::setDatabaseKey(const QString &dbFileName, const QString &key)
{
    d_ptr->setFilePath(dbFileName);
    d_ptr->setKey(key);
}

void VaultStorage::initialize(const bool initializeSchema)
{
    d_ptr->initialize(initializeSchema);
}

bool VaultStorage::isStorageValid() const
{
    return d_ptr->isStorageValid();
}

bool VaultStorage::changeKey(const QString &oldKey, const QString &newKey) const
{
    d_ptr->setKey(oldKey);
    QSqlQuery query = d_ptr->databaseQuery();
    bool oldKeyValid = d_ptr->isStorageValid();
    if (!oldKeyValid) {
        return oldKeyValid;
    }

    query.exec(QString("PRAGMA key='%1';").arg(d_ptr->escapeKey(newKey)));
    return d_ptr->isStorageValid();
}

QString VaultStorage::storagePath() const
{
    QString path = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    return QString("%1/%2").arg(path, SingleApplication::organizationName());
}

void VaultStorage::close()
{
    d_ptr->close();
}

void VaultStorage::storeSetting(const QString &key, const QVariant &value, const QString &group)
{
    if (!group.isEmpty()) {
        d_ptr->settings()->beginGroup(group);
    }

    d_ptr->settings()->setValue(key, value);

    if (!group.isEmpty()) {
        d_ptr->settings()->endGroup();
    }

    d_ptr->settings()->sync();
}

QVariant VaultStorage::setting(const QString &key,
                               const QString &group,
                               const QVariant &defaultValue) const
{
    if (!group.isEmpty()) {
        d_ptr->settings()->beginGroup(group);
    }

    QVariant value = d_ptr->settings()->value(key, defaultValue);

    if (!group.isEmpty()) {
        d_ptr->settings()->endGroup();
    }

    return value;
}

void VaultStorage::addAccount(const Account *account)
{
    if (account == Q_NULLPTR) {
        return;
    }

    if (!d_ptr->isStorageValid()) {
        return;
    }

    QSqlQuery query = d_ptr->databaseQuery();
    d_ptr->accountQuery(account, query).exec();
}

void VaultStorage::addAccounts(const QVector<const Account *> &accounts)
{
    if (accounts.isEmpty()) {
        return;
    }

    if (!d_ptr->isStorageValid()) {
        return;
    }

    qApp->setOverrideCursor(Qt::WaitCursor);

    QEventLoop loop(this);
    QFutureWatcher<void> accountWatcher(this);
    connect(&accountWatcher, &QFutureWatcher<void>::finished, &loop, [&]() {
        loop.quit();
        accountWatcher.cancel();
        accountWatcher.waitForFinished();

        qApp->restoreOverrideCursor();
    });

    QSqlQuery query = d_ptr->databaseQuery();
    accountWatcher.setFuture(QtConcurrent::run([this, accounts, &query]() -> void {
        const int accountSize = accounts.size();
        for (int currentIndex = 0; currentIndex < accountSize; ++currentIndex) {
            const auto account = accounts.at(currentIndex);
            d_ptr->accountQuery(account, query).exec();
            const qreal percentage = currentIndex * 100.0 / accountSize;
            Q_EMIT progress(percentage);
        }
    }));

    loop.exec();
}

QVector<quint32> VaultStorage::accountIds()
{
    if (!d_ptr->isStorageValid()) {
        return {};
    }

    qApp->setOverrideCursor(Qt::WaitCursor);

    QEventLoop loop(this);
    QFutureWatcher<QVector<quint32>> accountWatcher(this);
    connect(&accountWatcher, &QFutureWatcher<QVector<quint32>>::finished, &loop, [&]() {
        loop.quit();
        accountWatcher.cancel();
        accountWatcher.waitForFinished();

        qApp->restoreOverrideCursor();
    });

    QSqlQuery query = d_ptr->databaseQuery();
    accountWatcher.setFuture(QtConcurrent::run([&query]() -> QVector<quint32> {
        QVector<quint32> ids = QVector<quint32>();
        query.exec(StorageSqlAccountSelectQuery);
        const int fieldNo = query.record().indexOf("unique_id");
        while (query.next()) {
            ids.append(query.value(fieldNo).toInt());
        }
        return ids;
    }));
    loop.exec();

    return accountWatcher.result();
}

void VaultStorage::addTransaction(const quint32 &accountId, const Transaction *transaction)
{
    if (!d_ptr->isStorageValid()) {
        return;
    }

    QSqlQuery query = d_ptr->databaseQuery();
    d_ptr->transactionQuery(accountId, transaction, query).exec();
}

void VaultStorage::addTransactions(const quint32 &accountId,
                                   const QVector<const Transaction *> &transactions)
{
    if (transactions.isEmpty()) {
        return;
    }

    if (!d_ptr->isStorageValid()) {
        return;
    }

    qApp->setOverrideCursor(Qt::WaitCursor);

    QEventLoop loop(this);
    QFutureWatcher<void> transactionWatcher(this);
    connect(&transactionWatcher, &QFutureWatcher<void>::finished, &loop, [&]() {
        loop.quit();
        transactionWatcher.cancel();
        transactionWatcher.waitForFinished();

        qApp->restoreOverrideCursor();
    });

    QSqlQuery query = d_ptr->databaseQuery();
    transactionWatcher.setFuture(
        QtConcurrent::run([this, accountId, transactions, &query]() -> void {
            const int transactionSize = transactions.size();
            for (int currentIndex = 0; currentIndex < transactionSize; ++currentIndex) {
                const auto transaction = transactions.at(currentIndex);
                const QString hash = d_ptr->calculateTransactionHash(transaction);

                query.prepare(StorageSqlTransactionExists);
                query.bindValue(":account_id", accountId);
                query.bindValue(":hash", hash);
                query.exec();
                query.first();

                const int count = query.record().count();
                if (count == 0) {
                    d_ptr->transactionQuery(accountId, transaction, query).exec();
                }

                const qreal percentage = currentIndex * 100.0 / transactionSize;
                Q_EMIT progress(percentage);
            }
        }));
    loop.exec();
}

QVector<const Transaction *> VaultStorage::transactionsIds(const quint32 &accountId,
                                                           const qint32 &limit,
                                                           const qint32 &offset)
{
    if (!d_ptr->isStorageValid()) {
        return {};
    }

    qApp->setOverrideCursor(Qt::WaitCursor);

    QEventLoop loop(this);
    QFutureWatcher<QVector<const Transaction *>> transactionWatcher(this);
    connect(&transactionWatcher,
            &QFutureWatcher<QVector<const Transaction *>>::finished,
            &loop,
            [&]() {
                loop.quit();
                transactionWatcher.cancel();
                transactionWatcher.waitForFinished();

                qApp->restoreOverrideCursor();
            });

    QSqlQuery query = d_ptr->databaseQuery();
    transactionWatcher.setFuture(QtConcurrent::run(
        [this, accountId, limit, offset, &query]() -> QVector<const Transaction *> {
            QVector<const Transaction *> transactions = QVector<const Transaction *>();
            query.prepare(StorageSqlTransactionByAccountIdWithLimitQuery);
            query.bindValue(":account_id", accountId);
            query.bindValue(":limit", limit);
            query.bindValue(":offset", offset);
            query.exec();

            int currentIndex = 0;
            while (query.next()) {
                transactions.append(Q_NULLPTR);
                ++currentIndex;
            }

            const qreal percentage = currentIndex * 100.0 / limit;
            Q_EMIT progress(percentage);

            return transactions;
        }));
    loop.exec();

    return transactionWatcher.result();
}
