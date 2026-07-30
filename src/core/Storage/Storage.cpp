/**
 * Copyright (C) 2022-2026, Alexander Saal <developer@olbaflinx.chm-projects.de>
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

#include "core/Storage/Storage.h"
#include "core/Storage/StorageConnection.h"

#include "core/Banking/Account/Account.h"
#include "core/Banking/Account/ReferenceAccount.h"
#include "core/Banking/Transaction/Transaction.h"
#include "core/Logging.h"
#include "core/Result.h"

#include <QtCore/QCryptographicHash>
#include <QtCore/QFile>
#include <QtCore/QMetaEnum>
#include <QtCore/QScopedPointer>
#include <QtCore/QSet>
#include <QtCore/QSettings>
#include <QtCore/QStandardPaths>
#include <QtCore/QStringList>
#include <QtCore/QTextStream>
#include <QtCore/QVariant>

#include <QtSql/QSqlError>
#include <QtSql/QSqlField>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlRecord>

#include <memory>
#include <utility>

using namespace olbaflinx::core;
using namespace olbaflinx::core::storage;
using namespace olbaflinx::core::banking;
using namespace olbaflinx::core::banking::account;
using namespace olbaflinx::core::banking::transaction;

namespace {

/**
 * The store is attacked offline, its file can be copied away. The pass phrase is
 * the only thing left in the way of whoever holds the copy, so the lower bound
 * sits above what a password prompt usually asks for. The upper bound exists
 * because an unbounded length is an unchecked size, see QT-SEC-004.
 */
constexpr int MinPasswordLength = 12;
constexpr int MaxPasswordLength = 128;

/**
 * The widest window a single read may open. Without a bound a caller could ask
 * for INT_MAX rows and hold a whole table in memory at once. QT-SEC-004.
 */
constexpr int MaxItemsPerQuery = 1000;

/**
 * Password regular expression
 *
 * At least one lower case English letter, a-z
 * At least one upper case English letter, A-Z
 * At least one digit, 0-9
 * At least one special character out of the class below, umlauts among them
 * Between MinPasswordLength and MaxPasswordLength characters, with the anchors
 *
 * The pattern is compiled once. It used to be a macro and was therefore built
 * anew on every password check.
 */
const QRegularExpression &minPasswordPattern()
{
    // The hyphen stands last so that it counts as a literal. It used to sit
    // between '#' and '_', where a character class reads it as a range from 0x23
    // to 0x5F. That range covers every digit and every capital letter, so the
    // fourth lookahead matched on those alone and asked for nothing.
    //
    // The backslash, 0x5C, sat inside that range and reached the class through
    // it. With the range gone it has to stand on its own, which is what the
    // escaped pair in front of the 'ß' is for.
    //
    // The class is interpolated into both places of the pattern rather than
    // written out twice, so that the two cannot drift apart.
    static const QString specialCharacters
        = QStringLiteral("!\"§$%&/()=?´`{}\\[\\]\\\\ß@€~’*'+#_.:,;µöäüÖÄÜ<|>-");

    static const QRegularExpression pattern(
        QStringLiteral("^(?=.*[a-z])(?=.*[A-Z])(?=.*\\d)(?=.*[%1])[A-Za-z\\d%1]{%2,%3}$")
            .arg(specialCharacters)
            .arg(MinPasswordLength)
            .arg(MaxPasswordLength));

    return pattern;
}

/**
 * Wraps the pass phrase into a SQL string literal. Inside such a literal SQLite
 * knows exactly one special character, the single quote, and it is escaped by
 * doubling it. Everything else passes through as UTF-8, umlauts included.
 *
 * Deviation from QT-SEC-050, recorded here per QT-MAINT-012: SQLite accepts no
 * bound parameter in a PRAGMA. Verified against Qt 6.11.1 with SQLCipher 4.5.2,
 * where prepare("PRAGMA key = :key") fails with `near ":key": syntax error`. The
 * rule is met in substance, because the one character that could end the literal
 * early is escaped here and no other can.
 *
 * This replaces a hand written escape routine that read every character through
 * QChar::toLatin1. That call answers with a signed char on this platform, so its
 * own range check for the upper half of Latin-1 could never be true and every
 * character outside 32 to 126 fell through a bare default and was dropped from
 * the key without a word.
 */
QString keyLiteral(const QString &key)
{
    QString escaped = key;
    escaped.replace(QLatin1Char('\''), QLatin1StringView("''"));

    return QLatin1Char('\'') + escaped + QLatin1Char('\'');
}

/**
 * The tables the storage reads from. A table name cannot be bound, so a name is
 * checked against this list before it reaches a statement. QT-SEC-051.
 */
bool isKnownTable(const QString &table)
{
    static const QSet<QString> knownTables = {
        QStringLiteral("accounts"),
        QStringLiteral("balances"),
        QStringLiteral("categories"),
        QStringLiteral("contacts"),
        QStringLiteral("migrations"),
        QStringLiteral("transaction_categories"),
        QStringLiteral("transactions"),
    };

    return knownTables.contains(table);
}

constexpr auto AccountInsertQuery = QLatin1StringView(
    "INSERT INTO accounts (`type`, unique_id, backend_name, owner_name, "
    "account_name, currency, memo, iban, bic, country, bank_code, bank_name, "
    "branch_id, account_number, sub_account_number) "
    "VALUES (:type, :unique_id, :backend_name, :owner_name, :account_name, "
    ":currency, :memo, :iban, :bic, :country, :bank_code, :bank_name, "
    ":branch_id, :account_number, :sub_account_number);");

constexpr auto TransactionInsertQuery = QLatin1StringView(
    "INSERT INTO transactions (account_id, `type`, sub_type, command, status, "
    "unique_account_id, unique_id, ref_unique_id, id_for_application, "
    "string_id_for_application, session_id, group_id, fi_id, local_iban, local_bic, "
    "local_country, local_bank_code, local_branch_id, local_account_number, local_suffix, "
    "local_name, remote_country, remote_bank_code, remote_branch_id, "
    "remote_account_number, remote_suffix, remote_iban, remote_bic, remote_name, `date`, "
    "valuta_date, value, currency, fees, transaction_code, transaction_text, "
    "transaction_key, text_key, primanota, purpose, `category`, customer_reference, "
    "bank_reference, end_to_end_reference, creditor_scheme_id, originator_id, mandate_id, "
    "mandate_date, mandate_debitor_name, original_creditor_scheme_id, original_mandate_id, "
    "original_creditor_name, `sequence`, charge, remote_addr_street, remote_addr_zipcode, "
    "remote_addr_city, remote_addr_phone, period, `cycle`, execution_day, first_date, "
    "last_date, next_date, unit_id, unit_id_name_space, ticker_symbol, units, "
    "unit_price_value, unit_price_date, commission_value, memo, `hash`) "
    "VALUES (:account_id, :type, :sub_type, :command, :status, :unique_account_id, "
    ":unique_id, :ref_unique_id, :id_for_application, :string_id_for_application, "
    ":session_id, :group_id, :fi_id, :local_iban, :local_bic, :local_country, "
    ":local_bank_code, :local_branch_id, :local_account_number, :local_suffix, "
    ":local_name, :remote_country, :remote_bank_code, :remote_branch_id, "
    ":remote_account_number, :remote_suffix, :remote_iban, :remote_bic, :remote_name, "
    ":date, :valuta_date, :value, :currency, :fees, :transaction_code, :transaction_text, "
    ":transaction_key, :text_key, :primanota, :purpose, :category, :customer_reference, "
    ":bank_reference, :end_to_end_reference, :creditor_scheme_id, :originator_id, "
    ":mandate_id, :mandate_date, :mandate_debitor_name, :original_creditor_scheme_id, "
    ":original_mandate_id, :original_creditor_name, :sequence, :charge, "
    ":remote_addr_street, :remote_addr_zipcode, :remote_addr_city, :remote_addr_phone, "
    ":period, :cycle, :execution_day, :first_date, :last_date, :next_date, :unit_id, "
    ":unit_id_name_space, :ticker_symbol, :units, :unit_price_value, :unit_price_date, "
    ":commission_value, :memo, :hash);");

/**
 * Collects the placeholder names an insert statement carries, without the
 * leading colon. A property whose key is missing from that set would be dropped
 * by QSqlQuery::bindValue without a word, which is how the balance of an
 * account went missing.
 */
QSet<QString> placeholdersOf(QLatin1StringView statement)
{
    static const QRegularExpression placeholder(QStringLiteral(":([A-Za-z_][A-Za-z0-9_]*)"));

    auto names = QSet<QString>();

    auto matches = placeholder.globalMatch(QString::fromLatin1(statement));
    while (matches.hasNext()) {
        names.insert(matches.next().captured(1));
    }

    return names;
}

} // namespace

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
    explicit Private(Storage *storage, ApplicationInfo applicationInfo)
        : m_key()
        , m_storageFileName()
        , m_applicationInfo(std::move(applicationInfo))
        , m_connection(nullptr)
        , q_ptr(storage)
    {
        initResource();

        qRegisterMetaType<BankingItems>();
    }

    ~Private()
    {
        if (m_settings) {
            m_settings->sync();
        }

        close();
    }

    void setStorageFile(const QString &file) { m_storageFileName = file; }

    QString storageFileName() const { return m_storageFileName; }

    void setKey(const QString &key) { m_key = key; }

    StorageConnection *connection() { return m_connection; }

    QString lastErrorMessage() { return m_connection->lastErrorMessage(); }

    Error initialize(const bool withSchema = false)
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
                return {};
            }
        }

        initResource();

        m_connection = new StorageConnection(m_storageFileName);
        if (!m_connection->isOpen()) {
            // The message of the driver names the file and the reason, it is for
            // the log. The code tells the caller that the store could not be
            // opened, which is not the same as a wrong password.
            auto error = Error(ErrorCode::DatabaseFailure,
                               QStringLiteral("Could not open the storage file %1: %2")
                                   .arg(m_storageFileName, lastErrorMessage()));

            qCCritical(lcStorage) << error.message();

            Q_EMIT q_ptr->errorOccurred(error.code(), error.message());
            Q_EMIT q_ptr->finished();

            return error;
        }

        qCInfo(lcStorage) << "storage opened" << m_storageFileName;

        if (withSchema) {
            return setupTables();
        }

        return {};
    }

    void close()
    {
        if (m_connection) {
            if (m_connection->isOpen()) {
                QSqlQuery query;
                if (const auto error = openQuery(query); error.isError()) {
                    qCWarning(lcStorage) << "skipping maintenance on close:" << error.message();
                } else {
                    runMaintenance(query, QStringLiteral("REINDEX;"));
                    runMaintenance(query, QStringLiteral("VACUUM;"));
                }

                m_connection->close();

                qCInfo(lcStorage) << "storage closed" << m_storageFileName;
            }

            delete m_connection;
            m_connection = nullptr;
        }

        cleanupResource();
    }

    bool isConnectionValid()
    {
        if (m_connection == nullptr || !m_connection->isOpen()) {
            return false;
        }

        if (m_key.isEmpty()) {
            // A predicate, not an operation. A log entry is the whole of the
            // reporting here, the caller learns the outcome from the return value.
            qCDebug(lcStorage) << "storage has no key set";
            return false;
        }

        if (m_storageFileName.isEmpty()) {
            qCDebug(lcStorage) << "storage has no file set";
            return false;
        }

        QSqlQuery query;
        if (const auto error = openQuery(query); error.isError()) {
            qCDebug(lcStorage) << "storage is not readable:" << error.message();
            return false;
        }

        if (!query.exec(QStringLiteral("SELECT COUNT(*) AS ID_COUNT FROM accounts;"))) {
            qCDebug(lcStorage) << "storage is not readable:" << query.lastError().text();
            return false;
        }

        bool executed = true;
        while (query.next()) {
            const int count = query.value(QStringLiteral("ID_COUNT")).toInt();
            executed &= (count >= 0);
        }

        return executed;
    }

    /**
     * Puts a query on the connection and applies the decryption key to it.
     * Whoever gets no error back holds a query on a readable database; whoever
     * gets one must not carry on, every statement would then fail with a message
     * that does not name the cause.
     *
     * QSqlQuery cannot be copied, so the query is handed in rather than returned.
     */
    [[nodiscard]] Error openQuery(QSqlQuery &query)
    {
        if (m_connection == nullptr) {
            return Error(ErrorCode::DatabaseFailure,
                         QStringLiteral("No storage connection for %1").arg(m_storageFileName));
        }

        query = QSqlQuery(m_connection->database());
        if (!query.exec(QStringLiteral("PRAGMA key=") + keyLiteral(m_key) + QLatin1Char(';'))) {
            // The message of the driver can carry the key on this statement.
            // Only the file is named, never the reason verbatim.
            return Error(ErrorCode::PermissionDenied,
                         QStringLiteral("Could not apply the key to %1").arg(m_storageFileName));
        }

        return {};
    }

    /**
     * Maintenance. A failure leaves the data untouched, it only costs the
     * compactness of the file. A log entry is therefore the whole of the
     * handling.
     */
    static void runMaintenance(QSqlQuery &query, const QString &statement)
    {
        if (!query.exec(statement)) {
            qCWarning(lcStorage) << "maintenance statement failed:" << statement
                                 << query.lastError().text();
        }
    }

    QMap<int, QString> tableColumns(const QString &table)
    {
        if (!isKnownTable(table)) {
            qCCritical(lcStorage) << "refusing to read the columns of the unknown table" << table;
            return {};
        }

        auto columnList = QMap<int, QString>();

        QSqlQuery query;
        if (const auto error = openQuery(query); error.isError()) {
            qCCritical(lcStorage) << "could not read the columns of" << table << error.message();
            return {};
        }

        // The table name is interpolated because SQL knows no binding for an
        // identifier. It passed the list above, so it is one of ours.
        if (!query.exec(QStringLiteral("SELECT * FROM pragma_table_info('%1');").arg(table))) {
            qCCritical(lcStorage) << "could not read the columns of" << table
                                  << query.lastError().text();
            return {};
        }

        while (query.next()) {
            columnList[query.value(0).toInt()] = query.value(1).toString();
        }

        return columnList;
    }

    /**
     * The number of rows the given window actually yields. QSqlQuery::size() is
     * unavailable for SQLite and numRowsAffected() is undefined for a SELECT, so
     * the count comes from a query of its own.
     */
    Result<int> windowedRowCount(const QString &table, int offset, int limit)
    {
        if (!isKnownTable(table)) {
            return Error(ErrorCode::InvalidInput,
                         QStringLiteral("Unknown table %1").arg(table));
        }

        QSqlQuery query;
        if (const auto error = openQuery(query); error.isError()) {
            return error;
        }

        // The table name is interpolated because SQL knows no binding for an
        // identifier. It passed the list above. The window is bound.
        const auto statement
            = QStringLiteral(
                  "SELECT COUNT(*) FROM (SELECT 1 FROM %1 LIMIT :limit OFFSET :offset);")
                  .arg(table);

        if (!query.prepare(statement)) {
            return Error(ErrorCode::DatabaseFailure,
                         QStringLiteral("Could not prepare the row count of %1: %2")
                             .arg(table, query.lastError().text()));
        }

        query.bindValue(QStringLiteral(":limit"), limit);
        query.bindValue(QStringLiteral(":offset"), offset);

        if (!query.exec() || !query.next()) {
            return Error(ErrorCode::DatabaseFailure,
                         QStringLiteral("Could not count the rows of %1: %2")
                             .arg(table, query.lastError().text()));
        }

        return query.value(0).toInt();
    }

    QSettings *settings()
    {
        if (!m_settings) {
            m_settings = std::make_unique<QSettings>(QSettings::IniFormat,
                                                     QSettings::UserScope,
                                                     m_applicationInfo.organization,
                                                     m_applicationInfo.name);
        }

        return m_settings.get();
    }

    QString storagePath()
    {
        QString path = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
        return QStringLiteral("%1/%2").arg(path, m_applicationInfo.organization);
    }

    Error setupTables()
    {
        QFile storageFile(QStringLiteral(":/lib/olbaflinx-storage"));
        if (!storageFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return reportSchemaFailure(ErrorCode::IoFailure,
                                       QStringLiteral("Could not read the schema: %1")
                                           .arg(storageFile.errorString()));
        }

        const QStringList sqlStatements = QTextStream(&storageFile).readAll().split(';');
        QStringList queries = {};

        // The replacement works on a copy. It used to mutate sqlStatements as a
        // side effect of building the second list.
        for (const auto &statement : sqlStatements) {
            queries << QString(statement)
                           .replace(QStringLiteral("#"), QStringLiteral(";"))
                           .trimmed();
        }

        QSqlQuery query;
        if (const auto error = openQuery(query); error.isError()) {
            return reportSchemaFailure(error.code(), error.message());
        }

        for (const auto &sqlStatement : std::as_const(queries)) {
            if (sqlStatement.isEmpty()) {
                continue;
            }

            connection()->beginTransaction();
            if (!query.exec(sqlStatement)) {
                connection()->rollbackTransaction();

                // The statement itself stays out of the message, it goes to the
                // log only. It carries no secret, but it is of no use to a user.
                qCCritical(lcStorage) << "schema statement failed:" << sqlStatement
                                      << query.lastError().text();

                return reportSchemaFailure(ErrorCode::DatabaseFailure,
                                           QStringLiteral("Could not create the schema of %1: %2")
                                               .arg(m_storageFileName, lastErrorMessage()));
            }
            connection()->commitTransaction();
        }

        return {};
    }

private:
    Error reportSchemaFailure(ErrorCode code, const QString &message)
    {
        auto error = Error(code, message);

        qCCritical(lcStorage) << error.message();

        Q_EMIT q_ptr->errorOccurred(error.code(), error.message());
        Q_EMIT q_ptr->finished();

        return error;
    }

    QString m_key;
    QString m_storageFileName;
    ApplicationInfo m_applicationInfo;

    std::unique_ptr<QSettings> m_settings;
    StorageConnection *m_connection;

    friend class Storage;
    Storage *q_ptr;
};

Storage::Storage(ApplicationInfo applicationInfo, QObject *parent)
    : QObject(parent)
    , d_ptr(new Private(this, std::move(applicationInfo)))
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

Error Storage::changeKey(const QString &oldKey, const QString &newKey)
{
    d_ptr->setKey(oldKey);

    QSqlQuery query;
    if (const auto error = d_ptr->openQuery(query); error.isError()) {
        return error;
    }

    if (!d_ptr->isConnectionValid()) {
        return Error(ErrorCode::PermissionDenied,
                     QStringLiteral("The current key does not open %1")
                         .arg(d_ptr->storageFileName()));
    }

    d_ptr->setKey(newKey);

    if (!query.exec(QStringLiteral("PRAGMA rekey=") + keyLiteral(newKey) + QLatin1Char(';'))) {
        // Restores the state the caller handed us, so that a failed change does
        // not leave the storage holding a key it was never rekeyed to.
        d_ptr->setKey(oldKey);

        return Error(ErrorCode::DatabaseFailure,
                     QStringLiteral("Could not change the key of %1")
                         .arg(d_ptr->storageFileName()));
    }

    if (!d_ptr->isConnectionValid()) {
        return Error(ErrorCode::DatabaseFailure,
                     QStringLiteral("The storage %1 is not readable with the new key")
                         .arg(d_ptr->storageFileName()));
    }

    qCInfo(lcStorage) << "key changed for" << d_ptr->storageFileName();

    return {};
}

Error Storage::initialize(bool withSchema)
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
    return minPasswordPattern();
}

Error Storage::storeItem(const BankingItem *bankingItem)
{
    if (bankingItem == nullptr) {
        return Error(ErrorCode::InvalidInput, QStringLiteral("No banking item to store"));
    }

    const auto type = bankingItem->itemType();

    if (!bankingItem->isValid()) {
        // Used to return success without having written anything.
        return Error(ErrorCode::InvalidInput,
                     QStringLiteral("Invalid banking item of type %1").arg(type));
    }

    auto insertQuery = QLatin1StringView();
    if (type == QLatin1StringView("Account")) {
        insertQuery = AccountInsertQuery;
    } else if (type == QLatin1StringView("Transaction")) {
        insertQuery = TransactionInsertQuery;
    } else {
        // ReferenceAccount, Category and Contact have no table of their own yet.
        // The branch used to be empty, which sent an unprepared query on its way.
        return Error(ErrorCode::NotImplemented,
                     QStringLiteral("Storing an item of type %1 is not implemented").arg(type));
    }

    QSqlQuery query;
    if (const auto error = d_ptr->openQuery(query); error.isError()) {
        Q_EMIT errorOccurred(error.code(), error.message());
        Q_EMIT finished();

        return error;
    }

    if (!query.prepare(insertQuery)) {
        return Error(ErrorCode::DatabaseFailure,
                     QStringLiteral("Could not prepare the insert for type %1: %2")
                         .arg(type, query.lastError().text()));
    }

    const auto map = bankingItem->toMap();
    const auto placeholders = placeholdersOf(insertQuery);

    for (const auto &[key, value] : map.asKeyValueRange()) {
        if (!placeholders.contains(key)) {
            // bindValue would drop the property without a word. The property is
            // not persisted, which is a gap in the schema, not a failure of this
            // write; the item itself is stored.
            qCWarning(lcStorage) << "property" << key << "of type" << type
                                 << "has no column and is not stored";
            continue;
        }

        query.bindValue(QLatin1Char(':') + key, value);
    }

    if (!query.exec()) {
        auto error = Error(ErrorCode::DatabaseFailure,
                           QStringLiteral("Could not store an item of type %1: %2")
                               .arg(type, d_ptr->lastErrorMessage()));

        qCCritical(lcStorage) << error.message();

        Q_EMIT errorOccurred(error.code(), error.message());
        Q_EMIT finished();

        return error;
    }

    qCDebug(lcStorage) << "stored an item of type" << type;

    Q_EMIT finished();

    return {};
}

void Storage::receiveItems(Type type, int offset, int limit)
{
    const auto reportError = [this](ErrorCode code, const QString &message) {
        qCCritical(lcStorage) << message;

        Q_EMIT errorOccurred(code, message);
        Q_EMIT finished();
    };

    // The window used to travel into the statement unchecked. A negative offset
    // or a limit of INT_MAX is not a query anyone meant to run. QT-SEC-004.
    if (limit < 1 || limit > MaxItemsPerQuery || offset < 0) {
        reportError(ErrorCode::InvalidInput,
                    QStringLiteral("Invalid window: limit=%1 offset=%2").arg(limit).arg(offset));
        return;
    }

    auto table = QString();
    switch (type) {
    case Storage::StorageAccount:
        table = QStringLiteral("accounts");
        break;
    case Storage::StorageTransaction:
        table = QStringLiteral("transactions");
        break;
    case Storage::StorageReferenceAccount:
    case Storage::StorageCategories:
    case Storage::StorageContacts:
        // These three have no table of their own in the schema. The branches used
        // to be empty, which ended in a message that named the previous statement
        // instead of the cause.
        reportError(ErrorCode::NotImplemented,
                    QStringLiteral("Reading items of type %1 is not implemented")
                        .arg(QString::fromUtf8(
                            QMetaEnum::fromType<Storage::Type>().valueToKey(type))));
        return;
    }

    const auto columnList = d_ptr->tableColumns(table);
    if (columnList.isEmpty()) {
        reportError(ErrorCode::DatabaseFailure,
                    QStringLiteral("No columns found for the table %1").arg(table));
        return;
    }

    QSqlQuery query;
    if (const auto error = d_ptr->openQuery(query); error.isError()) {
        reportError(error.code(), error.message());
        return;
    }

    // The table name is interpolated because SQL knows no binding for an
    // identifier. It comes from the switch above and has passed the list in
    // tableColumns, which answers empty for a name it does not know. The window
    // is bound.
    const auto statement = QStringLiteral("SELECT * FROM %1 LIMIT :limit OFFSET :offset;")
                               .arg(table);

    if (!query.prepare(statement)) {
        reportError(ErrorCode::DatabaseFailure,
                    QStringLiteral("Could not prepare the read of %1: %2")
                        .arg(table, query.lastError().text()));
        return;
    }

    query.bindValue(QStringLiteral(":limit"), limit);
    query.bindValue(QStringLiteral(":offset"), offset);

    if (!query.exec()) {
        reportError(ErrorCode::DatabaseFailure,
                    QStringLiteral("Could not read the table %1: %2")
                        .arg(table, query.lastError().text()));
        return;
    }

    // numRowsAffected() is undefined for a SELECT and SQLite answers -1, which
    // turned the progress negative. The count comes from a query of its own. A
    // failure there costs the progress reporting, not the read itself.
    const auto rowCount = d_ptr->windowedRowCount(table, offset, limit);
    if (!rowCount.hasValue()) {
        qCWarning(lcStorage) << "no progress reporting:" << rowCount.error().message();
    }

    const int totalRows = rowCount.hasValue() ? rowCount.value() : 0;

    auto bankingItems = BankingItems();
    auto map = QMap<QString, QVariant>();
    int index = 0;

    while (query.next()) {
        for (const auto &[key, value] : columnList.asKeyValueRange()) {
            map[value] = query.value(key);
        }

        switch (type) {
        case Storage::StorageAccount:
            bankingItems << Account::fromMap(map);
            break;
        case Storage::StorageTransaction:
            bankingItems << Transaction::fromMap(map);
            break;
        case Storage::StorageReferenceAccount:
        case Storage::StorageCategories:
        case Storage::StorageContacts:
            Q_UNREACHABLE();
        }

        ++index;

        if (totalRows > 0) {
            Q_EMIT progressChanged(qMin(index * 100 / totalRows, 100));
        }

        // No clear on purpose. Every row sets the same keys, so the inserts of
        // the next round turn into assignments.
    }

    if (bankingItems.isEmpty()) {
        reportError(ErrorCode::NotFound,
                    QStringLiteral("No items found in the table %1").arg(table));
        return;
    }

    qCDebug(lcStorage) << "read" << bankingItems.size() << "items from" << table;

    Q_EMIT itemsReceived(bankingItems);

    Q_EMIT finished();
}
