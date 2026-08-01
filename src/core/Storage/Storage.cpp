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
#include <QtCore/QDate>
#include <QtCore/QFile>
#include <QtCore/QMetaEnum>
#include <QtCore/QScopeGuard>
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
        QStringLiteral("refaccounts"),
        QStringLiteral("transaction_categories"),
        QStringLiteral("transactions"),
    };

    return knownTables.contains(table);
}

/**
 * The schema this build writes and understands. It is the number carried by the
 * highest migration the resource file installs. A file above it was written by a
 * newer build and is refused; a file below it is brought up by setupTables,
 * whose statements all create what is missing rather than what is new.
 */
constexpr int CurrentSchemaVersion = 2;

/**
 * The number a migration name carries in its first four characters. Names
 * without one, as older files hold them, count as zero.
 */
constexpr auto SchemaVersionQuery = QLatin1StringView(
    "SELECT COALESCE(MAX(CAST(substr(name, 1, 4) AS INTEGER)), 0) FROM migrations "
    "WHERE migrated = 1;");

/**
 * prepare takes a QString, so a view would be converted at every call. These
 * statements run once per stored item and the one for a transaction is about
 * 2400 characters long. QT-CPP-051.
 */
const QString &accountInsertQuery()
{
    static const QString statement = QStringLiteral(
        "INSERT INTO accounts (`type`, unique_id, backend_name, owner_name, "
        "account_name, currency, memo, iban, bic, country, bank_code, bank_name, "
        "branch_id, account_number, sub_account_number) "
        "VALUES (:type, :unique_id, :backend_name, :owner_name, :account_name, "
        ":currency, :memo, :iban, :bic, :country, :bank_code, :bank_name, "
        ":branch_id, :account_number, :sub_account_number);");

    return statement;
}

/**
 * The balance of an account goes to a table of its own, which carries the day
 * and the currency the figure belongs to. A row per account, replaced on every
 * write, which is what the UNIQUE on account_id in the schema is for.
 */
const QString &balanceInsertQuery()
{
    static const QString statement = QStringLiteral(
        "INSERT INTO balances (account_id, `date`, `value`, `type`, currency) "
        "VALUES (:account_id, :date, :value, :type, :currency) "
        "ON CONFLICT (account_id) DO UPDATE SET "
        "`date` = excluded.`date`, `value` = excluded.`value`, "
        "`type` = excluded.`type`, currency = excluded.currency;");

    return statement;
}

const QString &referenceAccountInsertQuery()
{
    static const QString statement = QStringLiteral(
        "INSERT INTO refaccounts (account_id, account_type, owner_name, owner_name2, "
        "account_name, iban, bic, country, bank_code, account_number, sub_account_number) "
        "VALUES (:account_id, :account_type, :owner_name, :owner_name2, :account_name, "
        ":iban, :bic, :country, :bank_code, :account_number, :sub_account_number);");

    return statement;
}

constexpr auto TransactionInsertQueryText = QLatin1StringView(
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

const QString &transactionInsertQuery()
{
    static const QString statement = QString::fromLatin1(TransactionInsertQueryText);

    return statement;
}

/**
 * Collects the placeholder names an insert statement carries, without the
 * leading colon. A property whose key is missing from that set would be dropped
 * by QSqlQuery::bindValue without a word, which is how the balance of an
 * account went missing.
 */
QSet<QString> placeholdersOf(const QString &statement)
{
    static const QRegularExpression placeholder(QStringLiteral(":([A-Za-z_][A-Za-z0-9_]*)"));

    auto names = QSet<QString>();

    auto matches = placeholder.globalMatch(statement);
    while (matches.hasNext()) {
        names.insert(matches.next().captured(1));
    }

    return names;
}

/**
 * The columns each table must carry for the statements above to bind. Checked
 * against pragma_table_info after the schema ran, so that a file which lost a
 * column, or was written by a build that did not have it yet, is named as such
 * rather than failing later on a bind that says nothing.
 */
const QMap<QString, QStringList> &expectedColumns()
{
    static const QMap<QString, QStringList> columns = {
        {QStringLiteral("accounts"),
         {QStringLiteral("id"), QStringLiteral("type"), QStringLiteral("unique_id"),
          QStringLiteral("backend_name"), QStringLiteral("owner_name"),
          QStringLiteral("account_name"), QStringLiteral("currency"), QStringLiteral("memo"),
          QStringLiteral("iban"), QStringLiteral("bic"), QStringLiteral("country"),
          QStringLiteral("bank_code"), QStringLiteral("bank_name"), QStringLiteral("branch_id"),
          QStringLiteral("account_number"), QStringLiteral("sub_account_number"),
          QStringLiteral("balance")}},
        {QStringLiteral("balances"),
         {QStringLiteral("id"), QStringLiteral("account_id"), QStringLiteral("date"),
          QStringLiteral("value"), QStringLiteral("type"), QStringLiteral("currency")}},
        {QStringLiteral("refaccounts"),
         {QStringLiteral("id"), QStringLiteral("account_id"), QStringLiteral("account_type"),
          QStringLiteral("owner_name"), QStringLiteral("owner_name2"),
          QStringLiteral("account_name"), QStringLiteral("iban"), QStringLiteral("bic"),
          QStringLiteral("country"), QStringLiteral("bank_code"),
          QStringLiteral("account_number"), QStringLiteral("sub_account_number")}},
        {QStringLiteral("migrations"),
         {QStringLiteral("id"), QStringLiteral("name"), QStringLiteral("migrated"),
          QStringLiteral("created_at")}},
    };

    return columns;
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

        if (!m_connection->isDriverAvailable()) {
            // Without the plugin no file opens at all. Told apart from a wrong
            // pass phrase, because the two ask for entirely different remedies.
            return reportSchemaFailure(ErrorCode::DriverMissing,
                                       QStringLiteral("The database driver the storage needs is "
                                                      "not installed"));
        }

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

        // Read before anything else touches the file. A schema this build does
        // not know may hold columns it would silently ignore on read and drop on
        // write.
        const auto versionBefore = schemaVersion();
        if (!versionBefore.hasValue()) {
            return reportSchemaFailure(versionBefore.error().code(),
                                       versionBefore.error().message());
        }

        if (versionBefore.value() > CurrentSchemaVersion) {
            return reportSchemaFailure(ErrorCode::SchemaMismatch,
                                       QStringLiteral("The storage %1 was written by a newer "
                                                      "version of this program")
                                           .arg(m_storageFileName));
        }

        if (withSchema) {
            if (const auto error = setupTables(); error.isError()) {
                return error;
            }

            return verifySchema(versionBefore.value());
        }

        if (versionBefore.value() < CurrentSchemaVersion) {
            return reportSchemaFailure(ErrorCode::SchemaMismatch,
                                       QStringLiteral("The storage %1 is at schema version %2 and "
                                                      "has to be migrated to %3")
                                           .arg(m_storageFileName)
                                           .arg(versionBefore.value())
                                           .arg(CurrentSchemaVersion));
        }

        return {};
    }

    /**
     * The schema version of the open file. A file without the migrations table
     * has not been set up yet and counts as version zero.
     */
    Result<int> schemaVersion()
    {
        QSqlQuery query;
        if (const auto error = openQuery(query); error.isError()) {
            return error;
        }

        if (!query.exec(QStringLiteral("SELECT COUNT(*) FROM sqlite_master WHERE type = 'table' "
                                       "AND name = 'migrations';"))
            || !query.next()) {
            return Error(ErrorCode::DatabaseFailure,
                         QStringLiteral("Could not read the schema version of %1: %2")
                             .arg(m_storageFileName, query.lastError().text()));
        }

        if (query.value(0).toInt() == 0) {
            return 0;
        }

        if (!query.exec(QString::fromLatin1(SchemaVersionQuery)) || !query.next()) {
            return Error(ErrorCode::DatabaseFailure,
                         QStringLiteral("Could not read the schema version of %1: %2")
                             .arg(m_storageFileName, query.lastError().text()));
        }

        return query.value(0).toInt();
    }

    /**
     * Runs after the schema statements. It confirms that the file now carries the
     * version this build writes, and that the tables the insert statements bind
     * against hold the columns they name.
     */
    Error verifySchema(int versionBefore)
    {
        const auto versionAfter = schemaVersion();
        if (!versionAfter.hasValue()) {
            return reportSchemaFailure(versionAfter.error().code(), versionAfter.error().message());
        }

        if (versionAfter.value() != CurrentSchemaVersion) {
            return reportSchemaFailure(ErrorCode::SchemaMismatch,
                                       QStringLiteral("The storage %1 is at schema version %2 "
                                                      "after the migration, expected %3")
                                           .arg(m_storageFileName)
                                           .arg(versionAfter.value())
                                           .arg(CurrentSchemaVersion));
        }

        if (versionAfter.value() != versionBefore) {
            qCInfo(lcStorage) << "migrated" << m_storageFileName << "from schema version"
                              << versionBefore << "to" << versionAfter.value();
        }

        for (const auto &[table, columns] : expectedColumns().asKeyValueRange()) {
            const auto columnList = tableColumns(table);
            const auto present = QSet<QString>(columnList.cbegin(), columnList.cend());

            auto missing = QStringList();
            for (const auto &column : columns) {
                if (!present.contains(column)) {
                    missing << column;
                }
            }

            if (!missing.isEmpty()) {
                return reportSchemaFailure(ErrorCode::SchemaMismatch,
                                           QStringLiteral("The table %1 of %2 is missing the "
                                                          "columns %3")
                                               .arg(table,
                                                    m_storageFileName,
                                                    missing.join(QLatin1StringView(", "))));
            }
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

    /**
     * Writes one row and answers with the id the database assigned to it.
     *
     * A key of the property map that the statement does not name is reported and
     * skipped. bindValue would drop it without a word, which is how the balance
     * of an account went missing.
     */
    Result<QVariant> insertRow(const QString &statement,
                               const QMap<QString, QVariant> &map,
                               const QString &type)
    {
        QSqlQuery query;
        if (const auto error = openQuery(query); error.isError()) {
            return error;
        }

        if (!query.prepare(statement)) {
            return Error(ErrorCode::DatabaseFailure,
                         QStringLiteral("Could not prepare the insert for type %1: %2")
                             .arg(type, query.lastError().text()));
        }

        const auto placeholders = placeholdersOf(statement);

        for (const auto &[key, value] : map.asKeyValueRange()) {
            if (!placeholders.contains(key)) {
                qCWarning(lcStorage) << "property" << key << "of type" << type
                                     << "has no column and is not stored";
                continue;
            }

            query.bindValue(QLatin1Char(':') + key, value);
        }

        if (!query.exec()) {
            return Error(ErrorCode::DatabaseFailure,
                         QStringLiteral("Could not store an item of type %1: %2")
                             .arg(type, lastErrorMessage()));
        }

        return query.lastInsertId();
    }

    /**
     * An account spans three tables: its own row, the balance that belongs to it
     * and the reference accounts held with it. Either all three are written or
     * none is, so that no account ends up carrying the balance of an older write.
     */
    Error storeAccount(const QMap<QString, QVariant> &map)
    {
        auto accountMap = map;

        // Both are kept in tables of their own. Left in place they would be
        // reported as columnless properties on every single write.
        const auto balance = accountMap.take(QStringLiteral("balance"));
        const auto referenceAccounts = accountMap.take(QStringLiteral("refAccounts"));

        if (!connection()->beginTransaction()) {
            return Error(ErrorCode::DatabaseFailure,
                         QStringLiteral("Could not begin a transaction on %1: %2")
                             .arg(m_storageFileName, lastErrorMessage()));
        }

        const auto accountId = insertRow(accountInsertQuery(),
                                         accountMap,
                                         QStringLiteral("Account"));
        if (!accountId.hasValue()) {
            return rollback(accountId.error());
        }

        if (const auto error = storeBalance(accountId.value(), balance, accountMap);
            error.isError()) {
            return rollback(error);
        }

        if (const auto error = storeReferenceAccounts(accountId.value(), referenceAccounts);
            error.isError()) {
            return rollback(error);
        }

        if (!connection()->commitTransaction()) {
            return Error(ErrorCode::DatabaseFailure,
                         QStringLiteral("Could not commit an account to %1: %2")
                             .arg(m_storageFileName, lastErrorMessage()));
        }

        return {};
    }

    /**
     * The balance goes to a row of its own, keyed by the account. The day is the
     * day of the write in UTC; AB_ACCOUNT_SPEC carries no date with the figure,
     * and inventing a business day would be worse than recording when it was
     * read. The type stays at zero for the same reason, the source does not say
     * whether the figure is booked or noted.
     */
    Error storeBalance(const QVariant &accountId,
                       const QVariant &balance,
                       const QMap<QString, QVariant> &accountMap)
    {
        if (!balance.isValid()) {
            return {};
        }

        const auto balanceMap = QMap<QString, QVariant>{
            {QStringLiteral("account_id"), accountId},
            {QStringLiteral("date"), QDate::currentDate()},
            {QStringLiteral("value"), balance},
            {QStringLiteral("type"), 0},
            {QStringLiteral("currency"), accountMap.value(QStringLiteral("currency"))},
        };

        const auto result = insertRow(balanceInsertQuery(),
                                      balanceMap,
                                      QStringLiteral("Balance"));

        return result.hasValue() ? Error() : result.error();
    }

    /**
     * Reference accounts belong to the account that holds them. An account is
     * written whole, so the rows of an earlier write go first.
     *
     * Ownership: the list travels through QVariant as raw pointers, created by
     * Account::referenceAccounts. They are deleted here, where the list ends.
     */
    Error storeReferenceAccounts(const QVariant &accountId, const QVariant &referenceAccounts)
    {
        if (!referenceAccounts.canConvert<ReferenceAccounts>()) {
            return {};
        }

        auto accounts = qvariant_cast<ReferenceAccounts>(referenceAccounts);
        const auto guard = qScopeGuard([&accounts] { qDeleteAll(accounts); });

        QSqlQuery query;
        if (const auto error = openQuery(query); error.isError()) {
            return error;
        }

        if (!query.prepare(QStringLiteral("DELETE FROM refaccounts WHERE account_id = :id;"))) {
            return Error(ErrorCode::DatabaseFailure,
                         QStringLiteral("Could not prepare the removal of reference accounts: %1")
                             .arg(query.lastError().text()));
        }

        query.bindValue(QStringLiteral(":id"), accountId);

        if (!query.exec()) {
            return Error(ErrorCode::DatabaseFailure,
                         QStringLiteral("Could not remove the reference accounts of %1: %2")
                             .arg(accountId.toString(), query.lastError().text()));
        }

        for (const auto referenceAccount : std::as_const(accounts)) {
            if (referenceAccount == nullptr || !referenceAccount->isValid()) {
                continue;
            }

            auto map = referenceAccount->toMap();
            map[QStringLiteral("account_id")] = accountId;

            const auto result = insertRow(referenceAccountInsertQuery(),
                                          map,
                                          QStringLiteral("ReferenceAccount"));
            if (!result.hasValue()) {
                return result.error();
            }
        }

        return {};
    }

    /**
     * Fills in what an account carries outside its own row: the balance and the
     * reference accounts held with it. Both live in tables of their own, keyed by
     * the id of the account.
     *
     * A failure of either read costs the property, not the account. It is logged
     * and the row is handed on without it, which is what Account::fromMap already
     * copes with.
     */
    void enrichAccountRow(QMap<QString, QVariant> &row)
    {
        const auto accountId = row.value(QStringLiteral("id"));
        if (!accountId.isValid()) {
            return;
        }

        QSqlQuery query;
        if (const auto error = openQuery(query); error.isError()) {
            qCWarning(lcStorage) << "could not read the balance of an account:" << error.message();
            return;
        }

        if (query.prepare(QStringLiteral("SELECT `value` FROM balances WHERE account_id = :id;"))) {
            query.bindValue(QStringLiteral(":id"), accountId);

            if (query.exec() && query.next()) {
                row[QStringLiteral("balance")] = query.value(0);
            }
        }

        if (!query.prepare(QStringLiteral("SELECT * FROM refaccounts WHERE account_id = :id;"))) {
            qCWarning(lcStorage) << "could not read the reference accounts of an account:"
                                 << query.lastError().text();
            return;
        }

        query.bindValue(QStringLiteral(":id"), accountId);

        if (!query.exec()) {
            qCWarning(lcStorage) << "could not read the reference accounts of an account:"
                                 << query.lastError().text();
            return;
        }

        // Ownership passes to Account::fromMap, which deletes the list once it has
        // copied the values into the account spec.
        auto referenceAccounts = ReferenceAccounts();
        const auto record = query.record();

        while (query.next()) {
            auto referenceMap = QMap<QString, QVariant>();
            for (int column = 0; column < record.count(); ++column) {
                referenceMap[record.fieldName(column)] = query.value(column);
            }

            if (auto *referenceAccount = ReferenceAccount::create(referenceMap)) {
                referenceAccounts << referenceAccount;
            }
        }

        if (!referenceAccounts.isEmpty()) {
            row[QStringLiteral("refAccounts")] = QVariant::fromValue(referenceAccounts);
        }
    }

    /**
     * Undoes the open transaction and hands back the error that caused it. A
     * failing rollback is logged; it cannot change what the caller is told, the
     * first error is the one that matters.
     */
    Error rollback(const Error &error)
    {
        if (!connection()->rollbackTransaction()) {
            qCWarning(lcStorage) << "could not roll back after" << error.message() << ":"
                                 << lastErrorMessage();
        }

        return error;
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

            if (!connection()->beginTransaction()) {
                return reportSchemaFailure(ErrorCode::DatabaseFailure,
                                           QStringLiteral("Could not begin a transaction on %1: %2")
                                               .arg(m_storageFileName, lastErrorMessage()));
            }

            if (!query.exec(sqlStatement)) {
                if (!connection()->rollbackTransaction()) {
                    qCWarning(lcStorage) << "could not roll back the failed schema statement:"
                                         << lastErrorMessage();
                }

                // The statement itself stays out of the message, it goes to the
                // log only. It carries no secret, but it is of no use to a user.
                qCCritical(lcStorage) << "schema statement failed:" << sqlStatement
                                      << query.lastError().text();

                return reportSchemaFailure(ErrorCode::DatabaseFailure,
                                           QStringLiteral("Could not create the schema of %1: %2")
                                               .arg(m_storageFileName, lastErrorMessage()));
            }

            if (!connection()->commitTransaction()) {
                return reportSchemaFailure(ErrorCode::DatabaseFailure,
                                           QStringLiteral("Could not commit the schema of %1: %2")
                                               .arg(m_storageFileName, lastErrorMessage()));
            }
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

    auto error = Error();

    if (type == QLatin1StringView("Account")) {
        error = d_ptr->storeAccount(bankingItem->toMap());
    } else if (type == QLatin1StringView("Transaction")) {
        const auto result = d_ptr->insertRow(transactionInsertQuery(),
                                             bankingItem->toMap(),
                                             type);
        error = result.hasValue() ? Error() : result.error();
    } else {
        // Category and Contact have no table of their own yet. The branch used to
        // be empty, which sent an unprepared query on its way.
        return Error(ErrorCode::NotImplemented,
                     QStringLiteral("Storing an item of type %1 is not implemented").arg(type));
    }

    if (error.isError()) {
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
        table = QStringLiteral("refaccounts");
        break;
    case Storage::StorageCategories:
    case Storage::StorageContacts:
        // These two have no table of their own in the schema. The branches used
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

    // The rows are collected first. An account needs a second read for its
    // balance and its reference accounts, and that read cannot run while this
    // query is still stepping over its own result.
    auto rows = QList<QMap<QString, QVariant>>();

    while (query.next()) {
        for (const auto &[key, value] : columnList.asKeyValueRange()) {
            map[value] = query.value(key);
        }

        rows << map;

        // No clear on purpose. Every row sets the same keys, so the inserts of
        // the next round turn into assignments.
    }

    for (auto &row : rows) {
        switch (type) {
        case Storage::StorageAccount:
            d_ptr->enrichAccountRow(row);
            bankingItems << Account::fromMap(row);
            break;
        case Storage::StorageTransaction:
            bankingItems << Transaction::fromMap(row);
            break;
        case Storage::StorageReferenceAccount:
            bankingItems << ReferenceAccount::fromMap(row);
            break;
        case Storage::StorageCategories:
        case Storage::StorageContacts:
            Q_UNREACHABLE();
        }

        ++index;

        if (totalRows > 0) {
            Q_EMIT progressChanged(qMin(index * 100 / totalRows, 100));
        }
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
