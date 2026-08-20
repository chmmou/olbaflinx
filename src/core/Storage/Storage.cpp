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
#include "core/Banking/Balance/Balance.h"
#include "core/Banking/StandingOrder/StandingOrder.h"
#include "core/Banking/Transaction/Transaction.h"
#include "core/Logging.h"
#include "core/Result.h"

#include <QtConcurrent/QtConcurrentRun>

#include <QtCore/QCryptographicHash>
#include <QtCore/QDate>
#include <QtCore/QDateTime>
#include <QtCore/QFile>
#include <QtCore/QFutureWatcher>
#include <QtCore/QMetaEnum>
#include <QtCore/QMetaObject>
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

#include <algorithm>
#include <memory>
#include <utility>

using namespace olbaflinx::core;
using namespace olbaflinx::core::storage;
using namespace olbaflinx::core::banking;
using namespace olbaflinx::core::banking::account;
using namespace olbaflinx::core::banking::standingorder;
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
 * How long a statement waits for a lock another connection of this storage
 * holds. Long enough to sit out a page of a read or the commit of a fetch,
 * short enough that a caller which really is stuck says so within a few
 * seconds instead of standing forever.
 */
constexpr int LockWaitMs = 5000;

/**
 * Password regular expression
 *
 * At least one lower case English letter, a-z
 * At least one upper case English letter, A-Z
 * At least one digit, 0-9
 * At least one special character out of the class below, umlauts among them
 * Between MinPasswordLength and MaxPasswordLength characters, with the anchors
 *
 * Compiled once and kept, rather than built anew on every password check.
 */
const QRegularExpression &minPasswordPattern()
{
    // The hyphen stands last so that it counts as a literal. Between '#' and
    // '_' a character class reads it as a range from 0x23 to 0x5F, which covers
    // every digit and every capital letter: the fourth lookahead would then
    // match on those alone and ask for nothing.
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
        QStringLiteral("standing_orders"),
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
constexpr int CurrentSchemaVersion = 5;

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
 *
 * This one is for the figure a fetch brings: it carries a day and a type of its
 * own and takes the place of whatever stood there.
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

/**
 * The same table, written from the account path. That path carries neither a day
 * nor a type and puts a placeholder in both, so it must not push aside a figure
 * a fetch put there: the fetched one was chosen by its type, and a placeholder
 * would undo that choice on the next run over the account list.
 *
 * It recognises its own by that placeholder type and refreshes only that. An
 * account that is never fetched, because it has no online access, would
 * otherwise stay on the figure of the day it was set up.
 */
const QString &placeholderBalanceInsertQuery()
{
    static const QString statement = QStringLiteral(
        "INSERT INTO balances (account_id, `date`, `value`, `type`, currency) "
        "VALUES (:account_id, :date, :value, :type, :currency) "
        "ON CONFLICT (account_id) DO UPDATE SET "
        "`date` = excluded.`date`, `value` = excluded.`value`, "
        "`type` = excluded.`type`, currency = excluded.currency "
        "WHERE balances.`type` = excluded.`type`;");

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
    ":commission_value, :memo, :hash) "
    "ON CONFLICT (`hash`) DO NOTHING;");

const QString &transactionInsertQuery()
{
    static const QString statement = QString::fromLatin1(TransactionInsertQueryText);

    return statement;
}

constexpr auto StandingOrderColumns = QLatin1StringView(
    "account_id, unique_account_id, fi_id, unique_id, fingerprint, identified_by, "
    "local_iban, local_bic, local_name, remote_iban, remote_bic, remote_name, `value`, "
    "currency, purpose, end_to_end_reference, period, `cycle`, execution_day, first_date, "
    "last_date, next_date, status, memo");

const QString &standingOrderInsertQuery()
{
    static const QString statement
        = QStringLiteral("INSERT INTO standing_orders (%1) VALUES (:account_id, "
                         ":unique_account_id, :fi_id, :unique_id, :fingerprint, :identified_by, "
                         ":local_iban, :local_bic, :local_name, :remote_iban, :remote_bic, "
                         ":remote_name, :value, :currency, :purpose, :end_to_end_reference, "
                         ":period, :cycle, :execution_day, :first_date, :last_date, :next_date, "
                         ":status, :memo);")
              .arg(QString(StandingOrderColumns));

    return statement;
}

/**
 * An order that is already there is written whole rather than in the fields that
 * moved: the fetch reports the current state and the row is to hold it. The mark
 * goes with it, which is how an order that comes back loses it without a path of
 * its own.
 */
const QString &standingOrderUpdateQuery()
{
    static const QString statement = QStringLiteral(
        "UPDATE standing_orders SET account_id = :account_id, "
        "unique_account_id = :unique_account_id, fi_id = :fi_id, unique_id = :unique_id, "
        "fingerprint = :fingerprint, identified_by = :identified_by, local_iban = :local_iban, "
        "local_bic = :local_bic, local_name = :local_name, remote_iban = :remote_iban, "
        "remote_bic = :remote_bic, remote_name = :remote_name, `value` = :value, "
        "currency = :currency, purpose = :purpose, "
        "end_to_end_reference = :end_to_end_reference, period = :period, `cycle` = :cycle, "
        "execution_day = :execution_day, first_date = :first_date, last_date = :last_date, "
        "next_date = :next_date, status = :status, memo = :memo, ended_at = NULL "
        "WHERE id = :id;");

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

    // The answer hangs on the statement alone, and a run asks it once per record
    // against a handful of statements. Held per thread rather than shared: a
    // writing run and the window reach this at the same time, and a lock around a
    // lookup this small would cost more than the scan it saves. It grows to the
    // number of insert statements the schema has and no further.
    thread_local QHash<QString, QSet<QString>> known;

    const auto cached = known.constFind(statement);
    if (cached != known.cend()) {
        return *cached;
    }

    auto names = QSet<QString>();

    auto matches = placeholder.globalMatch(statement);
    while (matches.hasNext()) {
        names.insert(matches.next().captured(1));
    }

    return *known.insert(statement, names);
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
        {QStringLiteral("standing_orders"),
         {QStringLiteral("id"),
          QStringLiteral("account_id"),
          QStringLiteral("unique_account_id"),
          QStringLiteral("fi_id"),
          QStringLiteral("unique_id"),
          QStringLiteral("fingerprint"),
          QStringLiteral("identified_by"),
          QStringLiteral("local_iban"),
          QStringLiteral("local_bic"),
          QStringLiteral("local_name"),
          QStringLiteral("remote_iban"),
          QStringLiteral("remote_bic"),
          QStringLiteral("remote_name"),
          QStringLiteral("value"),
          QStringLiteral("currency"),
          QStringLiteral("purpose"),
          QStringLiteral("end_to_end_reference"),
          QStringLiteral("period"),
          QStringLiteral("cycle"),
          QStringLiteral("execution_day"),
          QStringLiteral("first_date"),
          QStringLiteral("last_date"),
          QStringLiteral("next_date"),
          QStringLiteral("status"),
          QStringLiteral("ended_at"),
          QStringLiteral("memo")}},
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
 * Both queries of one read share it. The records and the number the filter bar
 * shows have to stand under the same condition, or they contradict each other.
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
    explicit Private(ApplicationInfo applicationInfo)
        : m_key()
        , m_storageFileName()
        , m_applicationInfo(std::move(applicationInfo))
        , m_connection(nullptr)
    {
        initResource();

        qRegisterMetaType<BankingItems>();
    }

    ~Private()
    {
        waitForRuns();

        if (m_settings) {
            m_settings->sync();
        }

        close();
    }

    /**
     * Waits out the runs that read and write in threads of their own.
     *
     * Nothing else ends them. Each holds a connection of its own and hands it
     * back to the global registry of QSqlDatabase when it is done, and a run
     * that outlives this object does that after the state it reaches into has
     * been torn down. The wait blocks the thread of the owner, which is the
     * price of that: this runs on the way out, where there is nothing left to
     * keep responsive.
     *
     * The wait hands on what a run threw, and this is called from a destructor,
     * which is implicitly noexcept. An exception passing through would end the
     * process instead of reporting anything.
     *
     * One wait per call, because a read that throws must not take the wait for
     * the write with it: that one would then outlive the object this is here to
     * protect.
     */
    void waitForRuns()
    {
        const auto waitFor = [](QFutureWatcherBase &watcher, const char *kind) {
            const QString failure = exceptionOf([&watcher] { watcher.waitForFinished(); });
            if (!failure.isEmpty()) {
                qCCritical(lcStorage) << "a" << kind << "of the storage ended in" << failure;
            }
        };

        waitFor(m_readWatcher, "read");
        waitFor(m_writeWatcher, "write");
    }

    void setStorageFile(const QString &file) { m_storageFileName = file; }

    QString storageFileName() const { return m_storageFileName; }

    /**
     * The key every path of the storage works with.
     *
     * The length is checked here and not at the public setKey, so that it hangs
     * on every way in rather than on one of two facades. changeKey is the second
     * way, and a new key below the lower bound would rekey the file to something
     * no later open accepts, which puts the holding out of reach.
     *
     * The length, not the full guideline. The character classes of
     * minPasswordGuidelines end in [A-Za-z\d<special>], which no CJK character
     * and no emoji is a member of, however many of the lookaheads a pass phrase
     * built from them satisfies. Enforcing the whole pattern here would lock out
     * exactly the keys the storage was taught to carry unmangled, and would shut
     * the door on every file created under an older, weaker rule. The classes
     * are checked where a key is chosen, in the dialog; what cannot open a file
     * at all is checked here.
     *
     * The length is counted in UTF-16 units, as QString counts it. A character
     * outside the basic multilingual plane, an emoji among them, therefore
     * counts as two. That is the same measure the guideline pattern applies.
     */
    Error setKey(const QString &key)
    {
        if (key.length() < MinPasswordLength || key.length() > MaxPasswordLength) {
            // The key itself never reaches the message.
            return Error(ErrorCode::InvalidInput,
                         QStringLiteral("The key has to be between %1 and %2 characters long")
                             .arg(MinPasswordLength)
                             .arg(MaxPasswordLength));
        }

        m_key = key;

        return {};
    }

    /**
     * Puts back a key this object held a moment ago. Only for the way out of a
     * failed rekey: the value passed the check on its way in, so there is
     * nothing left to decide and nothing left to report.
     */
    void restoreKey(const QString &key) { m_key = key; }

    StorageConnection *connection() { return m_connection; }

    QString lastErrorMessage() { return m_connection->lastErrorMessage(); }

    /**
     * A failed attempt takes its connection with it. Left standing, it is
     * registered under the name of the file and open, and the next call would
     * take it for a storage that has already been checked.
     */
    Error initialize(const bool withSchema = false)
    {
        const auto error = openAndVerify(withSchema);
        if (error.isError()) {
            dropConnection();
        }

        return error;
    }

    Error openAndVerify(const bool withSchema)
    {
        if (m_connection != nullptr) {
            const auto currentDatabaseName = m_connection->database().databaseName();

            // The file alone does not make a connection worth keeping. An
            // attempt that failed to open leaves exactly such a connection
            // behind, and answering success on it hands the caller a store that
            // no statement can reach. It is torn down and built anew instead.
            const bool reusable = currentDatabaseName.toLower() == m_storageFileName.toLower()
                                  && m_connection->isOpen();

            if (!reusable) {
                dropConnection();
            }
        }

        // Only the opening is skipped for a connection that stands, never the
        // checks below it. Answering on the strength of an open file alone would
        // pass a storage whose schema this build cannot read, and the columns
        // the statements bind against would go unasked for.
        if (m_connection == nullptr) {
            if (const auto error = openConnection(); error.isError()) {
                return error;
            }
        }

        // Read before anything else touches the file. A schema this build does
        // not know may hold columns it would silently ignore on read and drop on
        // write.
        const auto versionBefore = schemaVersion();
        if (!versionBefore.hasValue()) {
            return schemaFailure(versionBefore.error().code(), versionBefore.error().message());
        }

        if (versionBefore.value() > CurrentSchemaVersion) {
            return schemaFailure(ErrorCode::SchemaMismatch,
                                 QStringLiteral("The storage %1 was written by a newer "
                                                "version of this program")
                                     .arg(m_storageFileName));
        }

        if (withSchema) {
            // Only where the file is behind. The schema statements are a
            // migration and not an opening routine: they walk the whole holding
            // once, the run that drops doubled bookings twice over, and they do
            // it in the thread that opens the vault. A file that already carries
            // the current version has been through them, and its columns are
            // asked for below whether they ran or not.
            if (versionBefore.value() < CurrentSchemaVersion) {
                if (const auto error = setupTables(); error.isError()) {
                    return error;
                }
            }

            return verifySchema(versionBefore.value());
        }

        if (versionBefore.value() < CurrentSchemaVersion) {
            return schemaFailure(ErrorCode::SchemaMismatch,
                                 QStringLiteral("The storage %1 is at schema version %2 and "
                                                "has to be migrated to %3")
                                     .arg(m_storageFileName)
                                     .arg(versionBefore.value())
                                     .arg(CurrentSchemaVersion));
        }

        return verifyColumns();
    }

    Error openConnection()
    {
        initResource();

        m_connection = new StorageConnection(m_storageFileName);

        if (!m_connection->isDriverAvailable()) {
            // Without the plugin no file opens at all. Told apart from a wrong
            // pass phrase, because the two ask for entirely different remedies.
            return schemaFailure(ErrorCode::DriverMissing,
                                 QStringLiteral("The database driver the storage needs is "
                                                "not installed"));
        }

        if (!m_connection->isOpen()) {
            // The message of the driver names the file and the reason, it is for
            // the log. The code tells the caller that the store could not be
            // opened, which is not the same as a wrong password.
            return schemaFailure(ErrorCode::DatabaseFailure,
                                 QStringLiteral("Could not open the storage file %1: %2")
                                     .arg(m_storageFileName, lastErrorMessage()));
        }

        qCInfo(lcStorage) << "storage opened" << m_storageFileName;

        return {};
    }

    /**
     * Takes the connection down and raises the read generation with it.
     *
     * The number is what tells a result of the file that was open from one of
     * the file that is open now. It is raised wherever a connection falls, not
     * in close() alone: a read that is still going would otherwise pass its
     * records off as the holding of a storage that was opened in the meantime.
     */
    void dropConnection()
    {
        if (m_connection == nullptr) {
            return;
        }

        ++m_connectionGeneration;

        if (m_connection->isOpen()) {
            m_connection->close();
        }

        delete m_connection;
        m_connection = nullptr;
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
            return schemaFailure(versionAfter.error().code(), versionAfter.error().message());
        }

        if (versionAfter.value() != CurrentSchemaVersion) {
            return schemaFailure(ErrorCode::SchemaMismatch,
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

        return verifyColumns();
    }

    /**
     * Whether the tables hold the columns the statements bind against.
     *
     * Asked apart from the version, because the version alone does not answer
     * it. A file that reached the current version before a column was added to
     * that version carries the number without the column, and every statement
     * that names it fails at the first write.
     */
    Error verifyColumns()
    {
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
                return schemaFailure(ErrorCode::SchemaMismatch,
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
        // Closed without maintenance. VACUUM writes the whole encrypted file
        // anew and would hold the window for as long as that takes on a vault
        // of years, on a path the user reaches by closing a storage or by
        // leaving the program.
        //
        // What it would win is the space of deleted rows, and the holding only
        // grows. The one statement that deletes is the deduplication of the
        // schema, which runs once per version step.
        if (m_connection != nullptr && m_connection->isOpen()) {
            qCInfo(lcStorage) << "storage closed" << m_storageFileName;
        }

        dropConnection();

        cleanupResource();
    }

    /**
     * Whether the given key opens the file this storage stands on.
     *
     * Asked on a connection of its own. SQLCipher takes the key of a connection
     * once, when the first statement runs on it, and a PRAGMA key on a
     * connection that is already decrypted changes nothing: a check on the open
     * connection therefore answers for the key that opened it, whatever key it
     * was handed. A second connection is the only place the question can be put.
     */
    [[nodiscard]] bool keyOpensTheFile(const QString &key)
    {
        if (m_connection == nullptr || !m_connection->isOpen()) {
            return false;
        }

        const auto sourceConnectionName = m_connection->database().connectionName();
        const auto probeConnectionName = sourceConnectionName + QStringLiteral("_keyprobe");

        // Called rather than written out, so that every way out of it releases
        // the query and the handle before removeDatabase runs below. Qt warns
        // and leaks the connection while either still refers to it.
        const bool opens = [&] {
            QSqlDatabase database = QSqlDatabase::cloneDatabase(sourceConnectionName,
                                                                probeConnectionName);
            if (!database.isValid() || !database.open()) {
                return false;
            }

            QSqlQuery query;
            if (openQueryOn(database, key, m_storageFileName, query).isError()) {
                database.close();
                return false;
            }

            // A statement that has to read a page of the file. Applying the key
            // says nothing on its own: SQLCipher takes a wrong one without
            // complaint and fails at the first read.
            const bool readable = query.exec(QStringLiteral("SELECT COUNT(*) FROM sqlite_master;"));

            database.close();

            return readable;
        }();

        QSqlDatabase::removeDatabase(probeConnectionName);

        return opens;
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

        // A read and a write of this storage run at the same time and on
        // connections of their own, over one file. Without a wait SQLite refuses
        // the moment the other holds the lock, and the whole run falls: a fetch
        // that cost minutes on the line is rolled back because the user clicked
        // an account while it was being written.
        //
        // Interpolating into a statement is normally forbidden. It is
        // unavoidable here for the reason given at keyLiteral above: SQLite
        // accepts no bound parameter in a PRAGMA. The value is a constant of
        // this file and comes from no input.
        if (!query.exec(QStringLiteral("PRAGMA busy_timeout=%1;").arg(LockWaitMs))) {
            qCWarning(lcStorage) << "could not set the lock wait on" << fileName << ":"
                                 << query.lastError().text();
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

        // An order the last successful fetch no longer reported is kept and
        // hidden, and the caller does not get to ask for it: the mark says what
        // the application observed, not what it was asked to show.
        if (query.type == Storage::StorageStandingOrder) {
            parts << QStringLiteral("ended_at IS NULL");
        }

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
     * Runs one COUNT statement and hands back its number. It carries the key,
     * the preparation and the bindings, so that a caller has only its statement
     * to give.
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
     * How many rows of the table satisfy the condition, without the window. This
     * is the number the filter bar shows, and the window a read hands back
     * cannot stand in for it: fifty rows say nothing about three thousand.
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

            // The number the filter bar shows. It stands under the same condition
            // as the read and counts the whole holding, which the window the
            // query just opened cannot say. A failure costs the number, not the
            // records, so the run carries on and reports nothing rather than
            // something wrong.
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
                for (const auto &[key_, value] : std::as_const(columnList).asKeyValueRange()) {
                    map[value] = query.value(key_);
                }

                rows << map;

                // No clear on purpose. Every row sets the same keys, so the
                // inserts of the next round turn into assignments.
            }

            // What the progress below is measured against. numRowsAffected() is
            // undefined for a SELECT and SQLite answers -1, which turned the
            // progress negative; the rows that were just collected are the window
            // this run reports over, and asking the file for their number would
            // scan the same page a second time.
            const int totalRows = rows.size();

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
                case Storage::StorageStandingOrder:
                    bankingItems << StandingOrder::fromMap(row);
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
     * Writes one row and answers with the number of rows that changed by it.
     *
     * Nought is not a failure: a statement with an ON CONFLICT clause reaches
     * this on a record that is already there, and passing over it is what the
     * clause is for. The number is what a run reports as its result, and only a
     * count of the rows that actually changed answers the question the user
     * asks, which is how much is new.
     *
     * A key of the property map that the statement does not name is reported and
     * skipped. bindValue would drop it without a word, which is how the balance
     * of an account went missing.
     */
    static Result<int> insertRowOn(const QSqlDatabase &database,
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
            // The result carries the failure of an exec, not the driver. Asking
            // the driver answers with the last error it saw for itself, which
            // for a statement SQLite refused is nothing at all.
            return Error(ErrorCode::DatabaseFailure,
                         QStringLiteral("Could not store an item of type %1: %2")
                             .arg(type, query.lastError().text()));
        }

        // numRowsAffected is defined for an INSERT and answers what SQLite
        // counted. A record the ON CONFLICT clause passed over answers nought.
        return qMax(query.numRowsAffected(), 0);
    }

    /**
     * Writes one record, whatever its type, and answers with the number of rows
     * it added. The whole write path is static and takes the database, because
     * storeItems runs it in a thread of its own on a connection of its own; a
     * QSqlDatabase belongs to the thread that created it. Storage::storeItem
     * hands in the connection of this object and answers its caller through the
     * return value; neither it nor this function emits anything.
     *
     * The type is told from the record itself rather than from the enumeration of
     * storage types: that one steers the read path, where a table name and a row
     * have to be brought together, and the write path has never needed it.
     */
    static Result<int> storeItemOn(const QSqlDatabase &database,
                                   const QString &key,
                                   const QString &fileName,
                                   const BankingItem *bankingItem,
                                   QSet<qint64> *touchedStandingOrders = nullptr)
    {
        if (bankingItem == nullptr) {
            return Error(ErrorCode::InvalidInput, QStringLiteral("No banking item to store"));
        }

        const auto type = bankingItem->itemType();

        if (!bankingItem->isValid()) {
            return Error(ErrorCode::InvalidInput,
                         QStringLiteral("Invalid banking item of type %1").arg(type));
        }

        if (type == QLatin1StringView("Account")) {
            return storeAccountOn(database, key, fileName, bankingItem->toMap());
        }

        if (type == QLatin1StringView("Transaction")) {
            return insertRowOn(database,
                               key,
                               fileName,
                               transactionInsertQuery(),
                               bankingItem->toMap(),
                               type);
        }

        if (type == QLatin1StringView("Balance")) {
            return storeFetchedBalanceOn(database, key, fileName, bankingItem->toMap());
        }

        if (type == QLatin1StringView("StandingOrder")) {
            return storeStandingOrderOn(database,
                                        key,
                                        fileName,
                                        bankingItem->toMap(),
                                        touchedStandingOrders);
        }

        // Category and Contact have no table of their own yet. Answered here,
        // because an empty branch would send an unprepared query on its way.
        return Error(ErrorCode::NotImplemented,
                     QStringLiteral("Storing an item of type %1 is not implemented").arg(type));
    }

    /**
     * Writes the balance a fetch brought, without touching the account it hangs
     * on. It carries a day, a type and a currency of its own and takes the place
     * of whatever the account row held.
     *
     * The record names the account by the identifier the institution assigns; the
     * table keys on the row id of the stored account, so the one is translated
     * into the other here, the same way the account path does it.
     */
    static Result<int> storeFetchedBalanceOn(const QSqlDatabase &database,
                                             const QString &key,
                                             const QString &fileName,
                                             const QMap<QString, QVariant> &map)
    {
        auto balanceMap = map;

        const auto uniqueAccountId = balanceMap.take(QStringLiteral("unique_account_id"));

        const auto accountId = accountIdOfOn(database, key, fileName, uniqueAccountId);
        if (!accountId.hasValue()) {
            return Error(ErrorCode::NotFound,
                         QStringLiteral("No stored account for the balance of account %1")
                             .arg(uniqueAccountId.toString()));
        }

        balanceMap[QStringLiteral("account_id")] = accountId.value();

        return insertRowOn(database,
                           key,
                           fileName,
                           balanceInsertQuery(),
                           balanceMap,
                           QStringLiteral("Balance"));
    }

    /**
     * The row a delivered standing order belongs to, or nought where none does.
     *
     * The identifier of the institution comes first, and its uniqueness per
     * account is not assumed: where several rows carry it, the fingerprint
     * decides between them, and an order that matches none of them falls through
     * to the search over the fingerprint alone. That last step is also what finds
     * an order again which was reported without an identifier the first time and
     * with one the next, so that it does not become a second order.
     */
    static Result<qint64> existingStandingOrderOn(const QSqlDatabase &database,
                                                  const QString &key,
                                                  const QString &fileName,
                                                  const QVariant &uniqueAccountId,
                                                  const QString &fiId,
                                                  const QString &fingerprint)
    {
        QSqlQuery query;
        if (const auto error = openQueryOn(database, key, fileName, query); error.isError()) {
            return error;
        }

        if (!fiId.isEmpty()) {
            if (!query.prepare(QStringLiteral("SELECT id, fingerprint FROM standing_orders WHERE "
                                              "unique_account_id = :accountId AND fi_id = :fiId "
                                              "ORDER BY id ASC;"))) {
                return Error(ErrorCode::DatabaseFailure,
                             QStringLiteral("Could not prepare the lookup of a standing order: %1")
                                 .arg(query.lastError().text()));
            }

            query.bindValue(QStringLiteral(":accountId"), uniqueAccountId);
            query.bindValue(QStringLiteral(":fiId"), fiId);

            if (!query.exec()) {
                return Error(ErrorCode::DatabaseFailure,
                             QStringLiteral("Could not look a standing order up by its "
                                            "identifier: %1")
                                 .arg(query.lastError().text()));
            }

            auto rows = QList<QPair<qint64, QString>>();
            while (query.next()) {
                rows.append({query.value(0).toLongLong(), query.value(1).toString()});
            }

            if (rows.size() == 1) {
                return rows.constFirst().first;
            }

            for (const auto &[rowId, storedFingerprint] : std::as_const(rows)) {
                if (storedFingerprint == fingerprint) {
                    return rowId;
                }
            }
        }

        if (!query.prepare(QStringLiteral("SELECT id FROM standing_orders WHERE "
                                          "unique_account_id = :accountId AND "
                                          "fingerprint = :fingerprint;"))) {
            return Error(ErrorCode::DatabaseFailure,
                         QStringLiteral("Could not prepare the lookup of a standing order: %1")
                             .arg(query.lastError().text()));
        }

        query.bindValue(QStringLiteral(":accountId"), uniqueAccountId);
        query.bindValue(QStringLiteral(":fingerprint"), fingerprint);

        if (!query.exec()) {
            return Error(ErrorCode::DatabaseFailure,
                         QStringLiteral("Could not look a standing order up by its fingerprint: %1")
                             .arg(query.lastError().text()));
        }

        return query.next() ? query.value(0).toLongLong() : qint64{0};
    }

    /**
     * Writes one standing order and answers with the number of rows it added.
     *
     * An order that is already there is updated and adds none. That is what the
     * count says and what a second fetch of the same account reports: nothing
     * came in that was not there before.
     *
     * The set of rows this run touched is kept by the caller. It is what the
     * marking below tells a delivered order from one the fetch passed over, and
     * a row that is touched twice within one run is the limit of the fingerprint
     * showing itself.
     */
    static Result<int> storeStandingOrderOn(const QSqlDatabase &database,
                                            const QString &key,
                                            const QString &fileName,
                                            const QMap<QString, QVariant> &map,
                                            QSet<qint64> *touchedRows)
    {
        auto orderMap = map;

        const auto uniqueAccountId = orderMap.value(QStringLiteral("unique_account_id"));

        const auto accountId = accountIdOfOn(database, key, fileName, uniqueAccountId);
        if (!accountId.hasValue()) {
            return Error(ErrorCode::NotFound,
                         QStringLiteral("No stored account for the standing order of account %1")
                             .arg(uniqueAccountId.toString()));
        }

        orderMap[QStringLiteral("account_id")] = accountId.value();

        const auto fingerprint = orderMap.value(QStringLiteral("fingerprint")).toString();

        const auto existing
            = existingStandingOrderOn(database,
                                      key,
                                      fileName,
                                      uniqueAccountId,
                                      orderMap.value(QStringLiteral("fi_id")).toString(),
                                      fingerprint);
        if (!existing.hasValue()) {
            return existing.error();
        }

        if (existing.value() == 0) {
            const auto written = insertRowOn(database,
                                             key,
                                             fileName,
                                             standingOrderInsertQuery(),
                                             orderMap,
                                             QStringLiteral("StandingOrder"));
            if (!written.hasValue()) {
                return written.error();
            }

            if (touchedRows != nullptr) {
                const auto inserted
                    = existingStandingOrderOn(database,
                                              key,
                                              fileName,
                                              uniqueAccountId,
                                              orderMap.value(QStringLiteral("fi_id")).toString(),
                                              fingerprint);
                if (!inserted.hasValue()) {
                    return inserted.error();
                }

                touchedRows->insert(inserted.value());
            }

            return written.value();
        }

        if (touchedRows != nullptr && touchedRows->contains(existing.value())) {
            // Two orders of one account that agree in every field the fingerprint
            // is formed over. Whether the institution holds two or reported one
            // twice is not something this side can tell, so it goes into the log
            // and no further. The count the run reports is the number of rows
            // that were actually written.
            qCWarning(lcStorage) << "two standing orders of account" << uniqueAccountId.toString()
                                 << "cannot be told apart and were merged into one row";
        }

        orderMap[QStringLiteral("id")] = existing.value();

        const auto updated = insertRowOn(database,
                                         key,
                                         fileName,
                                         standingOrderUpdateQuery(),
                                         orderMap,
                                         QStringLiteral("StandingOrder"));
        if (!updated.hasValue()) {
            return updated.error();
        }

        if (touchedRows != nullptr) {
            touchedRows->insert(existing.value());
        }

        // An order that was already there adds no row.
        return 0;
    }

    /**
     * Marks every standing order of one account that this run did not carry.
     *
     * Nothing is removed. The mark says that a fetch which went through no longer
     * reported the order, and an order that comes back loses it again, which a
     * deleted row could not.
     */
    static Error markEndedStandingOrdersOn(const QSqlDatabase &database,
                                           const QString &key,
                                           const QString &fileName,
                                           quint32 uniqueAccountId,
                                           const QSet<qint64> &touchedRows)
    {
        QSqlQuery query;
        if (const auto error = openQueryOn(database, key, fileName, query); error.isError()) {
            return error;
        }

        auto statement = QStringLiteral("UPDATE standing_orders SET ended_at = :endedAt WHERE "
                                        "unique_account_id = :accountId AND ended_at IS NULL");

        if (!touchedRows.isEmpty()) {
            // SQL knows no binding for a list, so the row ids are written into
            // the statement. They are numbers this run read out of the file
            // itself and turned back into numbers here; nothing a caller wrote
            // reaches the text.
            auto ids = QStringList();
            ids.reserve(touchedRows.size());

            for (const auto rowId : touchedRows) {
                ids << QString::number(rowId);
            }

            statement += QStringLiteral(" AND id NOT IN (%1)").arg(ids.join(QLatin1Char(',')));
        }

        statement += QLatin1Char(';');

        if (!query.prepare(statement)) {
            return Error(ErrorCode::DatabaseFailure,
                         QStringLiteral("Could not prepare the marking of ended standing "
                                        "orders: %1")
                             .arg(query.lastError().text()));
        }

        query.bindValue(QStringLiteral(":endedAt"), QDateTime::currentDateTime());
        query.bindValue(QStringLiteral(":accountId"), uniqueAccountId);

        if (!query.exec()) {
            return Error(ErrorCode::DatabaseFailure,
                         QStringLiteral("Could not mark the ended standing orders of account "
                                        "%1: %2")
                             .arg(QString::number(uniqueAccountId), query.lastError().text()));
        }

        return {};
    }

    /**
     * Writes a run of records, in a thread of its own.
     *
     * The connection is cloned rather than shared, for the same reason readItems
     * clones it. Everything the run needs is passed by value, the records
     * included: they are shared pointers, so the run holds them alive on its own.
     *
     * The bracket sits around the run, not around the single record: a run that
     * fails at one record leaves no row of that run behind, because half a
     * holding would move the starting point of the next fetch past bookings
     * nobody holds. The run ends at the first failure rather than carrying on
     * over a record that may be the cause. A run that carries an account is the
     * exception, and the reason stands where that is decided.
     */
    static void writeItems(QPromise<WriteResult> &promise,
                           const QString &sourceConnectionName,
                           const QString &key,
                           const QString &fileName,
                           BankingItems items,
                           StandingOrderRun standingOrderRun)
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

            // A run that carries an account is not bracketed here: the account
            // path begins a transaction of its own, and SQLite does not nest
            // them. It brackets each account for itself, which is the guarantee
            // that path needs. A fetch hands over bookings and a balance and
            // never an account, so the run that needs the bracket gets it.
            const bool carriesAccount
                = std::any_of(items.cbegin(), items.cend(), [](const BankingItemPtr &item) {
                      return item != nullptr && item->itemType() == QLatin1StringView("Account");
                  });

            if (!carriesAccount && !database.transaction()) {
                promise.addResult(
                    WriteResult{0,
                                Error(ErrorCode::DatabaseFailure,
                                      QStringLiteral("Could not begin a transaction on %1: %2")
                                          .arg(fileName, database.lastError().text()))});
                database.close();
                return;
            }

            auto error = Error();
            int handled = 0;

            // The rows the run wrote or updated. What is not in it when the run
            // ends is what the fetch no longer reported.
            auto touchedStandingOrders = QSet<qint64>();

            for (const auto &item : std::as_const(items)) {
                const auto written = storeItemOn(database,
                                                 key,
                                                 fileName,
                                                 item.get(),
                                                 &touchedStandingOrders);
                if (!written.hasValue()) {
                    error = written.error();
                    break;
                }

                stored += written.value();

                ++handled;
                promise.setProgressValue(qMin(handled * 100 / items.size(), 100));
            }

            // Inside the bracket, so that a run which failed halfway leaves
            // neither the orders it wrote nor a mark on the ones it did not.
            // Only a fetch that went through says anything about what the
            // institution still holds; one that was aborted, that failed, that
            // the bank refused, or an account that was passed over, says nothing
            // and marks nothing.
            if (!error.isError() && standingOrderRun.accountId != 0 && standingOrderRun.succeeded) {
                error = markEndedStandingOrdersOn(database,
                                                  key,
                                                  fileName,
                                                  standingOrderRun.accountId,
                                                  touchedStandingOrders);
            }

            if (!carriesAccount) {
                if (error.isError()) {
                    // Nothing of this run stays. Half a holding would move the
                    // starting point of the next fetch past bookings that nobody
                    // holds, and the count that is reported has to say so.
                    error = rollbackOn(database, error);
                    stored = 0;
                } else if (!database.commit()) {
                    // Rolled back like the failure above, so that the state this
                    // run leaves behind is the one it reports. A commit that did
                    // not go through can leave the transaction open, and closing
                    // the connection alone would decide the outcome without
                    // saying so.
                    error = rollbackOn(database,
                                       Error(ErrorCode::DatabaseFailure,
                                             QStringLiteral(
                                                 "Could not commit a run of records to %1: %2")
                                                 .arg(fileName, database.lastError().text())));
                    stored = 0;
                }
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
    static Result<int> storeAccountOn(QSqlDatabase database,
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

        const auto written = insertRowOn(database,
                                         key,
                                         fileName,
                                         accountInsertQuery(),
                                         accountMap,
                                         QStringLiteral("Account"));
        if (!written.hasValue()) {
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
            // Rolled back like every other way out of this function that failed.
            // A commit that did not go through can leave the transaction open,
            // and the next write on this connection would then run inside it.
            return rollbackOn(database,
                              Error(ErrorCode::DatabaseFailure,
                                    QStringLiteral("Could not commit an account to %1: %2")
                                        .arg(fileName, database.lastError().text())));
        }

        // The row of the account itself. The balance and the reference accounts
        // belong to it and are not counted beside it.
        return written.value();
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
                             .arg(fileName, query.lastError().text()));
        }

        return {};
    }

    /**
     * The balance goes to a row of its own, keyed by the account. The day is the
     * day of the write in UTC; AB_ACCOUNT_SPEC carries no date with the figure,
     * and inventing a business day would be worse than recording when it was
     * read.
     *
     * The type says unknown for the same reason: the account list does not say
     * whether the figure is booked or noted. Not none, which a bank also sends
     * for a figure it gives no type for: the two have to be told apart, and
     * telling them apart is what the statement below rests on. It
     * refreshes the row it wrote itself and leaves a fetched figure alone, which
     * was chosen by its type and would otherwise be pushed aside by this
     * placeholder on the next run over the account list.
     *
     * A row that stays untouched for that reason is not a failure, so nought
     * rows changed is a result like any other here.
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
            {QStringLiteral("type"), static_cast<int>(AB_Balance_TypeUnknown)},
            {QStringLiteral("currency"), accountMap.value(QStringLiteral("currency"))},
        };

        const auto result = insertRowOn(database,
                                        key,
                                        fileName,
                                        placeholderBalanceInsertQuery(),
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

        if (!query.prepare(
                QStringLiteral("SELECT `value` FROM balances WHERE account_id = :id;"))) {
            qCWarning(lcStorage) << "could not read the balance of an account:"
                                 << query.lastError().text();
        } else {
            query.bindValue(QStringLiteral(":id"), accountId);

            if (!query.exec()) {
                qCWarning(lcStorage)
                    << "could not read the balance of an account:" << query.lastError().text();
            } else if (query.next()) {
                // An account with no balance row is not a failure. It carries no
                // figure yet, and the row goes on without the property.
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
            return schemaFailure(ErrorCode::IoFailure,
                                 QStringLiteral("Could not read the schema: %1")
                                     .arg(storageFile.errorString()));
        }

        const QStringList sqlStatements = QTextStream(&storageFile).readAll().split(';');
        QStringList queries = {};

        // The replacement works on a copy, so that building the second list
        // leaves the first one as it was.
        for (const auto &statement : sqlStatements) {
            queries << QString(statement).replace(QStringLiteral("#"), QStringLiteral(";")).trimmed();
        }

        QSqlQuery query;
        if (const auto error = openQuery(query); error.isError()) {
            return schemaFailure(error.code(), error.message());
        }

        for (const auto &sqlStatement : std::as_const(queries)) {
            if (sqlStatement.isEmpty()) {
                continue;
            }

            if (!connection()->beginTransaction()) {
                return schemaFailure(ErrorCode::DatabaseFailure,
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

                // The result carries the failure of an exec, not the driver.
                // Asking the driver answers with the last error it saw for
                // itself, which for a statement SQLite refused is nothing at
                // all, and the message would name the file without a cause.
                return schemaFailure(ErrorCode::DatabaseFailure,
                                     QStringLiteral("Could not create the schema of %1: %2")
                                         .arg(m_storageFileName, query.lastError().text()));
            }

            if (!connection()->commitTransaction()) {
                return schemaFailure(ErrorCode::DatabaseFailure,
                                     QStringLiteral("Could not commit the schema of %1: %2")
                                         .arg(m_storageFileName, lastErrorMessage()));
            }
        }

        return {};
    }

private:
    /**
     * Notes a failure of the setup in the log and hands it back.
     *
     * It reaches the caller through the return value alone. initialize is a
     * synchronous call: whoever asked for it is standing right there holding an
     * Error, and a signal beside it would put a second message on the same
     * status bar, from a path the caller has already handled.
     */
    Error schemaFailure(ErrorCode code, const QString &message)
    {
        auto error = Error(code, message);

        qCCritical(lcStorage) << error.message();

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
     * A member rather than a local, because a local would go at the end of the
     * call that started the run. Its destructor does not wait for the future, it
     * only cuts the delivery, so the run would carry on with nobody left to
     * report to. What waits is waitForRuns, on the way out of this object.
     */
    QFutureWatcher<ReadResult> m_readWatcher;

    /**
     * Tells a run that is going from one that started on a connection which has
     * since fallen. Every way a connection goes raises it, closing and a failed
     * open alike, and a result that comes back under an older number belongs to
     * a file nobody has open any more.
     */
    quint64 m_connectionGeneration = 0;

    /**
     * The same for the run started by storeItems. A watcher of its own rather
     * than a shared one, because the two carry different results and a run of
     * either kind must not cancel the other.
     */
    QFutureWatcher<WriteResult> m_writeWatcher;

    /**
     * Whether a run of that kind is still owed its answer.
     *
     * The watcher alone cannot say it. It reports the state of the future, which
     * turns to finished the moment the worker returns, while the completion is
     * still on its way to this thread as a queued signal. A second call arriving
     * in that gap would pass the watcher's own guard and cut the connection the
     * first run's answer is to travel over, leaving whoever waits for it waiting
     * for good. These are raised where a run is started and lowered where its
     * answer is handed on, so they stay up across the whole of that gap.
     */
    bool m_readInFlight = false;
    bool m_writeInFlight = false;

    // Nothing here emits. Every signal of the storage belongs to a run that
    // Storage itself starts and watches, so this class needs no way back to it.
    friend class Storage;
};

Storage::Storage(ApplicationInfo applicationInfo, QObject *parent)
    : QObject(parent)
    , d_ptr(new Private(std::move(applicationInfo)))
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
    return d_ptr->setKey(key);
}

Error Storage::changeKey(const QString &oldKey, const QString &newKey)
{
    const QString previousKey = d_ptr->m_key;

    if (const auto error = d_ptr->setKey(oldKey); error.isError()) {
        return error;
    }

    // Asked of the file and not of the connection that stands. The open one
    // answers for the key it was opened with, so a wrong current key would pass
    // here and the rekey would go through on it: whoever has an open vault in
    // front of them could set a new password without knowing the old one, and
    // the dialog that says the current password is not correct would never say
    // it.
    if (!d_ptr->keyOpensTheFile(oldKey)) {
        // The key that was handed in stays out of the object. It does not open
        // the file, so every later statement would set it and fail, and a close
        // followed by an open would leave the holding out of reach.
        d_ptr->restoreKey(previousKey);

        return Error(ErrorCode::PermissionDenied,
                     QStringLiteral("The current key does not open %1")
                         .arg(d_ptr->storageFileName()));
    }

    // The query is held for the length of the rekey and no longer. Below this
    // block the connection may come down, and a query still standing on it would
    // hold the connection open past removeDatabase and leak it for the run.
    {
        QSqlQuery query;
        if (const auto error = d_ptr->openQuery(query); error.isError()) {
            d_ptr->restoreKey(previousKey);

            return error;
        }

        // Refused before the file is touched. A rekey to a key the public
        // interface does not accept would leave the holding out of reach of
        // every later open.
        if (const auto error = d_ptr->setKey(newKey); error.isError()) {
            d_ptr->restoreKey(oldKey);

            return error;
        }

        if (!query.exec(QStringLiteral("PRAGMA rekey=") + keyLiteral(newKey) + QLatin1Char(';'))) {
            // Restores the state the caller handed us, so that a failed change
            // does not leave the storage holding a key it was never rekeyed to.
            d_ptr->restoreKey(oldKey);

            return Error(ErrorCode::DatabaseFailure,
                         QStringLiteral("Could not change the key of %1")
                             .arg(d_ptr->storageFileName()));
        }
    }

    // Asked of the file for the same reason the old key was: the connection that
    // stands kept the cipher context it was opened with and answers for it,
    // whatever key the file on disk now carries. A rekey writes every page anew
    // and can stop halfway.
    if (!d_ptr->keyOpensTheFile(newKey)) {
        // Neither key is known to open the file, so no later statement on this
        // connection would say anything about the one on disk. It comes down,
        // which shows the damage here rather than at the next start.
        d_ptr->dropConnection();

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

bool Storage::isOpen() const
{
    return d_ptr->connection() != nullptr && d_ptr->connection()->isOpen();
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
        // Asked for before it is reached for. A call before initialize would
        // otherwise take the whole application down with it.
        return Error(ErrorCode::DatabaseFailure,
                     QStringLiteral("No storage connection for %1").arg(d_ptr->storageFileName()));
    }

    const auto written = Private::storeItemOn(d_ptr->connection()->database(),
                                              d_ptr->m_key,
                                              d_ptr->storageFileName(),
                                              bankingItem);

    // Reported through the return value alone. This call is synchronous and its
    // caller holds the outcome the moment it comes back; a signal beside it
    // would reach the receivers of a run of storeItems, which this is not, and
    // would put a second message on a status bar the caller has already written.
    if (!written.hasValue()) {
        const auto error = written.error();

        qCCritical(lcStorage) << error.message();

        return error;
    }

    qCDebug(lcStorage) << "stored an item of type" << bankingItem->itemType();

    return {};
}

Error Storage::storeItems(const BankingItems &items, const StandingOrderRun &standingOrderRun)
{
    // Answered to the caller and to nobody else. A signal here would reach
    // whoever is waiting for the run that is already going, and that receiver
    // would take the refusal of this call for the end of its own run.
    const auto refuse = [](ErrorCode code, const QString &message) {
        qCCritical(lcStorage) << message;

        return Error(code, message);
    };

    if (d_ptr->connection() == nullptr || !d_ptr->connection()->isOpen()) {
        return refuse(ErrorCode::DatabaseFailure,
                      QStringLiteral("No open storage connection for %1")
                          .arg(d_ptr->storageFileName()));
    }

    // A second run while one is still going would be a second transaction on the
    // same file, and the watcher of the first would be lost.
    if (d_ptr->m_writeInFlight) {
        return refuse(ErrorCode::Busy, QStringLiteral("A write to the storage is already running"));
    }

    // A run without records is not a failure. Nothing is started, and the caller
    // still gets its end.
    //
    // Reported through the event loop rather than from here, the way a run that
    // does start reports. A caller makes its connections after the call, on the
    // strength of the return value, and would not be among the receivers of a
    // signal sent before this function came back.
    //
    // A standing order fetch is the exception: a run without records is its
    // answer that the account holds none any more, and that answer marks the
    // whole holding. It has to reach the write path rather than end here.
    if (items.isEmpty() && standingOrderRun.accountId == 0) {
        QMetaObject::invokeMethod(
            this,
            [this] {
                Q_EMIT itemsStored(0);
                Q_EMIT writeFinished();
            },
            Qt::QueuedConnection);

        return {};
    }

    const auto sourceConnectionName = d_ptr->connection()->database().connectionName();

    // The connections are made before the run is started. A short write could
    // otherwise finish before anyone is listening.
    QObject::disconnect(&d_ptr->m_writeWatcher, nullptr, this, nullptr);

    connect(&d_ptr->m_writeWatcher,
            &QFutureWatcher<WriteResult>::progressValueChanged,
            this,
            [this](int progress) { Q_EMIT writeProgressChanged(progress); });

    const quint64 generation = d_ptr->m_connectionGeneration;

    connect(&d_ptr->m_writeWatcher,
            &QFutureWatcher<WriteResult>::finished,
            this,
            [this, generation]() {
                d_ptr->m_writeInFlight = false;

                if (generation != d_ptr->m_connectionGeneration) {
                    // The storage was closed while this run was going. What it wrote is
                    // in the file it wrote to, and that file is not the one that is open
                    // now: a count reported here would be read as the outcome of the
                    // storage that stands, and whoever refreshes on it would ask a
                    // connection that is gone. The end is still reported, so that nobody
                    // waits for a run that is over.
                    qCInfo(lcStorage) << "dropping the result of a write that outlived its storage";

                    Q_EMIT writeFinished();
                    return;
                }

                const auto future = d_ptr->m_writeWatcher.future();
                if (future.resultCount() == 0) {
                    // Cannot happen through writeItems, which reports on every path. A
                    // cancelled future can end here, and a silent return would leave the
                    // caller waiting for a signal that never comes.
                    qCCritical(lcStorage) << "the storage write ended without a result";

                    Q_EMIT writeFailed(ErrorCode::DatabaseFailure,
                                       QStringLiteral("The storage write ended without a result"));
                    Q_EMIT itemsStored(0);
                    Q_EMIT writeFinished();
                    return;
                }

                const auto result = future.result();
                if (result.error.isError()) {
                    qCCritical(lcStorage) << result.error.message();

                    Q_EMIT writeFailed(result.error.code(), result.error.message());
                } else {
                    qCDebug(lcStorage) << "stored" << result.stored << "items";
                }

                // The count goes out on both paths. A failure has to be reported with
                // the number of items that made it, not on its own.
                Q_EMIT itemsStored(result.stored);
                Q_EMIT writeFinished();
            });

    d_ptr->m_writeInFlight = true;

    d_ptr->m_writeWatcher.setFuture(QtConcurrent::run(&Private::writeItems,
                                                      sourceConnectionName,
                                                      d_ptr->m_key,
                                                      d_ptr->storageFileName(),
                                                      items,
                                                      standingOrderRun));

    return {};
}

Error Storage::receiveItems(const ItemQuery &query)
{
    // See storeItems: a call that starts no run answers its caller and emits
    // nothing.
    const auto refuse = [](ErrorCode code, const QString &message) {
        qCCritical(lcStorage) << message;

        return Error(code, message);
    };

    // A negative offset or a limit of INT_MAX is not a query anyone meant to
    // run, and neither belongs in a statement.
    if (query.limit < 1 || query.limit > MaxItemsPerQuery || query.offset < 0) {
        return refuse(ErrorCode::InvalidInput,
                      QStringLiteral("Invalid window: limit=%1 offset=%2")
                          .arg(query.limit)
                          .arg(query.offset));
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
    // The account is the exception among the five: standing_orders carries the
    // column as well, and a read of one account is what that path is for.
    const bool carriesAccount = query.type == StorageTransaction
                                || query.type == StorageStandingOrder;

    if (query.accountId != 0 && !carriesAccount) {
        return refuse(ErrorCode::InvalidInput,
                      QStringLiteral("An account is defined for bookings and standing orders "
                                     "only, not for %1")
                          .arg(QString::fromUtf8(
                              QMetaEnum::fromType<Storage::Type>().valueToKey(query.type))));
    }

    const bool filtered = !query.text.isEmpty() || query.from.isValid() || query.to.isValid()
                          || query.direction != Direction::Any;

    if (filtered && query.type != StorageTransaction) {
        return refuse(ErrorCode::InvalidInput,
                      QStringLiteral("A filter is defined for transactions only, not for %1")
                          .arg(QString::fromUtf8(
                              QMetaEnum::fromType<Storage::Type>().valueToKey(query.type))));
    }

    // The same for the column a read orders by. Every value of SortColumn names
    // a column of the transactions table, so one of them on another type reaches
    // the statement as an ORDER BY over a column that table does not carry. The
    // caller would get a failure of the database in place of the answer the
    // check above gives for the very same mistake.
    if (query.sort != SortColumn::None && query.type != StorageTransaction) {
        return refuse(ErrorCode::InvalidInput,
                      QStringLiteral("A sort column is defined for transactions only, not for %1")
                          .arg(QString::fromUtf8(
                              QMetaEnum::fromType<Storage::Type>().valueToKey(query.type))));
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
    case Storage::StorageStandingOrder:
        table = QStringLiteral("standing_orders");
        break;
    case Storage::StorageCategories:
    case Storage::StorageContacts:
        // These two have no table of their own in the schema. The branches used
        // to be empty, which ended in a message that named the previous statement
        // instead of the cause.
        return refuse(ErrorCode::NotImplemented,
                      QStringLiteral("Reading items of type %1 is not implemented")
                          .arg(QString::fromUtf8(
                              QMetaEnum::fromType<Storage::Type>().valueToKey(query.type))));
    }

    // Before the columns are asked for, because that asks the file. Without a
    // connection the question ends in a message about the columns of a table,
    // which names neither the cause nor a remedy.
    if (d_ptr->connection() == nullptr || !d_ptr->connection()->isOpen()) {
        return refuse(ErrorCode::DatabaseFailure,
                      QStringLiteral("No open storage connection for %1")
                          .arg(d_ptr->storageFileName()));
    }

    // A second run while one is still going would open a second reader and lose
    // the watcher of the first. Nothing in the application does it, and this
    // says so instead of leaving it to chance.
    if (d_ptr->m_readInFlight) {
        return refuse(ErrorCode::Busy, QStringLiteral("A read of the storage is already running"));
    }

    const auto columnList = d_ptr->tableColumns(table);
    if (columnList.isEmpty()) {
        return refuse(ErrorCode::DatabaseFailure,
                      QStringLiteral("No columns found for the table %1").arg(table));
    }

    // Everything above answers its caller at once, the column query included:
    // that one asks the file, but for the shape of a single table. What follows
    // is the part that reads the holding, and it is what must not sit in the
    // calling thread: a window of a thousand accounts costs a second query per
    // account for its balance and its reference accounts.
    const auto sourceConnectionName = d_ptr->connection()->database().connectionName();

    // The connections are made before the run is started. A short read could
    // otherwise finish before anyone is listening.
    QObject::disconnect(&d_ptr->m_readWatcher, nullptr, this, nullptr);

    connect(&d_ptr->m_readWatcher,
            &QFutureWatcher<ReadResult>::progressValueChanged,
            this,
            [this](int progress) { Q_EMIT readProgressChanged(progress); });

    const quint64 generation = d_ptr->m_connectionGeneration;

    connect(&d_ptr->m_readWatcher, &QFutureWatcher<ReadResult>::finished, this, [this, generation]() {
        d_ptr->m_readInFlight = false;

        if (generation != d_ptr->m_connectionGeneration) {
            // The storage was closed while this run was going, and the file it
            // read is not the one that is open now. Handing the records on would
            // show the accounts of the previous storage under the name of the
            // current one. The completion is still reported, so that nobody
            // waits for a run that is over.
            qCInfo(lcStorage) << "dropping the result of a read that outlived its storage";

            Q_EMIT readFinished();
            return;
        }

        const auto future = d_ptr->m_readWatcher.future();
        if (future.resultCount() == 0) {
            // Cannot happen through readItems, which reports on every path. A
            // cancelled future can end here, and a silent return would leave the
            // caller waiting for a signal that never comes.
            qCCritical(lcStorage) << "the storage read ended without a result";

            Q_EMIT readFailed(ErrorCode::DatabaseFailure,
                              QStringLiteral("The storage read ended without a result"));
            Q_EMIT readFinished();
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

            Q_EMIT readFailed(result.error.code(), result.error.message());
            Q_EMIT readFinished();
            return;
        }

        Q_EMIT itemsReceived(result.items);
        Q_EMIT readFinished();
    });

    d_ptr->m_readInFlight = true;

    d_ptr->m_readWatcher.setFuture(QtConcurrent::run(&Private::readItems,
                                                     sourceConnectionName,
                                                     d_ptr->m_key,
                                                     d_ptr->storageFileName(),
                                                     table,
                                                     columnList,
                                                     query));

    return {};
}

Result<QDate> Storage::latestTransactionDate(quint32 uniqueAccountId)
{
    QSqlQuery query;
    if (const auto error = d_ptr->openQuery(query); error.isError()) {
        return error;
    }

    // The valuta date stands in for a missing booking date. Both forms of a
    // date that is not there are caught: a row written by the application
    // carries an empty text, one written past it carries no value at all.
    //
    // Neither index of the two columns bears on the expression, so the read
    // sorts. Over a window of one row that is the cheaper of the two against an
    // index over an expression that no other read would use.
    if (!query.prepare(QStringLiteral(
            "SELECT COALESCE(NULLIF(`date`, ''), valuta_date) AS booking_date FROM transactions "
            "WHERE unique_account_id = :accountId ORDER BY booking_date DESC LIMIT 1;"))) {
        return Error(ErrorCode::DatabaseFailure,
                     QStringLiteral("Could not prepare the read of the latest booking date: %1")
                         .arg(query.lastError().text()));
    }

    query.bindValue(QStringLiteral(":accountId"), uniqueAccountId);

    if (!query.exec()) {
        return Error(ErrorCode::DatabaseFailure,
                     QStringLiteral("Could not read the latest booking date: %1")
                         .arg(query.lastError().text()));
    }

    // An account without a single stored booking. Nothing went wrong, there is
    // just no day to start from, and the fetch runs without one.
    if (!query.next()) {
        return QDate();
    }

    return query.value(0).toDate();
}
