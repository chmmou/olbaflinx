/**
 * Copyright (C) 2021-2026, Alexander Saal <developer@olbaflinx.chm-projects.de>
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

#include "core/ApplicationInfo.h"
#include "core/Banking/Account/Account.h"
#include "core/Banking/Balance/Balance.h"
#include "core/Banking/Banking.h"
#include "core/Banking/Transaction/Transaction.h"
#include "core/Error.h"
#include "core/Storage/Storage.h"

#include "TestHelpers.h"

#include <aqbanking/types/balance.h>
#include <aqbanking/types/imexporter_context.h>
#include <aqbanking/types/value.h>

#include <gwenhywfar/gwendate.h>

#include <QtTest/QtTest>

#include <chrono>
#include <memory>

using namespace olbaflinx::core;
using namespace olbaflinx::core::banking;
using namespace olbaflinx::core::banking::account;
using namespace olbaflinx::core::banking::balance;
using namespace olbaflinx::core::banking::transaction;
using namespace olbaflinx::core::storage;

namespace olbaflinx::core::storage::tests {

using namespace olbaflinx::core::tests;

/**
 * What keeps a booking from being stored twice, and what a balance of a fetch
 * looks like once it is stored. Every test function gets a temporary directory
 * of its own, so no two runs of this binary share a file.
 */
class StorageUniqueTest final : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> workingDirectory;
    std::unique_ptr<QTemporaryDir> bankingHome;

    static ApplicationInfo applicationInfo()
    {
        return {QStringLiteral("de.chm-projects.olbaflinx.test"),
                QStringLiteral("OlbaFlinxStorageUniqueTest"),
                QStringLiteral("1.0.0")};
    }

    static QString password() { return QStringLiteral("M'yF13\"stP\\$44W0$3d/"); }

    /**
     * How long a spy waits for a signal a worker thread has to produce first.
     * The call returns the moment the signal arrives, so no test sleeps for it.
     */
    static constexpr auto workerTimeout = std::chrono::seconds{60};

    QString storageFile() const
    {
        return workingDirectory->filePath(QStringLiteral("storage.obfx"));
    }

    static QVariant scalarOf(const QString &file, const QString &statement)
    {
        return TestHelpers::storageScalar(file, password(), statement);
    }

    static bool runStatement(const QString &file, const QString &statement)
    {
        return TestHelpers::runStatement(file, password(), statement);
    }

    static int rowsOf(const QString &file, const QString &table)
    {
        return scalarOf(file, QStringLiteral("SELECT COUNT(*) FROM %1;").arg(table)).toInt();
    }

    /**
     * Runs one write and hands back the number of rows that were added, or -1
     * when the run reported none.
     */
    static int storeAndWait(Storage &storage, const BankingItems &items)
    {
        QSignalSpy storedSpy(&storage, &Storage::itemsStored);
        QSignalSpy finishedSpy(&storage, &Storage::finished);

        storage.storeItems(items);

        if (finishedSpy.isEmpty() && !finishedSpy.wait(workerTimeout)) {
            return -1;
        }

        return storedSpy.isEmpty() ? -1 : storedSpy.takeFirst().at(0).toInt();
    }

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void aHundredBookingsBecomeAHundredRows();
    void theSameHundredWrittenAgainLeaveAHundred();
    void fiftyKnownAndTenNewReportTen();
    void aBookingWithoutAFingerprintCarriesOneAfterTheWrite();
    void aBookingKeepsTheFingerprintItAlreadyCarries();
    void anAccountCarriesTheSecondOfTwoStoredBalances();
    void theBookedBalanceOfAFetchReachesTheStorage();
    void aStoredBalanceCarriesDateTypeAndCurrency();
    void aFetchWithoutABalanceLeavesTheStoredOneStanding();
    void moreThanAThousandBookingsAllArrive();
    void transactionsAndBalanceOfOneAccountBothArrive();
    void aFailureInTheMiddleOfARunLeavesNoRowBehind();
    void theAccountPathLeavesAFetchedBalanceAlone();
    void aFileAtSchemaThreeIsTakenAndCarriesTheIndex();
    void duplicateBookingsAreRemovedAndTheFirstRowStays();
    void rowsWithoutAFingerprintSurviveTheStep();
};

namespace {

constexpr quint32 testAccountId = 4711;

/**
 * A booking the way the core hands one to the storage. The type is what makes
 * it valid; a transaction of type None is refused by the write path.
 *
 * @param fingerprint Left empty, the core forms the value while it writes.
 */
BankingItemPtr makeTransaction(quint32 uniqueAccountId,
                               quint32 uniqueId,
                               const QString &purpose,
                               const QString &fingerprint = {})
{
    auto map = QMap<QString, QVariant>{
        {QStringLiteral("type"), static_cast<int>(AB_Transaction_TypeStatement)},
        {QStringLiteral("unique_account_id"), uniqueAccountId},
        {QStringLiteral("unique_id"), uniqueId},
        {QStringLiteral("date"), QDate(2026, 1, 1).addDays(uniqueId)},
        {QStringLiteral("valuta_date"), QDate(2026, 1, 1).addDays(uniqueId)},
        {QStringLiteral("value"), uniqueId * 1.5},
        {QStringLiteral("currency"), QStringLiteral("EUR")},
        {QStringLiteral("purpose"), purpose},
        {QStringLiteral("remote_name"), QStringLiteral("Partner %1").arg(uniqueId)},
    };

    if (!fingerprint.isEmpty()) {
        map[QStringLiteral("hash")] = fingerprint;
    }

    return Transaction::fromMap(map);
}

/** A booking the write path refuses, so that a run can fail in its middle. */
BankingItemPtr makeUnusableTransaction()
{
    return Transaction::fromMap(
        {{QStringLiteral("type"), static_cast<int>(AB_Transaction_TypeNone)}});
}

/** A balance the way a fetch reports one. */
BankingItemPtr makeBalance(quint32 uniqueAccountId,
                           AB_BALANCE_TYPE type,
                           const QDate &date,
                           double value)
{
    AB_BALANCE *abBalance = AB_Balance_new();

    AB_Balance_SetType(abBalance, type);

    const auto text = date.toString(QStringLiteral("yyyyMMdd")).toLatin1();
    GWEN_DATE *gwenDate = GWEN_Date_fromString(text.constData());
    AB_Balance_SetDate(abBalance, gwenDate);
    GWEN_Date_free(gwenDate);

    AB_VALUE *abValue = AB_Value_fromDouble(value);
    AB_Value_SetCurrency(abValue, "EUR");
    AB_Balance_SetValue(abBalance, abValue);
    AB_Value_free(abValue);

    auto balance = std::make_shared<Balance>(uniqueAccountId, abBalance);
    AB_Balance_free(abBalance);

    return balance;
}

/** An account under a known identifier, so that a balance can be hung on it. */
BankingItemPtr makeAccount(quint32 uniqueId, double balance)
{
    auto map = TestHelpers::createFakeAccountMap();
    map[QStringLiteral("unique_id")] = uniqueId;
    map[QStringLiteral("balance")] = balance;

    return Account::fromMap(map);
}

/** The account the banking backend would report, for the fetch path. */
std::shared_ptr<Account> makeBackendAccount(quint32 uniqueId)
{
    AB_ACCOUNT_SPEC *spec = AB_AccountSpec_new();

    AB_AccountSpec_SetUniqueId(spec, uniqueId);
    AB_AccountSpec_SetBackendName(spec, "aqhbci");
    AB_AccountSpec_SetAccountName(spec, "Girokonto");
    AB_AccountSpec_SetIban(spec, "DE02120300000000202051");
    AB_AccountSpec_SetBankCode(spec, "12030000");
    AB_AccountSpec_SetAccountNumber(spec, "0000202051");
    AB_AccountSpec_SetCurrency(spec, "EUR");

    auto account = std::make_shared<Account>(spec);
    AB_AccountSpec_free(spec);

    return account;
}

/** One balance of the response container a session would have filled. */
struct BalanceSpec
{
    AB_BALANCE_TYPE type;
    QDate date;
    double value;
};

AB_IMEXPORTER_CONTEXT *makeContext(quint32 uniqueId, const QList<BalanceSpec> &balances)
{
    AB_IMEXPORTER_CONTEXT *context = AB_ImExporterContext_new();

    AB_IMEXPORTER_ACCOUNTINFO *info
        = AB_ImExporterContext_GetOrAddAccountInfo(context,
                                                   uniqueId,
                                                   "DE02120300000000202051",
                                                   "12030000",
                                                   "0000202051",
                                                   AB_AccountType_Checking);

    for (const BalanceSpec &spec : balances) {
        AB_BALANCE *balance = AB_Balance_new();

        AB_Balance_SetType(balance, spec.type);

        const auto text = spec.date.toString(QStringLiteral("yyyyMMdd")).toLatin1();
        GWEN_DATE *date = GWEN_Date_fromString(text.constData());
        AB_Balance_SetDate(balance, date);
        GWEN_Date_free(date);

        AB_VALUE *value = AB_Value_fromDouble(spec.value);
        AB_Value_SetCurrency(value, "EUR");
        AB_Balance_SetValue(balance, value);
        AB_Value_free(value);

        AB_ImExporterAccountInfo_AddBalance(info, balance);
    }

    return context;
}

} // namespace

void StorageUniqueTest::initTestCase()
{
    // Keeps QSettings out of the real user configuration, see QStandardPaths docs.
    QStandardPaths::setTestModeEnabled(true);

    // AqBanking keeps its configuration below AQBANKING_HOME. The fetch path is
    // reached here for the balance that travels from a response container into
    // the storage.
    bankingHome = std::make_unique<QTemporaryDir>();
    QVERIFY(bankingHome->isValid());
    QVERIFY(qputenv("AQBANKING_HOME", bankingHome->path().toUtf8()));
}

void StorageUniqueTest::cleanupTestCase()
{
    qunsetenv("AQBANKING_HOME");
    bankingHome.reset();
}

void StorageUniqueTest::init()
{
    workingDirectory = std::make_unique<QTemporaryDir>();
    QVERIFY(workingDirectory->isValid());
}

void StorageUniqueTest::cleanup()
{
    workingDirectory.reset();
}

void StorageUniqueTest::aHundredBookingsBecomeAHundredRows()
{
    const auto file = storageFile();

    Storage storage(applicationInfo());
    QVERIFY(!storage.setKey(password()).isError());
    storage.setStorageFile(file);
    QVERIFY(!storage.initialize(true).isError());

    auto items = BankingItems();
    for (quint32 index = 1; index <= 100; ++index) {
        items << makeTransaction(testAccountId, index, QStringLiteral("Booking %1").arg(index));
    }

    QCOMPARE(storeAndWait(storage, items), 100);

    storage.close();

    QCOMPARE(rowsOf(file, QStringLiteral("transactions")), 100);
}

/**
 * The same holding written a second time. Nothing is added and nothing is
 * reported as a failure, which is the whole point of the unique index.
 */
void StorageUniqueTest::theSameHundredWrittenAgainLeaveAHundred()
{
    const auto file = storageFile();

    Storage storage(applicationInfo());
    QVERIFY(!storage.setKey(password()).isError());
    storage.setStorageFile(file);
    QVERIFY(!storage.initialize(true).isError());

    auto items = BankingItems();
    for (quint32 index = 1; index <= 100; ++index) {
        items << makeTransaction(testAccountId, index, QStringLiteral("Booking %1").arg(index));
    }

    QCOMPARE(storeAndWait(storage, items), 100);

    QSignalSpy errorSpy(&storage, &Storage::errorOccurred);

    QCOMPARE(storeAndWait(storage, items), 0);
    QCOMPARE(errorSpy.count(), 0);

    storage.close();

    QCOMPARE(rowsOf(file, QStringLiteral("transactions")), 100);
}

/**
 * The number a run reports is the number of rows that were added, not the
 * number of records that were handed in.
 */
void StorageUniqueTest::fiftyKnownAndTenNewReportTen()
{
    const auto file = storageFile();

    Storage storage(applicationInfo());
    QVERIFY(!storage.setKey(password()).isError());
    storage.setStorageFile(file);
    QVERIFY(!storage.initialize(true).isError());

    auto known = BankingItems();
    for (quint32 index = 1; index <= 50; ++index) {
        known << makeTransaction(testAccountId, index, QStringLiteral("Booking %1").arg(index));
    }

    QCOMPARE(storeAndWait(storage, known), 50);

    auto mixed = known;
    for (quint32 index = 51; index <= 60; ++index) {
        mixed << makeTransaction(testAccountId, index, QStringLiteral("Booking %1").arg(index));
    }

    QCOMPARE(storeAndWait(storage, mixed), 10);

    storage.close();

    QCOMPARE(rowsOf(file, QStringLiteral("transactions")), 60);
}

/**
 * The fingerprint is formed while the record is written, at the one place that
 * forms it. A booking without one is not refused.
 */
void StorageUniqueTest::aBookingWithoutAFingerprintCarriesOneAfterTheWrite()
{
    const auto file = storageFile();

    Storage storage(applicationInfo());
    QVERIFY(!storage.setKey(password()).isError());
    storage.setStorageFile(file);
    QVERIFY(!storage.initialize(true).isError());

    const auto item = makeTransaction(testAccountId, 1, QStringLiteral("Without a fingerprint"));
    QVERIFY(std::static_pointer_cast<Transaction>(item)->hash().isEmpty());

    QCOMPARE(storeAndWait(storage, BankingItems{item}), 1);

    storage.close();

    const auto stored = scalarOf(file,
                                 QStringLiteral("SELECT `hash` FROM transactions WHERE "
                                                "unique_id = 1;"))
                            .toString();

    QCOMPARE(stored.length(), 64);
}

/**
 * The other half of the same promise: a value that is already there is kept.
 * Without this the one place would be a way of computing and not a single
 * source.
 */
void StorageUniqueTest::aBookingKeepsTheFingerprintItAlreadyCarries()
{
    const auto file = storageFile();

    Storage storage(applicationInfo());
    QVERIFY(!storage.setKey(password()).isError());
    storage.setStorageFile(file);
    QVERIFY(!storage.initialize(true).isError());

    const auto fingerprint = QStringLiteral("a1b2c3d4e5f6");
    const auto item = makeTransaction(testAccountId,
                                      1,
                                      QStringLiteral("With a fingerprint"),
                                      fingerprint);

    QCOMPARE(storeAndWait(storage, BankingItems{item}), 1);

    storage.close();

    QCOMPARE(scalarOf(file, QStringLiteral("SELECT `hash` FROM transactions WHERE unique_id = 1;"))
                 .toString(),
             fingerprint);
}

/**
 * An account holds one balance. The second one written takes the place of the
 * first, it does not stand beside it.
 */
void StorageUniqueTest::anAccountCarriesTheSecondOfTwoStoredBalances()
{
    const auto file = storageFile();

    Storage storage(applicationInfo());
    QVERIFY(!storage.setKey(password()).isError());
    storage.setStorageFile(file);
    QVERIFY(!storage.initialize(true).isError());

    QCOMPARE(storeAndWait(storage, BankingItems{makeAccount(testAccountId, 100.0)}), 1);

    QCOMPARE(storeAndWait(storage,
                          BankingItems{makeBalance(testAccountId,
                                                   AB_Balance_TypeBooked,
                                                   QDate(2026, 2, 1),
                                                   500.0)}),
             1);

    QCOMPARE(storeAndWait(storage,
                          BankingItems{makeBalance(testAccountId,
                                                   AB_Balance_TypeBooked,
                                                   QDate(2026, 2, 2),
                                                   750.0)}),
             1);

    storage.close();

    QCOMPARE(rowsOf(file, QStringLiteral("balances")), 1);
    QCOMPARE(scalarOf(file, QStringLiteral("SELECT `value` FROM balances;")).toDouble(), 750.0);
    QCOMPARE(scalarOf(file, QStringLiteral("SELECT `date` FROM balances;")).toDate(),
             QDate(2026, 2, 2));
}

/**
 * The whole way from the response container into the table: the core picks the
 * booked balance over the more recent noted one, and what it picked is what
 * the storage holds afterwards, with its type.
 */
void StorageUniqueTest::theBookedBalanceOfAFetchReachesTheStorage()
{
    const auto file = storageFile();

    Storage storage(applicationInfo());
    QVERIFY(!storage.setKey(password()).isError());
    storage.setStorageFile(file);
    QVERIFY(!storage.initialize(true).isError());

    QCOMPARE(storeAndWait(storage, BankingItems{makeAccount(testAccountId, 100.0)}), 1);

    const auto account = makeBackendAccount(testAccountId);

    AB_TRANSACTION_LIST2 *commands = Banking::buildFetchCommands(*account, QDate(2026, 1, 1));
    AB_IMEXPORTER_CONTEXT *context = makeContext(testAccountId,
                                                 {{AB_Balance_TypeNoted, QDate(2026, 2, 2), 17.50},
                                                  {AB_Balance_TypeBooked,
                                                   QDate(2026, 2, 1),
                                                   1234.56}});

    const BankingItems items = Banking::itemsFromContext(context, commands);

    AB_ImExporterContext_free(context);
    AB_Transaction_List2_freeAll(commands);

    QCOMPARE(items.size(), 1);
    QCOMPARE(storeAndWait(storage, items), 1);

    storage.close();

    QCOMPARE(rowsOf(file, QStringLiteral("balances")), 1);
    QCOMPARE(scalarOf(file, QStringLiteral("SELECT `value` FROM balances;")).toDouble(), 1234.56);
    QCOMPARE(scalarOf(file, QStringLiteral("SELECT `type` FROM balances;")).toInt(),
             static_cast<int>(AB_Balance_TypeBooked));
    QCOMPARE(scalarOf(file, QStringLiteral("SELECT `date` FROM balances;")).toDate(),
             QDate(2026, 2, 1));
}

/**
 * A balance carries four values into the table, and the account it hangs on is
 * not rewritten for it.
 */
void StorageUniqueTest::aStoredBalanceCarriesDateTypeAndCurrency()
{
    const auto file = storageFile();

    Storage storage(applicationInfo());
    QVERIFY(!storage.setKey(password()).isError());
    storage.setStorageFile(file);
    QVERIFY(!storage.initialize(true).isError());

    QCOMPARE(storeAndWait(storage, BankingItems{makeAccount(testAccountId, 100.0)}), 1);

    const auto accountBefore = scalarOf(file,
                                        QStringLiteral("SELECT account_name FROM accounts WHERE "
                                                       "unique_id = %1;")
                                            .arg(testAccountId))
                                   .toString();
    const auto changedBefore = scalarOf(file,
                                        QStringLiteral("SELECT COALESCE(changed_at, '') FROM "
                                                       "accounts WHERE unique_id = %1;")
                                            .arg(testAccountId))
                                   .toString();

    QCOMPARE(storeAndWait(storage,
                          BankingItems{makeBalance(testAccountId,
                                                   AB_Balance_TypeBooked,
                                                   QDate(2026, 3, 4),
                                                   987.65)}),
             1);

    storage.close();

    QCOMPARE(scalarOf(file, QStringLiteral("SELECT `date` FROM balances;")).toDate(),
             QDate(2026, 3, 4));
    QCOMPARE(scalarOf(file, QStringLiteral("SELECT `type` FROM balances;")).toInt(),
             static_cast<int>(AB_Balance_TypeBooked));
    QCOMPARE(scalarOf(file, QStringLiteral("SELECT currency FROM balances;")).toString(),
             QStringLiteral("EUR"));
    QCOMPARE(scalarOf(file, QStringLiteral("SELECT `value` FROM balances;")).toDouble(), 987.65);

    QCOMPARE(scalarOf(file,
                      QStringLiteral("SELECT account_name FROM accounts WHERE unique_id = %1;")
                          .arg(testAccountId))
                 .toString(),
             accountBefore);
    QCOMPARE(scalarOf(file,
                      QStringLiteral("SELECT COALESCE(changed_at, '') FROM accounts WHERE "
                                     "unique_id = %1;")
                          .arg(testAccountId))
                 .toString(),
             changedBefore);
}

/**
 * A fetch that brings no balance leaves the stored one where it is. Nothing
 * empties it, which is what the third case of the selection asks for.
 */
void StorageUniqueTest::aFetchWithoutABalanceLeavesTheStoredOneStanding()
{
    const auto file = storageFile();

    Storage storage(applicationInfo());
    QVERIFY(!storage.setKey(password()).isError());
    storage.setStorageFile(file);
    QVERIFY(!storage.initialize(true).isError());

    QCOMPARE(storeAndWait(storage, BankingItems{makeAccount(testAccountId, 100.0)}), 1);
    QCOMPARE(storeAndWait(storage,
                          BankingItems{makeBalance(testAccountId,
                                                   AB_Balance_TypeBooked,
                                                   QDate(2026, 2, 1),
                                                   1234.56)}),
             1);

    // What a fetch without a balance hands to the storage: the transactions
    // alone.
    QCOMPARE(storeAndWait(storage,
                          BankingItems{
                              makeTransaction(testAccountId, 1, QStringLiteral("Booking 1"))}),
             1);

    storage.close();

    QCOMPARE(rowsOf(file, QStringLiteral("balances")), 1);
    QCOMPARE(scalarOf(file, QStringLiteral("SELECT `value` FROM balances;")).toDouble(), 1234.56);
    QCOMPARE(scalarOf(file, QStringLiteral("SELECT `type` FROM balances;")).toInt(),
             static_cast<int>(AB_Balance_TypeBooked));
}

/**
 * The bound of a thousand records belongs to a read and to its window. A write
 * knows neither a bound nor a batch, and this holds the promise rather than
 * the assumption.
 */
void StorageUniqueTest::moreThanAThousandBookingsAllArrive()
{
    const auto file = storageFile();

    Storage storage(applicationInfo());
    QVERIFY(!storage.setKey(password()).isError());
    storage.setStorageFile(file);
    QVERIFY(!storage.initialize(true).isError());

    auto items = BankingItems();
    for (quint32 index = 1; index <= 1500; ++index) {
        items << makeTransaction(testAccountId, index, QStringLiteral("Booking %1").arg(index));
    }

    QCOMPARE(storeAndWait(storage, items), 1500);

    storage.close();

    QCOMPARE(rowsOf(file, QStringLiteral("transactions")), 1500);
}

/**
 * A fetch stores in two runs, the transactions first and the balance after the
 * end of that run. The storage refuses a second run while one is going, so the
 * order is not a preference but the only way both arrive.
 */
void StorageUniqueTest::transactionsAndBalanceOfOneAccountBothArrive()
{
    const auto file = storageFile();

    Storage storage(applicationInfo());
    QVERIFY(!storage.setKey(password()).isError());
    storage.setStorageFile(file);
    QVERIFY(!storage.initialize(true).isError());

    QCOMPARE(storeAndWait(storage, BankingItems{makeAccount(testAccountId, 100.0)}), 1);

    auto transactions = BankingItems();
    for (quint32 index = 1; index <= 5; ++index) {
        transactions << makeTransaction(testAccountId,
                                        index,
                                        QStringLiteral("Booking %1").arg(index));
    }

    QSignalSpy errorSpy(&storage, &Storage::errorOccurred);

    QCOMPARE(storeAndWait(storage, transactions), 5);
    QCOMPARE(storeAndWait(storage,
                          BankingItems{makeBalance(testAccountId,
                                                   AB_Balance_TypeBooked,
                                                   QDate(2026, 2, 1),
                                                   1234.56)}),
             1);

    QCOMPARE(errorSpy.count(), 0);

    storage.close();

    QCOMPARE(rowsOf(file, QStringLiteral("transactions")), 5);
    QCOMPARE(rowsOf(file, QStringLiteral("balances")), 1);
}

/**
 * All or nothing. A record the write path refuses sits in the middle of the
 * run, and what went in before it must not stay: a half holding would move the
 * starting point of the next fetch past bookings nobody holds.
 */
void StorageUniqueTest::aFailureInTheMiddleOfARunLeavesNoRowBehind()
{
    const auto file = storageFile();

    Storage storage(applicationInfo());
    QVERIFY(!storage.setKey(password()).isError());
    storage.setStorageFile(file);
    QVERIFY(!storage.initialize(true).isError());

    auto items = BankingItems();
    for (quint32 index = 1; index <= 5; ++index) {
        items << makeTransaction(testAccountId, index, QStringLiteral("Booking %1").arg(index));
    }

    items << makeUnusableTransaction();

    for (quint32 index = 6; index <= 10; ++index) {
        items << makeTransaction(testAccountId, index, QStringLiteral("Booking %1").arg(index));
    }

    QSignalSpy errorSpy(&storage, &Storage::errorOccurred);

    QCOMPARE(storeAndWait(storage, items), 0);
    QCOMPARE(errorSpy.count(), 1);

    storage.close();

    QCOMPARE(rowsOf(file, QStringLiteral("transactions")), 0);
}

/**
 * Three cases of one rule. The account path carries no date and no type and
 * writes a placeholder for both, so it must not push aside what a fetch put
 * there. It refreshes its own placeholder, and it creates one where none is.
 */
void StorageUniqueTest::theAccountPathLeavesAFetchedBalanceAlone()
{
    const auto file = storageFile();

    Storage storage(applicationInfo());
    QVERIFY(!storage.setKey(password()).isError());
    storage.setStorageFile(file);
    QVERIFY(!storage.initialize(true).isError());

    // Third case first, it is what every account starts from: no balance yet.
    QCOMPARE(storeAndWait(storage, BankingItems{makeAccount(testAccountId, 100.0)}), 1);

    QCOMPARE(rowsOf(file, QStringLiteral("balances")), 1);
    QCOMPARE(scalarOf(file, QStringLiteral("SELECT `value` FROM balances;")).toDouble(), 100.0);
    QCOMPARE(scalarOf(file, QStringLiteral("SELECT `type` FROM balances;")).toInt(),
             static_cast<int>(AB_Balance_TypeUnknown));

    // Second case: the placeholder is its own, so it is refreshed.
    QCOMPARE(storeAndWait(storage, BankingItems{makeAccount(testAccountId, 250.0)}), 1);

    QCOMPARE(scalarOf(file, QStringLiteral("SELECT `value` FROM balances;")).toDouble(), 250.0);

    // First case: a fetched balance stands, and the account path leaves it and
    // its date and its type where they are.
    QCOMPARE(storeAndWait(storage,
                          BankingItems{makeBalance(testAccountId,
                                                   AB_Balance_TypeBooked,
                                                   QDate(2026, 2, 1),
                                                   1234.56)}),
             1);

    QCOMPARE(storeAndWait(storage, BankingItems{makeAccount(testAccountId, 999.0)}), 1);

    QCOMPARE(scalarOf(file, QStringLiteral("SELECT `value` FROM balances;")).toDouble(), 1234.56);
    QCOMPARE(scalarOf(file, QStringLiteral("SELECT `type` FROM balances;")).toInt(),
             static_cast<int>(AB_Balance_TypeBooked));
    QCOMPARE(scalarOf(file, QStringLiteral("SELECT `date` FROM balances;")).toDate(),
             QDate(2026, 2, 1));

    storage.close();
}

namespace {

/**
 * Puts a storage back to the state a file carried before this step: the fourth
 * migration undone and the index gone. Nothing else of the schema differs, so
 * this is what an older file looks like from here.
 */
bool putBackToSchemaThree(const QString &file, const QString &key)
{
    return TestHelpers::runStatement(file,
                                     key,
                                     QStringLiteral("DELETE FROM migrations WHERE name LIKE "
                                                    "'0004%';"))
           && TestHelpers::runStatement(file,
                                        key,
                                        QStringLiteral("DROP INDEX IF EXISTS "
                                                       "transactions_hash_unique_index;"));
}

int indexCountOf(const QString &file, const QString &key)
{
    return TestHelpers::storageScalar(file,
                                      key,
                                      QStringLiteral("SELECT COUNT(*) FROM sqlite_master WHERE "
                                                     "type = 'index' AND name = "
                                                     "'transactions_hash_unique_index';"))
        .toInt();
}

} // namespace

/**
 * A file from before this step is taken, not refused, and it carries the
 * uniqueness afterwards. All three marks of the step are read: the fourth
 * migration is recorded as done, the index is in the catalogue, and the log
 * names the jump.
 */
void StorageUniqueTest::aFileAtSchemaThreeIsTakenAndCarriesTheIndex()
{
    const auto file = storageFile();

    {
        Storage storage(applicationInfo());
        QVERIFY(!storage.setKey(password()).isError());
        storage.setStorageFile(file);
        QVERIFY(!storage.initialize(true).isError());
        storage.close();
    }

    QVERIFY(putBackToSchemaThree(file, password()));

    QCOMPARE(scalarOf(file,
                      QStringLiteral("SELECT COALESCE(MAX(CAST(substr(name, 1, 4) AS INTEGER)), 0) "
                                     "FROM migrations WHERE migrated = 1;"))
                 .toInt(),
             3);
    QCOMPARE(indexCountOf(file, password()), 0);

    QTest::ignoreMessage(QtInfoMsg,
                         QRegularExpression(QStringLiteral("migrated .* from schema version 3 to "
                                                           "4")));

    Storage storage(applicationInfo());
    QVERIFY(!storage.setKey(password()).isError());
    storage.setStorageFile(file);

    // The way the user opens an existing storage. Without the schema step a file
    // below the current version is refused.
    QVERIFY(!storage.initialize(true).isError());
    QVERIFY(storage.isValid());

    storage.close();

    QCOMPARE(scalarOf(file,
                      QStringLiteral("SELECT COUNT(*) FROM migrations WHERE name LIKE '0004%' AND "
                                     "migrated = 1;"))
                 .toInt(),
             1);
    QCOMPARE(indexCountOf(file, password()), 1);
}

/**
 * A holding that already carries the same booking twice. The duplicates go
 * before the index is built, and the row that was written first is the one
 * that stays.
 */
void StorageUniqueTest::duplicateBookingsAreRemovedAndTheFirstRowStays()
{
    const auto file = storageFile();

    {
        Storage storage(applicationInfo());
        QVERIFY(!storage.setKey(password()).isError());
        storage.setStorageFile(file);
        QVERIFY(!storage.initialize(true).isError());
        storage.close();
    }

    QVERIFY(putBackToSchemaThree(file, password()));

    QVERIFY(runStatement(file,
                         QStringLiteral("INSERT INTO transactions (account_id, unique_account_id, "
                                        "purpose, `hash`) VALUES (1, %1, 'First', 'samehash'), "
                                        "(1, %1, 'Second', 'samehash'), (1, %1, 'Other', "
                                        "'otherhash');")
                             .arg(testAccountId)));

    QCOMPARE(rowsOf(file, QStringLiteral("transactions")), 3);

    Storage storage(applicationInfo());
    QVERIFY(!storage.setKey(password()).isError());
    storage.setStorageFile(file);
    QVERIFY(!storage.initialize(true).isError());
    storage.close();

    QCOMPARE(rowsOf(file, QStringLiteral("transactions")), 2);
    QCOMPARE(scalarOf(file,
                      QStringLiteral("SELECT purpose FROM transactions WHERE `hash` = 'samehash';"))
                 .toString(),
             QStringLiteral("First"));
    QCOMPARE(indexCountOf(file, password()), 1);
}

/**
 * Rows without a fingerprint are not duplicates of each other. A grouping over
 * the column puts every empty value into one group, so a cleanup without the
 * exception would leave one of a thousand such rows standing.
 */
void StorageUniqueTest::rowsWithoutAFingerprintSurviveTheStep()
{
    const auto file = storageFile();

    {
        Storage storage(applicationInfo());
        QVERIFY(!storage.setKey(password()).isError());
        storage.setStorageFile(file);
        QVERIFY(!storage.initialize(true).isError());
        storage.close();
    }

    QVERIFY(putBackToSchemaThree(file, password()));

    QVERIFY(TestHelpers::putTransactions(file,
                                         password(),
                                         testAccountId,
                                         500,
                                         QStringLiteral("Without fingerprint")));

    QCOMPARE(rowsOf(file, QStringLiteral("transactions")), 500);

    Storage storage(applicationInfo());
    QVERIFY(!storage.setKey(password()).isError());
    storage.setStorageFile(file);
    QVERIFY(!storage.initialize(true).isError());
    storage.close();

    QCOMPARE(rowsOf(file, QStringLiteral("transactions")), 500);
    QCOMPARE(indexCountOf(file, password()), 1);
}

} // namespace olbaflinx::core::storage::tests

QTEST_MAIN(olbaflinx::core::storage::tests::StorageUniqueTest)

#include "tst_storage_unique.moc"
