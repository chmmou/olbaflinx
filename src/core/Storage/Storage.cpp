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

#include <QtConcurrent/QtConcurrentRun>

#include <QtCore/QCryptographicHash>
#include <QtCore/QDate>
#include <QtCore/QFile>
#include <QtCore/QFutureWatcher>
#include <QtCore/QMetaEnum>
#include <QtCore/QPromise>
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
 * because an unbounded length is an unchecked size.
 */
constexpr int MinPasswordLength = 12;
constexpr int MaxPasswordLength = 128;

/**
 * The widest window a single read may open. Without a bound a caller could ask
 * for INT_MAX rows and hold a whole table in memory at once.
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
    static const QString specialCharacters = QStringLiteral(
        "!\"§$%&/()=?´`{}\\[\\]\\\\ß@€~’*'+#_.:,;µöäüÖÄÜ<|>-");

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
 * Interpolating into a statement is normally forbidden. It is unavoidable here:
 * SQLite accepts no bound parameter in a PRAGMA, so prepare("PRAGMA key = :key")
 * fails with `near ":key": syntax error`. The intent behind the ban is met,
 * because the one character that could end the literal early is escaped here and
 * no other can.
 *
 * A hand written escape routine is the wrong answer. Reading each character
 * through QChar::toLatin1 answers with a signed char on this platform, so a
 * range check for the upper half of Latin-1 can never be true and every
 * character outside 32 to 126 falls through the default and leaves the key
 * without a word.
 */
QString keyLiteral(const QString &key)
{
    QString escaped = key;
    escaped.replace(QLatin1Char('\''), QLatin1StringView("''"));

    return QLatin1Char('\'') + escaped + QLatin1Char('\'');
}

/**
 * Escapes what LIKE reads as a pattern, so that the search text is looked for as
 * it was typed. A percent sign a user enters is a character to him, not a
 * placeholder for anything.
 *
 * The backslash goes first, otherwise it would escape the escapes added after
 * it. It is the character the statements name in their ESCAPE clause.
 */
QString escapedForLike(const QString &text)
{
    auto escaped = text;

    escaped.replace(QLatin1Char('\\'), QLatin1StringView("\\\\"));
    escaped.replace(QLatin1Char('%'), QLatin1StringView("\\%"));
    escaped.replace(QLatin1Char('_'), QLatin1StringView("\\_"));

    return escaped;
}

/**
 * The column a value of the enumeration stands for. SQL binds no identifier, so
 * a column name reaches a statement by interpolation and by nothing else; the
 * closed enumeration is what keeps anything but these four out of it, the same
 * way the list of known tables does for a table name.
 *
 * None answers with nothing. A read without a chosen column orders by the row id
 * alone.
 */
QString sortColumnName(const Storage::SortColumn column)
{
    switch (column) {
    case Storage::SortColumn::None:
        break;
    case Storage::SortColumn::Date:
        return QStringLiteral("`date`");
    case Storage::SortColumn::Value:
        return QStringLiteral("`value`");
    case Storage::SortColumn::RemoteName:
        return QStringLiteral("remote_name");
    case Storage::SortColumn::Purpose:
        return QStringLiteral("purpose");
    }

    return {};
}

/**
 * The tables the storage reads from. A table name cannot be bound, so a name is
 * checked against this list before it reaches a statement.
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
constexpr int CurrentSchemaVersion = 3;

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
 * 2400 characters long.
 */
const QString &accountInsertQuery()
{
    static const QString statement = QStringLiteral(
        "INSERT INTO accounts (`type`, unique_id, backend_name, owner_name, "
        "account_name, currency, memo, iban, bic, country, bank_code, bank_name, "
        "branch_id, account_number, sub_account_number) "
        "VALUES (:type, :unique_id, :backend_name, :owner_name, :account_name, "
        ":currency, :memo, :iban, :bic, :country, :bank_code, :bank_name, "
        ":branch_id, :account_number, :sub_account_number) "
        "ON CONFLICT (unique_id) DO UPDATE SET "
        "account_name = excluded.account_name, owner_name = excluded.owner_name, "
        "bank_name = excluded.bank_name, iban = excluded.iban, bic = excluded.bic, "
        "account_number = excluded.account_number, currency = excluded.currency;");

    return statement;
}

/**
 * The state of an account is written on its own, not by the statement above.
 * That statement carries what the bank reports, and the state is not among it:
 * an account the user deselected must not become visible again merely because
 * the wizard offered it once more. Whoever wants it changed says so, which
 * main.cpp does once it has the result of the wizard.
 *
 * changed_at only moves when the value actually differs, so it records the last
 * switch rather than the last write. The comparison reads the old row; SQLite
 * evaluates every expression of an UPDATE against the values before it.
 */
const QString &accountStateUpdateQuery()
{
    static const QString statement = QStringLiteral(
        "UPDATE accounts SET "
        "changed_at = CASE WHEN active <> :state THEN datetime('now', 'localtime') "
        "ELSE changed_at END, "
        "active = :active "
        "WHERE unique_id = :unique_id;");

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
         {QStringLiteral("id"),
          QStringLiteral("type"),
          // Version 3 added both. They only come into being with the table
          // itself, so a file written before that carries neither, and naming
          // them here is what turns that into a message the user can act on
          // instead of a failing query later.
          QStringLiteral("active"),
          QStringLiteral("changed_at"),
          QStringLiteral("unique_id"),
          QStringLiteral("backend_name"),
          QStringLiteral("owner_name"),
          QStringLiteral("account_name"),
          QStringLiteral("currency"),
          QStringLiteral("memo"),
          QStringLiteral("iban"),
          QStringLiteral("bic"),
          QStringLiteral("country"),
          QStringLiteral("bank_code"),
          QStringLiteral("bank_name"),
          QStringLiteral("branch_id"),
          QStringLiteral("account_number"),
          QStringLiteral("sub_account_number"),
          QStringLiteral("balance")}},
        {QStringLiteral("balances"),
         {QStringLiteral("id"),
          QStringLiteral("account_id"),
          QStringLiteral("date"),
          QStringLiteral("value"),
          QStringLiteral("type"),
          QStringLiteral("currency")}},
        {QStringLiteral("refaccounts"),
         {QStringLiteral("id"),
          QStringLiteral("account_id"),
          QStringLiteral("account_type"),
          QStringLiteral("owner_name"),
          QStringLiteral("owner_name2"),
          QStringLiteral("account_name"),
          QStringLiteral("iban"),
          QStringLiteral("bic"),
          QStringLiteral("country"),
          QStringLiteral("bank_code"),
          QStringLiteral("account_number"),
          QStringLiteral("sub_account_number")}},
        {QStringLiteral("migrations"),
         {QStringLiteral("id"),
          QStringLiteral("name"),
          QStringLiteral("migrated"),
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

/**
 * What the worker thread of receiveItems hands back. QPromise carries one type,
 * and a read can end in either of two ways, so both travel together. Both parts
 * are default constructible, which QFuture wants of what it stores.
 */
struct ReadResult
{
    BankingItems items;
    Error error;

    /**
     * How many records satisfy the condition of the query, the whole holding
     * rather than the window that was read. Negative when the run ended before
     * it could count, which is the one case where nothing is reported.
     */
    int matched = -1;
};

/**
 * The condition a read stands under, as a fragment of SQL and the values that
 * fragment binds. It carries no value of its own: everything the caller asked
 * for reaches the statement through a binding, and only the column names are
 * written into the text.
 *
 * Three queries of one read share it. The records, the size of the window for
 * the progress, and the number the filter bar shows all have to stand under the
 * same condition, or they contradict each other.
 */
struct ReadCondition
{
    QString where;
    QMap<QString, QVariant> bindings;
};

/**
 * What the worker thread of storeItems hands back. The count travels with the
 * error because it is needed on both paths: whoever tells the user that the
 * setup did not finish has to say how many accounts did go in.
 */
struct WriteResult
{
    int stored = 0;
    Error error;
};

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
        // Raised before anything is torn down. A read that is still going keeps
        // its own number and is answered as stale when it comes back.
        ++m_readGeneration;

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
     * Puts a query on the given database and applies the decryption key to it.
     * Whoever gets no error back holds a query on a readable database; whoever
     * gets one must not carry on, every statement would then fail with a message
     * that does not name the cause.
     *
     * QSqlQuery cannot be copied, so the query is handed in rather than returned.
     *
     * Static and taking the database, because the worker thread of receiveItems
     * has one of its own and must not touch the connection of this object. A
     * QSqlDatabase belongs to the thread that created it.
     */
    [[nodiscard]] static Error openQueryOn(const QSqlDatabase &database,
                                           const QString &key,
                                           const QString &fileName,
                                           QSqlQuery &query)
    {
        query = QSqlQuery(database);
        if (!query.exec(QStringLiteral("PRAGMA key=") + keyLiteral(key) + QLatin1Char(';'))) {
            // The message of the driver can carry the key on this statement.
            // Only the file is named, never the reason verbatim.
            return Error(ErrorCode::PermissionDenied,
                         QStringLiteral("Could not apply the key to %1").arg(fileName));
        }

        return {};
    }

    [[nodiscard]] Error openQuery(QSqlQuery &query)
    {
        if (m_connection == nullptr) {
            return Error(ErrorCode::DatabaseFailure,
                         QStringLiteral("No storage connection for %1").arg(m_storageFileName));
        }

        return openQueryOn(m_connection->database(), m_key, m_storageFileName, query);
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
     * What a read asks the table for besides the window. The account is filtered
     * on unique_account_id, the identifier the institution assigns, and that
     * column exists on transactions alone; receiveItems refuses the filter on
     * any other type before a run is started.
     */
    static ReadCondition conditionOf(const Storage::ItemQuery &query)
    {
        auto condition = ReadCondition();
        auto parts = QStringList();

        if (query.accountId != 0) {
            parts << QStringLiteral("unique_account_id = :accountId");
            condition.bindings[QStringLiteral(":accountId")] = query.accountId;
        }

        if (!query.text.isEmpty()) {
            parts << QStringLiteral(
                "(remote_name LIKE :text ESCAPE '\\' OR purpose LIKE :text ESCAPE '\\')");
            condition.bindings[QStringLiteral(":text")] = QStringLiteral("%%%1%%").arg(
                escapedForLike(query.text));
        }

        // Both bounds are inclusive. An invalid date leaves its side open rather
        // than standing for today or for the beginning of time.
        if (query.from.isValid()) {
            parts << QStringLiteral("date >= :from");
            condition.bindings[QStringLiteral(":from")] = query.from;
        }

        if (query.to.isValid()) {
            parts << QStringLiteral("date <= :to");
            condition.bindings[QStringLiteral(":to")] = query.to;
        }

        // The sign of the value is what tells the direction. A booking of nought
        // is neither, and neither restriction lets it through.
        switch (query.direction) {
        case Storage::Direction::Incoming:
            parts << QStringLiteral("value > 0");
            break;
        case Storage::Direction::Outgoing:
            parts << QStringLiteral("value < 0");
            break;
        case Storage::Direction::Any:
            break;
        }

        if (!parts.isEmpty()) {
            condition.where = QStringLiteral(" WHERE ") + parts.join(QStringLiteral(" AND "));
        }

        return condition;
    }

    /**
     * Runs one COUNT statement and hands back its number. The two counts of a
     * read differ in their statement alone; the key, the preparation and the
     * bindings are the same for both.
     */
    static Result<int> countOn(const QSqlDatabase &database,
                               const QString &key,
                               const QString &fileName,
                               const QString &table,
                               const QString &statement,
                               const QMap<QString, QVariant> &bindings)
    {
        QSqlQuery query;
        if (const auto error = openQueryOn(database, key, fileName, query); error.isError()) {
            return error;
        }

        if (!query.prepare(statement)) {
            return Error(ErrorCode::DatabaseFailure,
                         QStringLiteral("Could not prepare the row count of %1: %2")
                             .arg(table, query.lastError().text()));
        }

        for (const auto &[name, value] : bindings.asKeyValueRange()) {
            query.bindValue(name, value);
        }

        if (!query.exec() || !query.next()) {
            return Error(ErrorCode::DatabaseFailure,
                         QStringLiteral("Could not count the rows of %1: %2")
                             .arg(table, query.lastError().text()));
        }

        return query.value(0).toInt();
    }

    /**
     * The number of rows the given window actually yields. QSqlQuery::size() is
     * unavailable for SQLite and numRowsAffected() is undefined for a SELECT, so
     * the count comes from a query of its own. It serves the progress.
     */
    static Result<int> windowedRowCountOn(const QSqlDatabase &database,
                                          const QString &key,
                                          const QString &fileName,
                                          const QString &table,
                                          const ReadCondition &condition,
                                          int offset,
                                          int limit)
    {
        if (!isKnownTable(table)) {
            return Error(ErrorCode::InvalidInput, QStringLiteral("Unknown table %1").arg(table));
        }

        auto bindings = condition.bindings;
        bindings[QStringLiteral(":limit")] = limit;
        bindings[QStringLiteral(":offset")] = offset;

        // The table name is interpolated because SQL knows no binding for an
        // identifier. It passed the list above. The window and the condition are
        // bound.
        return countOn(database,
                       key,
                       fileName,
                       table,
                       QStringLiteral(
                           "SELECT COUNT(*) FROM (SELECT 1 FROM %1%2 LIMIT :limit OFFSET :offset);")
                           .arg(table, condition.where),
                       bindings);
    }

    /**
     * How many rows of the table satisfy the condition, without the window. This
     * is the number the filter bar shows, and it is the one thing the count
     * above cannot answer: a window of fifty says nothing about three thousand.
     */
    static Result<int> matchingRowCountOn(const QSqlDatabase &database,
                                          const QString &key,
                                          const QString &fileName,
                                          const QString &table,
                                          const ReadCondition &condition)
    {
        if (!isKnownTable(table)) {
            return Error(ErrorCode::InvalidInput, QStringLiteral("Unknown table %1").arg(table));
        }

        // The table name is interpolated because SQL knows no binding for an
        // identifier. It passed the list above. The condition is bound.
        return countOn(database,
                       key,
                       fileName,
                       table,
                       QStringLiteral("SELECT COUNT(*) FROM %1%2;").arg(table, condition.where),
                       condition.bindings);
    }

    /**
     * Reads a window of one table, in a thread of its own.
     *
     * The connection is cloned rather than shared. A QSqlDatabase may only be
     * used by the thread that created it, and cloneDatabase taking the name
     * instead of the object is the way Qt offers for exactly this case: it
     * copies the settings without the caller touching the other thread's handle.
     *
     * Everything it needs is passed by value. Nothing here reads a member, so
     * there is no object left to outlive the run.
     */
    static void readItems(QPromise<ReadResult> &promise,
                          const QString &sourceConnectionName,
                          const QString &key,
                          const QString &fileName,
                          const QString &table,
                          QMap<int, QString> columnList,
                          Storage::ItemQuery itemQuery)
    {
        // matched stays negative on every path that ends before the count was
        // taken. Only then is the caller told nothing about it.
        const auto fail = [&promise](ErrorCode code, const QString &message, int matched = -1) {
            promise.addResult(ReadResult{{}, Error(code, message), matched});
        };

        const auto condition = conditionOf(itemQuery);

        // A name of its own, so that the two connections never collide. The one
        // of StorageConnection already carries a random number.
        const auto workerConnectionName = sourceConnectionName + QStringLiteral("_reader");

        // Called rather than written out, so that every way out of it releases
        // the query and the handle before removeDatabase runs below. Qt warns
        // and leaks the connection while either still refers to it, and a plain
        // block could not do it: a return inside one leaves the function and
        // skips what follows the block.
        [&] {
            QSqlDatabase database = QSqlDatabase::cloneDatabase(sourceConnectionName,
                                                                workerConnectionName);
            if (!database.isValid() || !database.open()) {
                fail(ErrorCode::DatabaseFailure,
                     QStringLiteral("Could not open a second connection to %1").arg(fileName));
                return;
            }

            QSqlQuery query;
            if (const auto error = openQueryOn(database, key, fileName, query); error.isError()) {
                fail(error.code(), error.message());
                database.close();
                return;
            }

            // The order by the row id is not a preference. Without it SQLite is
            // free to hand two windows back in an order of its own, and a record
            // could then fall between them or appear in both. A chosen column
            // stands in front of it and never in its place, because two records
            // may well carry the same date or the same amount.
            const auto sortName = sortColumnName(itemQuery.sort);
            const auto orderBy = sortName.isEmpty()
                                     ? QStringLiteral("ORDER BY id ASC")
                                     : QStringLiteral("ORDER BY %1 %2, id ASC")
                                           .arg(sortName,
                                                itemQuery.order == Qt::DescendingOrder
                                                    ? QStringLiteral("DESC")
                                                    : QStringLiteral("ASC"));

            // Three identifiers are interpolated because SQL knows no binding for
            // one: the table, the column that is ordered by, and the direction.
            // The table comes from the switch in receiveItems and has passed the
            // list in tableColumns, which answers empty for a name it does not
            // know. The other two come from sortColumnName and from a comparison
            // against one value of Qt::SortOrder, so neither can carry anything a
            // caller wrote. The window and the condition are bound.
            const auto statement = QStringLiteral(
                                       "SELECT * FROM %1%2 %3 LIMIT :limit OFFSET :offset;")
                                       .arg(table, condition.where, orderBy);

            if (!query.prepare(statement)) {
                fail(ErrorCode::DatabaseFailure,
                     QStringLiteral("Could not prepare the read of %1: %2")
                         .arg(table, query.lastError().text()));
                database.close();
                return;
            }

            for (const auto &[name, value] : condition.bindings.asKeyValueRange()) {
                query.bindValue(name, value);
            }

            query.bindValue(QStringLiteral(":limit"), itemQuery.limit);
            query.bindValue(QStringLiteral(":offset"), itemQuery.offset);

            if (!query.exec()) {
                fail(ErrorCode::DatabaseFailure,
                     QStringLiteral("Could not read the table %1: %2")
                         .arg(table, query.lastError().text()));
                database.close();
                return;
            }

            // numRowsAffected() is undefined for a SELECT and SQLite answers -1,
            // which turned the progress negative. The count comes from a query of
            // its own. A failure there costs the progress reporting, not the read.
            const auto rowCount = windowedRowCountOn(database,
                                                     key,
                                                     fileName,
                                                     table,
                                                     condition,
                                                     itemQuery.offset,
                                                     itemQuery.limit);
            if (!rowCount.hasValue()) {
                qCWarning(lcStorage) << "no progress reporting:" << rowCount.error().message();
            }

            const int totalRows = rowCount.hasValue() ? rowCount.value() : 0;

            // The number the filter bar shows. It stands under the same condition
            // as the read and counts the whole holding, which is what the count
            // above cannot do. A failure costs the number, not the records, so
            // the run carries on and reports nothing rather than something wrong.
            const auto matchCount = matchingRowCountOn(database, key, fileName, table, condition);
            if (!matchCount.hasValue()) {
                qCWarning(lcStorage) << "no record count:" << matchCount.error().message();
            }

            const int matched = matchCount.hasValue() ? matchCount.value() : -1;

            promise.setProgressRange(0, 100);

            // The rows are collected first. An account needs a second read for
            // its balance and its reference accounts, and that read cannot run
            // while this query is still stepping over its own result.
            auto rows = QList<QMap<QString, QVariant>>();
            auto map = QMap<QString, QVariant>();

            while (query.next()) {
                for (const auto &[key_, value] : columnList.asKeyValueRange()) {
                    map[value] = query.value(key_);
                }

                rows << map;

                // No clear on purpose. Every row sets the same keys, so the
                // inserts of the next round turn into assignments.
            }

            auto bankingItems = BankingItems();
            int index = 0;

            for (auto &row : rows) {
                switch (itemQuery.type) {
                case Storage::StorageAccount:
                    enrichAccountRowOn(database, key, fileName, row);
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
                    promise.setProgressValue(qMin(index * 100 / totalRows, 100));
                }
            }

            if (bankingItems.isEmpty()) {
                fail(ErrorCode::NotFound,
                     QStringLiteral("No items found in the table %1").arg(table),
                     matched);
            } else {
                qCDebug(lcStorage) << "read" << bankingItems.size() << "items from" << table;
                promise.addResult(ReadResult{bankingItems, Error(), matched});
            }

            database.close();
        }();

        QSqlDatabase::removeDatabase(workerConnectionName);
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
    static Result<QVariant> insertRowOn(const QSqlDatabase &database,
                                        const QString &key,
                                        const QString &fileName,
                                        const QString &statement,
                                        const QMap<QString, QVariant> &map,
                                        const QString &type)
    {
        QSqlQuery query;
        if (const auto error = openQueryOn(database, key, fileName, query); error.isError()) {
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
                qCWarning(lcStorage)
                    << "property" << key << "of type" << type << "has no column and is not stored";
                continue;
            }

            query.bindValue(QLatin1Char(':') + key, value);
        }

        if (!query.exec()) {
            return Error(ErrorCode::DatabaseFailure,
                         QStringLiteral("Could not store an item of type %1: %2")
                             .arg(type, database.lastError().text()));
        }

        return query.lastInsertId();
    }

    /**
     * Writes one record, whatever its type. The whole write path is static and
     * takes the database, because storeItems runs it in a thread of its own on a
     * connection of its own; a QSqlDatabase belongs to the thread that created
     * it. Storage::storeItem hands in the connection of this object and adds the
     * signals, this function emits none.
     */
    static Error storeItemOn(const QSqlDatabase &database,
                             const QString &key,
                             const QString &fileName,
                             const BankingItem *bankingItem)
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

        if (type == QLatin1StringView("Account")) {
            return storeAccountOn(database, key, fileName, bankingItem->toMap());
        }

        if (type == QLatin1StringView("Transaction")) {
            const auto result = insertRowOn(database,
                                            key,
                                            fileName,
                                            transactionInsertQuery(),
                                            bankingItem->toMap(),
                                            type);
            return result.hasValue() ? Error() : result.error();
        }

        // Category and Contact have no table of their own yet. The branch used to
        // be empty, which sent an unprepared query on its way.
        return Error(ErrorCode::NotImplemented,
                     QStringLiteral("Storing an item of type %1 is not implemented").arg(type));
    }

    /**
     * Writes a run of records, in a thread of its own.
     *
     * The connection is cloned rather than shared, for the same reason readItems
     * clones it. Everything the run needs is passed by value, the records
     * included: they are shared pointers, so the run holds them alive on its own.
     *
     * The bracket sits around the single record. What went in stays in, and the
     * run ends at the first failure rather than carrying on over a record that
     * may be the cause.
     */
    static void writeItems(QPromise<WriteResult> &promise,
                           const QString &sourceConnectionName,
                           const QString &key,
                           const QString &fileName,
                           BankingItems items)
    {
        // A name of its own, so that the two connections never collide. The one
        // of StorageConnection already carries a random number.
        const auto workerConnectionName = sourceConnectionName + QStringLiteral("_writer");

        int stored = 0;

        // Called rather than written out, so that every way out of it releases
        // the handle before removeDatabase runs below. Qt warns and leaks the
        // connection while anything still refers to it, and a plain block could
        // not do it: a return inside one leaves the whole function and skips
        // what follows the block. That is what happened here on the way out that
        // finds no second connection. The read path is built the same way.
        [&] {
            QSqlDatabase database = QSqlDatabase::cloneDatabase(sourceConnectionName,
                                                                workerConnectionName);
            if (!database.isValid() || !database.open()) {
                promise.addResult(
                    WriteResult{0,
                                Error(ErrorCode::DatabaseFailure,
                                      QStringLiteral("Could not open a second connection to %1")
                                          .arg(fileName))});
                return;
            }

            promise.setProgressRange(0, 100);

            auto error = Error();

            for (const auto &item : std::as_const(items)) {
                error = storeItemOn(database, key, fileName, item.get());
                if (error.isError()) {
                    break;
                }

                ++stored;
                promise.setProgressValue(qMin(stored * 100 / items.size(), 100));
            }

            promise.addResult(WriteResult{stored, error});

            database.close();
        }();

        QSqlDatabase::removeDatabase(workerConnectionName);
    }

    /**
     * An account spans three tables: its own row, the balance that belongs to it
     * and the reference accounts held with it. Either all three are written or
     * none is, so that no account ends up carrying the balance of an older write.
     */
    static Error storeAccountOn(QSqlDatabase database,
                                const QString &key,
                                const QString &fileName,
                                const QMap<QString, QVariant> &map)
    {
        auto accountMap = map;

        // The first two are kept in tables of their own, the state is written by
        // a statement of its own. Left in place they would be reported as
        // columnless properties on every single write.
        const auto balance = accountMap.take(QStringLiteral("balance"));
        const auto referenceAccounts = accountMap.take(QStringLiteral("refAccounts"));
        const auto active = accountMap.take(QStringLiteral("active"));
        const auto uniqueId = accountMap.value(QStringLiteral("unique_id"));

        // The transaction is taken on the handle that was passed in, not on the
        // connection of this object. The worker thread of storeItems holds one of
        // its own, and a transaction belongs to the connection it was begun on.
        // The handle is taken by value: QSqlDatabase shares its connection, so
        // the copy drives the same one and transaction() is not const.
        if (!database.transaction()) {
            return Error(ErrorCode::DatabaseFailure,
                         QStringLiteral("Could not begin a transaction on %1: %2")
                             .arg(fileName, database.lastError().text()));
        }

        if (const auto written = insertRowOn(database,
                                             key,
                                             fileName,
                                             accountInsertQuery(),
                                             accountMap,
                                             QStringLiteral("Account"));
            !written.hasValue()) {
            return rollbackOn(database, written.error());
        }

        // The upsert may have taken its update branch, and SQLite leaves
        // sqlite3_last_insert_rowid() where it was for that. The id insertRow
        // hands back would then belong to whichever account was inserted last,
        // and the balance and the reference accounts would hang on that one.
        const auto accountId = accountIdOfOn(database, key, fileName, uniqueId);
        if (!accountId.hasValue()) {
            return rollbackOn(database, accountId.error());
        }

        if (const auto error = storeAccountStateOn(database, key, fileName, uniqueId, active);
            error.isError()) {
            return rollbackOn(database, error);
        }

        if (const auto error
            = storeBalanceOn(database, key, fileName, accountId.value(), balance, accountMap);
            error.isError()) {
            return rollbackOn(database, error);
        }

        if (const auto error = storeReferenceAccountsOn(database,
                                                        key,
                                                        fileName,
                                                        accountId.value(),
                                                        referenceAccounts);
            error.isError()) {
            return rollbackOn(database, error);
        }

        if (!database.commit()) {
            return Error(ErrorCode::DatabaseFailure,
                         QStringLiteral("Could not commit an account to %1: %2")
                             .arg(fileName, database.lastError().text()));
        }

        return {};
    }

    /**
     * The id of the row that carries this unique id. Read rather than taken from
     * the write, because an upsert that updated an existing account reports no
     * new row id.
     */
    static Result<QVariant> accountIdOfOn(const QSqlDatabase &database,
                                          const QString &key,
                                          const QString &fileName,
                                          const QVariant &uniqueId)
    {
        QSqlQuery query;
        if (const auto error = openQueryOn(database, key, fileName, query); error.isError()) {
            return error;
        }

        if (!query.prepare(
                QStringLiteral("SELECT id FROM accounts WHERE unique_id = :unique_id;"))) {
            return Error(ErrorCode::DatabaseFailure,
                         QStringLiteral("Could not prepare the lookup of an account: %1")
                             .arg(query.lastError().text()));
        }

        query.bindValue(QStringLiteral(":unique_id"), uniqueId);

        if (!query.exec() || !query.next()) {
            return Error(ErrorCode::DatabaseFailure,
                         QStringLiteral("Could not find the account just written to %1: %2")
                             .arg(fileName, query.lastError().text()));
        }

        return query.value(0);
    }

    /**
     * Sets whether the user keeps the account. An account handed in without the
     * property keeps the state the row already carries, which is what the
     * default of the column gives a row that was never touched.
     */
    static Error storeAccountStateOn(const QSqlDatabase &database,
                                     const QString &key,
                                     const QString &fileName,
                                     const QVariant &uniqueId,
                                     const QVariant &active)
    {
        if (!active.isValid()) {
            return {};
        }

        QSqlQuery query;
        if (const auto error = openQueryOn(database, key, fileName, query); error.isError()) {
            return error;
        }

        if (!query.prepare(accountStateUpdateQuery())) {
            return Error(ErrorCode::DatabaseFailure,
                         QStringLiteral("Could not prepare the state of an account: %1")
                             .arg(query.lastError().text()));
        }

        // The statement names the value twice, once to compare against the old
        // row and once to write. Two placeholders rather than one repeated,
        // because a driver is free to bind a repeated name only once.
        query.bindValue(QStringLiteral(":state"), active);
        query.bindValue(QStringLiteral(":active"), active);
        query.bindValue(QStringLiteral(":unique_id"), uniqueId);

        if (!query.exec()) {
            return Error(ErrorCode::DatabaseFailure,
                         QStringLiteral("Could not store the state of an account in %1: %2")
                             .arg(fileName, database.lastError().text()));
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
    static Error storeBalanceOn(const QSqlDatabase &database,
                                const QString &key,
                                const QString &fileName,
                                const QVariant &accountId,
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

        const auto result = insertRowOn(database,
                                        key,
                                        fileName,
                                        balanceInsertQuery(),
                                        balanceMap,
                                        QStringLiteral("Balance"));

        return result.hasValue() ? Error() : result.error();
    }

    /**
     * Reference accounts belong to the account that holds them. An account is
     * written whole, so the rows of an earlier write go first.
     *
     * Ownership: the entries are shared. Whichever holder goes last releases
     * them, so an early return from this function leaks nothing.
     */
    static Error storeReferenceAccountsOn(const QSqlDatabase &database,
                                          const QString &key,
                                          const QString &fileName,
                                          const QVariant &accountId,
                                          const QVariant &referenceAccounts)
    {
        if (!referenceAccounts.canConvert<ReferenceAccounts>()) {
            return {};
        }

        const auto accounts = qvariant_cast<ReferenceAccounts>(referenceAccounts);

        QSqlQuery query;
        if (const auto error = openQueryOn(database, key, fileName, query); error.isError()) {
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

        for (const auto &referenceAccount : std::as_const(accounts)) {
            if (referenceAccount == nullptr || !referenceAccount->isValid()) {
                continue;
            }

            auto map = referenceAccount->toMap();
            map[QStringLiteral("account_id")] = accountId;

            const auto result = insertRowOn(database,
                                            key,
                                            fileName,
                                            referenceAccountInsertQuery(),
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
    static void enrichAccountRowOn(const QSqlDatabase &database,
                                   const QString &key,
                                   const QString &fileName,
                                   QMap<QString, QVariant> &row)
    {
        const auto accountId = row.value(QStringLiteral("id"));
        if (!accountId.isValid()) {
            return;
        }

        QSqlQuery query;
        if (const auto error = openQueryOn(database, key, fileName, query); error.isError()) {
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

        // The entries are shared with Account::fromMap, which copies their values
        // into the account spec and then lets go of them.
        auto referenceAccounts = ReferenceAccounts();
        const auto record = query.record();

        while (query.next()) {
            auto referenceMap = QMap<QString, QVariant>();
            for (int column = 0; column < record.count(); ++column) {
                referenceMap[record.fieldName(column)] = query.value(column);
            }

            if (auto referenceAccount = ReferenceAccount::fromMap(referenceMap)) {
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
    static Error rollbackOn(QSqlDatabase database, const Error &error)
    {
        if (!database.rollback()) {
            qCWarning(lcStorage) << "could not roll back after" << error.message() << ":"
                                 << database.lastError().text();
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
            queries << QString(statement).replace(QStringLiteral("#"), QStringLiteral(";")).trimmed();
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
                    qCWarning(lcStorage)
                        << "could not roll back the failed schema statement:" << lastErrorMessage();
                }

                // The statement itself stays out of the message, it goes to the
                // log only. It carries no secret, but it is of no use to a user.
                qCCritical(lcStorage)
                    << "schema statement failed:" << sqlStatement << query.lastError().text();

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

    /**
     * Watches the run started by receiveItems. It lives in the thread of the
     * Storage that owns it, which is what turns the progress and the completion
     * of the worker back into signals of that thread.
     *
     * A member rather than a local, because a watcher destroyed while its future
     * is still running waits for it, which would make the call blocking again.
     */
    QFutureWatcher<ReadResult> m_readWatcher;

    /**
     * Tells the run that is going from the one that was going before the storage
     * was closed. Closing raises it, and a result that comes back under an older
     * number belongs to a file nobody has open any more.
     */
    quint64 m_readGeneration = 0;

    /**
     * The same for the run started by storeItems. A watcher of its own rather
     * than a shared one, because the two carry different results and a run of
     * either kind must not cancel the other.
     */
    QFutureWatcher<WriteResult> m_writeWatcher;

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

Error Storage::setKey(const QString &key)
{
    // The length, not the full guideline. The character classes of
    // minPasswordGuidelines end in [A-Za-z\d<special>], which no CJK character
    // and no emoji is a member of, however many of the lookaheads a pass phrase
    // built from them satisfies. Enforcing the whole pattern here would lock out
    // exactly the keys the storage was taught to carry unmangled, and would shut
    // the door on every file created under an older, weaker rule. The classes
    // are checked where a key is chosen, in the dialog; what cannot open a file
    // at all is checked here.
    //
    // The length is counted in UTF-16 units, as QString counts it. A character
    // outside the basic multilingual plane, an emoji among them, therefore
    // counts as two. That is the same measure the guideline pattern applies.
    if (key.length() < MinPasswordLength || key.length() > MaxPasswordLength) {
        // The key itself never reaches the message.
        return Error(ErrorCode::InvalidInput,
                     QStringLiteral("The key has to be between %1 and %2 characters long")
                         .arg(MinPasswordLength)
                         .arg(MaxPasswordLength));
    }

    d_ptr->setKey(key);

    return {};
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
                     QStringLiteral("Could not change the key of %1").arg(d_ptr->storageFileName()));
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

int Storage::minPasswordLength() const
{
    return MinPasswordLength;
}

Error Storage::storeItem(const BankingItem *bankingItem)
{
    if (d_ptr->connection() == nullptr) {
        // The write path used to reach for the connection without asking. A call
        // before initialize took the whole application down with it.
        return Error(ErrorCode::DatabaseFailure,
                     QStringLiteral("No storage connection for %1").arg(d_ptr->storageFileName()));
    }

    const auto error = Private::storeItemOn(d_ptr->connection()->database(),
                                            d_ptr->m_key,
                                            d_ptr->storageFileName(),
                                            bankingItem);

    if (error.isError()) {
        qCCritical(lcStorage) << error.message();

        Q_EMIT errorOccurred(error.code(), error.message());
        Q_EMIT finished();

        return error;
    }

    qCDebug(lcStorage) << "stored an item of type" << bankingItem->itemType();

    Q_EMIT finished();

    return {};
}

void Storage::storeItems(const BankingItems &items)
{
    const auto reportError = [this](ErrorCode code, const QString &message) {
        qCCritical(lcStorage) << message;

        Q_EMIT errorOccurred(code, message);
        Q_EMIT finished();
    };

    if (d_ptr->connection() == nullptr || !d_ptr->connection()->isOpen()) {
        reportError(ErrorCode::DatabaseFailure,
                    QStringLiteral("No open storage connection for %1")
                        .arg(d_ptr->storageFileName()));
        return;
    }

    // A second run while one is still going would be a second transaction on the
    // same file, and the watcher of the first would be lost.
    if (d_ptr->m_writeWatcher.isRunning()) {
        reportError(ErrorCode::InvalidInput,
                    QStringLiteral("A write to the storage is already running"));
        return;
    }

    // A run without records is not a failure. Nothing is started, and the caller
    // still gets its end.
    if (items.isEmpty()) {
        Q_EMIT itemsStored(0);
        Q_EMIT finished();
        return;
    }

    const auto sourceConnectionName = d_ptr->connection()->database().connectionName();

    // The connections are made before the run is started. A short write could
    // otherwise finish before anyone is listening.
    QObject::disconnect(&d_ptr->m_writeWatcher, nullptr, this, nullptr);

    connect(&d_ptr->m_writeWatcher,
            &QFutureWatcher<WriteResult>::progressValueChanged,
            this,
            [this](int progress) { Q_EMIT progressChanged(progress); });

    connect(&d_ptr->m_writeWatcher, &QFutureWatcher<WriteResult>::finished, this, [this]() {
        const auto future = d_ptr->m_writeWatcher.future();
        if (future.resultCount() == 0) {
            // Cannot happen through writeItems, which reports on every path. A
            // cancelled future can end here, and a silent return would leave the
            // caller waiting for a signal that never comes.
            qCCritical(lcStorage) << "the storage write ended without a result";

            Q_EMIT errorOccurred(ErrorCode::DatabaseFailure,
                                 QStringLiteral("The storage write ended without a result"));
            Q_EMIT itemsStored(0);
            Q_EMIT finished();
            return;
        }

        const auto result = future.result();
        if (result.error.isError()) {
            qCCritical(lcStorage) << result.error.message();

            Q_EMIT errorOccurred(result.error.code(), result.error.message());
        } else {
            qCDebug(lcStorage) << "stored" << result.stored << "items";
        }

        // The count goes out on both paths. A failure has to be reported with
        // the number of items that made it, not on its own.
        Q_EMIT itemsStored(result.stored);
        Q_EMIT finished();
    });

    d_ptr->m_writeWatcher.setFuture(QtConcurrent::run(&Private::writeItems,
                                                      sourceConnectionName,
                                                      d_ptr->m_key,
                                                      d_ptr->storageFileName(),
                                                      items));
}

void Storage::receiveItems(const ItemQuery &query)
{
    const auto reportError = [this](ErrorCode code, const QString &message) {
        qCCritical(lcStorage) << message;

        Q_EMIT errorOccurred(code, message);
        Q_EMIT finished();
    };

    // The window used to travel into the statement unchecked. A negative offset
    // or a limit of INT_MAX is not a query anyone meant to run.
    if (query.limit < 1 || query.limit > MaxItemsPerQuery || query.offset < 0) {
        reportError(ErrorCode::InvalidInput,
                    QStringLiteral("Invalid window: limit=%1 offset=%2")
                        .arg(query.limit)
                        .arg(query.offset));
        return;
    }

    // The account goes over transactions.unique_account_id, the identifier the
    // institution assigns. No other table carries that column: a reference
    // account hangs on the row id of accounts, which no type of the core hands
    // out. Both identifiers are quint32, so a filter on another type would be a
    // mix-up nothing else could catch.
    //
    // The three of the filter bar go over columns of the transactions table as
    // well, and are refused on another type for the same reason: silently
    // dropping them would answer a narrower question with the wider holding.
    const bool filtered = query.accountId != 0 || !query.text.isEmpty() || query.from.isValid()
                          || query.to.isValid() || query.direction != Direction::Any;

    if (filtered && query.type != StorageTransaction) {
        reportError(ErrorCode::InvalidInput,
                    QStringLiteral("A filter is defined for transactions only, not for %1")
                        .arg(QString::fromUtf8(
                            QMetaEnum::fromType<Storage::Type>().valueToKey(query.type))));
        return;
    }

    auto table = QString();
    switch (query.type) {
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
                            QMetaEnum::fromType<Storage::Type>().valueToKey(query.type))));
        return;
    }

    const auto columnList = d_ptr->tableColumns(table);
    if (columnList.isEmpty()) {
        reportError(ErrorCode::DatabaseFailure,
                    QStringLiteral("No columns found for the table %1").arg(table));
        return;
    }

    if (d_ptr->connection() == nullptr || !d_ptr->connection()->isOpen()) {
        reportError(ErrorCode::DatabaseFailure,
                    QStringLiteral("No open storage connection for %1")
                        .arg(d_ptr->storageFileName()));
        return;
    }

    // A second run while one is still going would open a second reader and lose
    // the watcher of the first. Nothing in the application does it, and this
    // says so instead of leaving it to chance.
    if (d_ptr->m_readWatcher.isRunning()) {
        reportError(ErrorCode::InvalidInput,
                    QStringLiteral("A read of the storage is already running"));
        return;
    }

    // Everything above is cheap and answers a programming error at once. What
    // follows is the part that reads the file, and it is what must not sit in
    // the calling thread: a window of a thousand accounts costs a second query
    // per account for its balance and its reference accounts.
    const auto sourceConnectionName = d_ptr->connection()->database().connectionName();

    // The connections are made before the run is started. A short read could
    // otherwise finish before anyone is listening.
    QObject::disconnect(&d_ptr->m_readWatcher, nullptr, this, nullptr);

    connect(&d_ptr->m_readWatcher,
            &QFutureWatcher<ReadResult>::progressValueChanged,
            this,
            [this](int progress) { Q_EMIT progressChanged(progress); });

    const quint64 generation = d_ptr->m_readGeneration;

    connect(&d_ptr->m_readWatcher, &QFutureWatcher<ReadResult>::finished, this, [this, generation]() {
        if (generation != d_ptr->m_readGeneration) {
            // The storage was closed while this run was going, and the file it
            // read is not the one that is open now. Handing the records on would
            // show the accounts of the previous storage under the name of the
            // current one. The completion is still reported, so that nobody
            // waits for a run that is over.
            qCInfo(lcStorage) << "dropping the result of a read that outlived its storage";

            Q_EMIT finished();
            return;
        }

        const auto future = d_ptr->m_readWatcher.future();
        if (future.resultCount() == 0) {
            // Cannot happen through readItems, which reports on every path. A
            // cancelled future can end here, and a silent return would leave the
            // caller waiting for a signal that never comes.
            qCCritical(lcStorage) << "the storage read ended without a result";

            Q_EMIT errorOccurred(ErrorCode::DatabaseFailure,
                                 QStringLiteral("The storage read ended without a result"));
            Q_EMIT finished();
            return;
        }

        const auto result = future.result();

        // Before the records and before a failure. Whoever picks what to show
        // when nothing was found needs the number at that moment, and a filter
        // that matches nothing ends in exactly that pair: zero, then the report
        // that the table held nothing. A run that never reached the table has no
        // number to give.
        if (result.matched >= 0) {
            Q_EMIT itemsCounted(result.matched);
        }

        if (result.error.isError()) {
            qCCritical(lcStorage) << result.error.message();

            Q_EMIT errorOccurred(result.error.code(), result.error.message());
            Q_EMIT finished();
            return;
        }

        Q_EMIT itemsReceived(result.items);
        Q_EMIT finished();
    });

    d_ptr->m_readWatcher.setFuture(QtConcurrent::run(&Private::readItems,
                                                     sourceConnectionName,
                                                     d_ptr->m_key,
                                                     d_ptr->storageFileName(),
                                                     table,
                                                     columnList,
                                                     query));
}
