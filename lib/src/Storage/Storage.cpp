/**
 * Copyright (C2022-2025, Alexander Saal <developer@olbaflinx.chm-projects.de>
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

#include "Storage.h"
#include "StorageConnection.h"

#include "Banking/Account/Account.h"
#include "Banking/Account/ReferenceAccount.h"
#include "Banking/Transaction/Transaction.h"

#include <QtConcurrent/QtConcurrent>

#include <QtCore/QCryptographicHash>
#include <QtCore/QEventLoop>
#include <QtCore/QFile>
#include <QtCore/QScopedPointer>
#include <QtCore/QSettings>
#include <QtCore/QStringList>
#include <QtCore/QTextStream>
#include <QtCore/QVariant>

#include <QtSql/QSqlError>
#include <QtSql/QSqlField>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlRecord>

#include <QtWidgets/QApplication>

/**
 * Password regular expression
 *
 * /^(?=.*[a-z])(?=.*[A-Z])(?=.*\d)(?=.*[!"§$%&\/()=?´`{}\[\]\\ß@€~’*'+#-_.:,;µöäüÖÄÜ<|>])[A-Za-z\d!"§$%&\/()=?´`{}\[\]\\ß@€~’*'+#-_.:,;µöäüÖÄÜ<|>]{6,}$/g
 *
 * At least one lower case English letter, a-z
 * At least one upper case English letter, A-Z
 * At least one lower umlaut case letter, öäü
 * At least one upper umlaut case letter, ÖÄÜ
 * At least one digit, 0-9
 * At least one of special character, !"§$%&/()=?´`{}[]\ß@€~’*'+#-_.:,;µöäüÖÄÜ<|>
 * Minimum six in length 6 (with the anchors)
 */
#define MinPasswordReqEx \
    QRegularExpression("^(?=.*[a-z])(?=.*[A-Z])(?=.*\\d)(?=.*[!\"§$%&/" \
                       "()=?´`{}\\[\\]\\ß@€~’*'+#-_.:,;µöäüÖÄÜ<|>])[A-Za-z\\d!\"§$%&/" \
                       "()=?´`{}\\[\\]\\ß@€~’*'+#-_.:,;µöäüÖÄÜ<|>]{6,}$")

#define AccountInsertQuery \
    "INSERT INTO accounts (`type`, unique_id, backend_name, owner_name, " \
    "account_name, currency, memo, iban, bic, country, bank_code, bank_name, " \
    "branch_id, account_number, sub_account_number) " \
    "VALUES (:type, :unique_id, :backend_name, :owner_name, :account_name, " \
    ":currency, :memo, :iban, :bic, :country, :bank_code, :bank_name, " \
    ":branch_id, :account_number, :sub_account_number);"

#define TransactionInsertQuery \
    "INSERT INTO transactions (account_id, `type`, sub_type, command, status, " \
    "unique_account_id, unique_id, ref_unique_id, id_for_application, " \
    "string_id_for_application, session_id, group_id, fi_id, local_iban, local_bic, " \
    "local_country, local_bank_code, local_branch_id, local_account_number, local_suffix, " \
    "local_name, remote_country, remote_bank_code, remote_branch_id, " \
    "remote_account_number, remote_suffix, remote_iban, remote_bic, remote_name, `date`, " \
    "valuta_date, value, currency, fees, transaction_code, transaction_text, " \
    "transaction_key, text_key, primanota, purpose, `category`, customer_reference, " \
    "bank_reference, end_to_end_reference, creditor_scheme_id, originator_id, mandate_id, " \
    "mandate_date, mandate_debitor_name, original_creditor_scheme_id, original_mandate_id, " \
    "original_creditor_name, `sequence`, charge, remote_addr_street, remote_addr_zipcode, " \
    "remote_addr_city, remote_addr_phone, period, `cycle`, execution_day, first_date, " \
    "last_date, next_date, unit_id, unit_id_name_space, ticker_symbol, units, " \
    "unit_price_value, unit_price_date, commission_value, memo, `hash`) " \
    "VALUES (:account_id, :type, :sub_type, :command, :status, :unique_account_id, " \
    ":unique_id, :ref_unique_id, :id_for_application, :string_id_for_application, " \
    ":session_id, :group_id, :fi_id, :local_iban, :local_bic, :local_country, " \
    ":local_bank_code, :local_branch_id, :local_account_number, :local_suffix, " \
    ":local_name, :remote_country, :remote_bank_code, :remote_branch_id, " \
    ":remote_account_number, :remote_suffix, :remote_iban, :remote_bic, :remote_name, " \
    ":date, :valuta_date, :value, :currency, :fees, :transaction_code, :transaction_text, " \
    ":transaction_key, :text_key, :primanota, :purpose, :category, :customer_reference, " \
    ":bank_reference, :end_to_end_reference, :creditor_scheme_id, :originator_id, " \
    ":mandate_id, :mandate_date, :mandate_debitor_name, :original_creditor_scheme_id, " \
    ":original_mandate_id, :original_creditor_name, :sequence, :charge, " \
    ":remote_addr_street, :remote_addr_zipcode, :remote_addr_city, :remote_addr_phone, " \
    ":period, :cycle, :execution_day, :first_date, :last_date, :next_date, :unit_id, " \
    ":unit_id_name_space, :ticker_symbol, :units, :unit_price_value, :unit_price_date, " \
    ":commission_value, :memo, :hash);"

using namespace olbaflinx::core::storage;
using namespace olbaflinx::core::banking;
using namespace olbaflinx::core::banking::account;
using namespace olbaflinx::core::banking::transaction;

inline void initResource()
{
    Q_INIT_RESOURCE(OlbaFlinxCore);
}
inline void cleanupResource()
{
    Q_CLEANUP_RESOURCE(OlbaFlinxCore);
}

class Storage::Private
{
public:
    explicit Private(Storage *storage)
        : m_key("")
        , m_storageFileName("")
        , m_settings(Q_NULLPTR)
        , m_connection(Q_NULLPTR)
        , q_ptr(storage)
    {
        initResource();

        qRegisterMetaType<Account *>();
        qRegisterMetaType<ReferenceAccount *>();
        qRegisterMetaType<Transaction *>();
        qRegisterMetaType<BankingItem *>();
        qRegisterMetaType<const BankingItem *>();
    }

    ~Private()
    {
        m_settings->sync();
        delete m_settings;

        close();
    }

    void setStorageFile(const QString &file) { m_storageFileName = file; }

    void setKey(const QString &key) { m_key = key; }

    StorageConnection *connection() { return m_connection; }

    QString lastErrorMessage() { return m_connection->lastErrorMessage(); }

    bool initialize(const bool withSchema = false)
    {
        if (m_connection != nullptr) {
            const auto currentDatabaseName = m_connection->database().databaseName();
            if (currentDatabaseName.toLower() != m_storageFileName.toLower()) {
                if (m_connection->isOpen()) {
                    m_connection->close();
                }
                delete m_connection;
                m_connection = nullptr;
            } else {
                if (withSchema) {
                    return setupTables();
                }
                return true;
            }
        }

        initResource();

        m_connection = new StorageConnection(m_storageFileName);
        if (!m_connection->isOpen()) {
            Q_EMIT q_ptr->errorOccurred(Storage::PasswordChanged, lastErrorMessage());
            Q_EMIT q_ptr->finished();
            return false;
        }

        if (withSchema) {
            return setupTables();
        }

        return m_connection->isOpen();
    }

    void close()
    {
        if (m_connection) {
            if (m_connection->isOpen()) {
                QSqlQuery query = databaseQuery();
                query.exec("REINDEX;");
                query.exec("VACUUM;");
                m_connection->close();
            }

            delete m_connection;
            m_connection = nullptr;
        }

        cleanupResource();
    }

    bool isConnectionValid()
    {
        if (!m_connection->isOpen()) {
            return false;
        }

        if (m_key.isEmpty()) {
            return false;
        }

        if (m_storageFileName.isEmpty()) {
            return false;
        }

        QSqlQuery dbQuery = databaseQuery();
        bool executed = dbQuery.exec("SELECT COUNT(*) AS ID_COUNT FROM accounts;");
        if (!executed) {
            return false;
        }

        while (dbQuery.next()) {
            const int count = dbQuery.value("ID_COUNT").toInt();
            executed &= (count >= 0);
        }

        return executed;
    }

    QSqlQuery databaseQuery()
    {
        QSqlQuery dbQuery(m_connection->database());
        dbQuery.exec(QString("PRAGMA key='%1';").arg(escapeKey(m_key)));
        return dbQuery;
    }

    QMap<int, QString> tableColumns(const QString &table)
    {
        auto columnList = QMap<int, QString>();

        QSqlQuery query = databaseQuery();
        bool executed = query.exec(QString("SELECT * FROM pragma_table_info('%1');").arg(table));

        if (!executed) {
            return {};
        }

        while (query.next()) {
            columnList[query.value(0).toInt()] = query.value(1).toString();
        }

        return columnList;
    }

    QString escapeKey(const QString &key)
    {
        QString result = {};

        const qsizetype stringLength = key.length();
        for (int a = 0; a < stringLength; ++a) {
            const QChar strPart = key.at(a);
            const int ascii = (int) strPart.toLatin1();

            const bool noEscapeSeq = (strPart != '\'' && strPart != '"' && strPart != '\\');
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

    QSettings *settings()
    {
        if (m_settings == nullptr) {
            m_settings = new QSettings(QSettings::IniFormat,
                                       QSettings::UserScope,
                                       QApplication::organizationName(),
                                       QApplication::applicationName());
        }

        return m_settings;
    }

    QString storagePath()
    {
        QString path = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
        return QString("%1/%2").arg(path, QApplication::organizationName());
    }

    bool setupTables()
    {
        QFile storageFile(":/lib/olbaflinx-storage");
        if (!storageFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            Q_EMIT q_ptr->errorOccurred(Storage::SchemaSetup, storageFile.errorString());
            Q_EMIT q_ptr->finished();
            return false;
        }

        QStringList sqlStatements = QTextStream(&storageFile).readAll().split(';');
        QStringList queries = {};

        for (auto &query : sqlStatements) {
            queries << query.replace("#", ";").trimmed();
        }

        QSqlQuery query = databaseQuery();
        for (const auto &sqlStatement : std::as_const(queries)) {
            if (sqlStatement.isEmpty()) {
                continue;
            }

            connection()->beginTransaction();
            bool success = query.exec(sqlStatement);
            if (!success) {
                connection()->rollbackTransaction();
                Q_EMIT q_ptr->errorOccurred(Storage::SchemaSetup, lastErrorMessage());
                Q_EMIT q_ptr->finished();
                return false;
            }
            connection()->commitTransaction();
        }

        return true;
    }

private:
    QString m_key;
    QString m_storageFileName;

    QSettings *m_settings;
    StorageConnection *m_connection;

    friend class Storage;
    Storage *q_ptr;
};

Storage::Storage()
    : QObject(Q_NULLPTR)
    , d_ptr(new Private(this))
{}

Storage::~Storage()
{
    delete d_ptr;
}

void Storage::setStorageFile(const QString &storageFileName)
{
    d_ptr->setStorageFile(storageFileName);
}

void Storage::setKey(const QString &key)
{
    d_ptr->setKey(key);
}

bool Storage::changeKey(const QString &oldKey, const QString &newKey)
{
    d_ptr->setKey(oldKey);

    QSqlQuery query = d_ptr->databaseQuery();
    bool valid = d_ptr->isConnectionValid();

    if (!valid) {
        return false;
    }

    d_ptr->setKey(newKey);

    valid = query.exec(QString("PRAGMA rekey='%1';").arg(d_ptr->escapeKey(newKey)));

    return valid && d_ptr->isConnectionValid();
}

bool Storage::initialize(bool withSchema)
{
    return d_ptr->initialize(withSchema);
}

bool Storage::isValid()
{
    return d_ptr->isConnectionValid();
}

QString Storage::storagePath() const
{
    return d_ptr->storagePath();
}

void Storage::close()
{
    d_ptr->close();
}

void Storage::storeSetting(const QString &key, const QVariant &value, const QString &group)
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

QVariant Storage::setting(const QString &key,
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

QRegularExpression Storage::minPasswordGuidelines() const
{
    return MinPasswordReqEx;
}

bool Storage::storeItem(const BankingItem *bankingItem)
{
    auto query = d_ptr->databaseQuery();

    if (bankingItem->isValid()) {
        auto map = bankingItem->toMap();

        const auto type = bankingItem->itemType();
        if (type.startsWith("Account")) {
            query.prepare(AccountInsertQuery);
        } else if (type.startsWith("ReferenceAccount")) {
        } else if (type.startsWith("Transaction")) {
            query.prepare(TransactionInsertQuery);
        }

        for (auto [key, value] : map.asKeyValueRange()) {
            query.bindValue(key, value);
        }

        const auto success = query.exec();
        if (!success) {
            Q_EMIT errorOccurred(Error::StoreItem, d_ptr->lastErrorMessage());
            Q_EMIT finished();
            return false;
        }

        map.clear();
    }

    query.clear();

    Q_EMIT finished();

    return true;
}

void Storage::receiveItems(Type type, int offset, int limit)
{
    auto columnList = QMap<int, QString>();
    auto query = d_ptr->databaseQuery();

    bool executed = false;
    int index = 0;

    switch (type) {
    case Storage::StorageAccount:
        columnList = d_ptr->tableColumns({"accounts"});
        executed = query.exec(
            QString("SELECT * FROM accounts LIMIT %1 OFFSET %2;").arg(limit).arg(offset));
        break;
    case Storage::StorageReferenceAccount:
        columnList = d_ptr->tableColumns({"refaccounts"});
        executed = query.exec(
            QString("SELECT * FROM refaccounts LIMIT %1 OFFSET %2;").arg(limit).arg(offset));
        break;
    case Storage::StorageTransaction:
        columnList = d_ptr->tableColumns({"transactions"});
        executed = query.exec(
            QString("SELECT * FROM transactions LIMIT %1 OFFSET %2;").arg(limit).arg(offset));
        break;
    case Storage::StorageCategories: {
    } break;
    case Storage::StorageContacts: {
    } break;
    }

    if (!executed) {
        Q_EMIT errorOccurred(Storage::StoreItem, d_ptr->lastErrorMessage());
        Q_EMIT finished();
        return;
    }

    auto bankingItems = QList<BankingItem *>();
    auto map = QMap<QString, QVariant>();

    if (columnList.isEmpty()) {
        Q_EMIT errorOccurred(Storage::StoreItem, tr("Not columns for store item found."));
        Q_EMIT finished();
        return;
    }

    int totalRows = query.numRowsAffected();

    while (query.next()) {
        for (auto [key, value] : columnList.asKeyValueRange()) {
            map[":" + value] = query.value(key);
        }

        switch (type) {
        case Storage::StorageAccount: {
            const auto account = new Account();
            bankingItems << account->create(map);
            delete account;

            break;
        }
        case Storage::StorageReferenceAccount: {
            const auto refAccount = new ReferenceAccount();
            bankingItems << refAccount->create(map);
            delete refAccount;
            break;
        }
        case Storage::StorageTransaction: {
            const auto transaction = new Transaction();
            bankingItems << transaction->create(map);
            delete transaction;
            break;
        }
        case Storage::StorageCategories: {
        } break;
        case Storage::StorageContacts: {
        } break;
        }

        const auto percentage = index * 100.0 / totalRows;
        Q_EMIT progressChanged((int) percentage);

        ++index;

        map.clear();
    }

    if (bankingItems.isEmpty()) {
        Q_EMIT errorOccurred(Storage::StoreItem, tr("No items found"));
        Q_EMIT finished();
        return;
    }

    columnList.clear();

    Q_EMIT itemsReceived(bankingItems);

    qDeleteAll(bankingItems);
    bankingItems.clear();

    Q_EMIT finished();
}
