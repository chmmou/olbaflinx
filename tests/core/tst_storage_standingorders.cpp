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
#include "core/Banking/StandingOrder/StandingOrder.h"
#include "core/Error.h"
#include "core/Storage/Storage.h"

#include "StandingOrderHelpers.h"
#include "TestHelpers.h"
#include "TransactionHelpers.h"

#include <QtTest/QtTest>

#include <chrono>
#include <memory>

using namespace olbaflinx::core;
using namespace olbaflinx::core::banking;
using namespace olbaflinx::core::banking::account;
using namespace olbaflinx::core::banking::standingorder;
using namespace olbaflinx::core::storage;

namespace olbaflinx::core::storage::tests {

using namespace olbaflinx::core::tests;

/**
 * The place a standing order lives in: the schema step that creates it, the two
 * ways an order is recognised again, and the mark that says a fetch no longer
 * reports it. Every test function gets a temporary directory of its own, so no
 * two runs of this binary share a file.
 */
class StorageStandingOrdersTest final : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> workingDirectory;

    static ApplicationInfo applicationInfo()
    {
        return TestHelpers::applicationInfo(QStringLiteral("OlbaFlinxStorageStandingOrdersTest"));
    }

    static QString password() { return TestHelpers::password(); }

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
        return TestHelpers::rowCount(file, password(), table);
    }

    /**
     * Opens a storage, creating it where none is there yet, and leaves it open
     * under the handle the caller holds.
     */
    static bool openStorage(Storage &storage, const QString &file)
    {
        return !storage.setKey(password()).isError()
               && (storage.setStorageFile(file), !storage.initialize(true).isError());
    }

    /** A storage that is opened and closed again, for its schema alone. */
    bool createStorage() const
    {
        Storage storage(applicationInfo());
        const auto opened = openStorage(storage, storageFile());
        storage.close();

        return opened;
    }

    /**
     * Runs one write and hands back the number of rows that were added, or -1
     * when the run reported none.
     */
    static int storeAndWait(Storage &storage,
                            const BankingItems &items,
                            const StandingOrderRun &run = {})
    {
        QSignalSpy storedSpy(&storage, &Storage::itemsStored);
        QSignalSpy finishedSpy(&storage, &Storage::writeFinished);

        if (storage.storeItems(items, run).isError()) {
            return -1;
        }

        if (finishedSpy.isEmpty() && !finishedSpy.wait(workerTimeout)) {
            return -1;
        }

        return storedSpy.isEmpty() ? -1 : storedSpy.takeFirst().at(0).toInt();
    }

    /** Reads back what a read of the given query hands over. */
    static BankingItems readAndWait(Storage &storage, const Storage::ItemQuery &query)
    {
        QSignalSpy itemsSpy(&storage, &Storage::itemsReceived);

        if (storage.receiveItems(query).isError()) {
            return {};
        }

        if (itemsSpy.isEmpty() && !itemsSpy.wait(workerTimeout)) {
            return {};
        }

        return qvariant_cast<BankingItems>(itemsSpy.takeFirst().at(0));
    }

    /** The account every order of this binary hangs on. */
    static bool storeTestAccount(Storage &storage);

private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void aFileAtSchemaFourIsTakenAndCarriesTheTable();
    void aFileAtSchemaThreeReachesFiveInOneStep();
    void theHoldingSurvivesTheStep();
    void aSecondOpenChangesNeitherVersionNorRecord();
    void aFileFromANewerBuildIsRefused();
    void aTableWithoutAnExpectedColumnIsRefused();

    void twentyOrdersBecomeTwentyRows();
    void theSameTwentyWrittenAgainLeaveTwenty();
    void anOrderWithAnIdentifierIsFoundByIt();
    void anOrderWithoutAnIdentifierIsFoundByItsFingerprint();
    void anOrderThatGainsAnIdentifierStaysOne();
    void twoRowsUnderOneIdentifierAreToldApartByTheirFingerprint();
    void anOrderTheFetchNoLongerReportsIsMarkedAndStays();
    void aSuccessfulFetchWithoutAnyOrderMarksTheWholeHolding();
    void anOrderThatComesBackLosesTheMark();
    void aFetchThatDidNotSucceedMarksNothing();
    void aReadOverTransactionsHandsBackNoStandingOrder();
    void twoIdenticalOrdersWithoutAnIdentifierBecomeOne();
    void aStorageThatIsNotOpenAnswersWithAFailure();
};

namespace {

constexpr quint32 testAccountId = 4711;

/** The statement that reads the schema version out of a file. */
QString versionStatement()
{
    return QStringLiteral("SELECT COALESCE(MAX(CAST(substr(name, 1, 4) AS INTEGER)), 0) FROM "
                          "migrations WHERE migrated = 1;");
}

/**
 * Puts a storage back to the state a file carried before this step: the fifth
 * migration undone and the table gone. Nothing else of the schema differs, so
 * this is what a file of the previous build looks like from here.
 */
bool putBackToSchemaFour(const QString &file, const QString &key)
{
    return TestHelpers::runStatement(file,
                                     key,
                                     QStringLiteral("DELETE FROM migrations WHERE name LIKE "
                                                    "'0005%';"))
           && TestHelpers::runStatement(file,
                                        key,
                                        QStringLiteral("DROP TABLE IF EXISTS standing_orders;"));
}

/** The same for a file that never saw the step before this one either. */
bool putBackToSchemaThree(const QString &file, const QString &key)
{
    return putBackToSchemaFour(file, key)
           && TestHelpers::runStatement(file,
                                        key,
                                        QStringLiteral("DELETE FROM migrations WHERE name LIKE "
                                                       "'0004%';"))
           && TestHelpers::runStatement(file,
                                        key,
                                        QStringLiteral("DROP INDEX IF EXISTS "
                                                       "transactions_hash_unique_index;"));
}

/** How many indexes of the new table the catalogue carries. */
int indexCountOf(const QString &file, const QString &key)
{
    return TestHelpers::storageScalar(file,
                                      key,
                                      QStringLiteral("SELECT COUNT(*) FROM sqlite_master WHERE "
                                                     "type = 'index' AND name LIKE "
                                                     "'standing_orders_%';"))
        .toInt();
}

/** One order of the test account, told from the others by its purpose. */
BankingItemPtr makeOrder(const QString &purpose,
                         double value = 42.5,
                         const QString &fiId = {},
                         const QString &remoteName = QStringLiteral("Erika Musterfrau"))
{
    auto spec = StandingOrderSpec{};
    spec.uniqueAccountId = testAccountId;
    spec.purpose = purpose;
    spec.value = value;
    spec.fiId = fiId;
    spec.remoteName = remoteName;

    return StandingOrderHelpers::fromBackend(spec);
}

} // namespace

bool StorageStandingOrdersTest::storeTestAccount(Storage &storage)
{
    const auto account = Account::fromMap(TestHelpers::accountMapWith(testAccountId, 100.0));

    return storeAndWait(storage, BankingItems{account}) == 1;
}

void StorageStandingOrdersTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);

    QVERIFY(TestHelpers::useTemporaryHome());
}

void StorageStandingOrdersTest::init()
{
    workingDirectory = std::make_unique<QTemporaryDir>();
    QVERIFY(workingDirectory->isValid());
}

void StorageStandingOrdersTest::cleanup()
{
    workingDirectory.reset();
}

/**
 * A file written by the build before this one is taken, not refused, and it
 * carries the table afterwards. All three marks of the step are read: the
 * migration is recorded as done, the indexes are in the catalogue, and the
 * version has moved.
 */
void StorageStandingOrdersTest::aFileAtSchemaFourIsTakenAndCarriesTheTable()
{
    const auto file = storageFile();

    QVERIFY(createStorage());
    QVERIFY(putBackToSchemaFour(file, password()));

    QCOMPARE(scalarOf(file, versionStatement()).toInt(), 4);
    QCOMPARE(indexCountOf(file, password()), 0);

    {
        Storage storage(applicationInfo());
        QVERIFY(openStorage(storage, file));
        QVERIFY(storage.isValid());
        storage.close();
    }

    QCOMPARE(scalarOf(file, versionStatement()).toInt(), 5);
    QCOMPARE(scalarOf(file,
                      QStringLiteral("SELECT COUNT(*) FROM migrations WHERE name = "
                                     "'0005_standing_orders' AND migrated = 1;"))
                 .toInt(),
             1);
    QCOMPARE(indexCountOf(file, password()), 5);
    QCOMPARE(rowsOf(file, QStringLiteral("standing_orders")), 0);
}

/**
 * The schema resource runs whole on every step, so a file that missed two of
 * them takes both at once. What the step before this one brought has to be
 * there afterwards as well.
 */
void StorageStandingOrdersTest::aFileAtSchemaThreeReachesFiveInOneStep()
{
    const auto file = storageFile();

    QVERIFY(createStorage());
    QVERIFY(putBackToSchemaThree(file, password()));

    QCOMPARE(scalarOf(file, versionStatement()).toInt(), 3);

    {
        Storage storage(applicationInfo());
        QVERIFY(openStorage(storage, file));
        QVERIFY(storage.isValid());
        storage.close();
    }

    QCOMPARE(scalarOf(file, versionStatement()).toInt(), 5);
    QCOMPARE(indexCountOf(file, password()), 5);

    // The unique index over the bookings belongs to the step in between and has
    // to arrive with the same run.
    QCOMPARE(scalarOf(file,
                      QStringLiteral("SELECT COUNT(*) FROM sqlite_master WHERE type = 'index' AND "
                                     "name = 'transactions_hash_unique_index';"))
                 .toInt(),
             1);
}

/**
 * What the user is promised over the step: the file opens afterwards and every
 * account, booking and balance is still counted.
 */
void StorageStandingOrdersTest::theHoldingSurvivesTheStep()
{
    const auto file = storageFile();

    {
        Storage storage(applicationInfo());
        QVERIFY(openStorage(storage, file));
        QVERIFY(storeTestAccount(storage));
        QCOMPARE(storeAndWait(storage, TransactionHelpers::transactionRun(testAccountId, 30)), 30);
        storage.close();
    }

    const auto accountsBefore = rowsOf(file, QStringLiteral("accounts"));
    const auto transactionsBefore = rowsOf(file, QStringLiteral("transactions"));
    const auto balancesBefore = rowsOf(file, QStringLiteral("balances"));

    QCOMPARE(accountsBefore, 1);
    QCOMPARE(transactionsBefore, 30);

    QVERIFY(putBackToSchemaFour(file, password()));

    {
        Storage storage(applicationInfo());
        QVERIFY(openStorage(storage, file));
        QVERIFY(storage.isValid());
        storage.close();
    }

    QCOMPARE(rowsOf(file, QStringLiteral("accounts")), accountsBefore);
    QCOMPARE(rowsOf(file, QStringLiteral("transactions")), transactionsBefore);
    QCOMPARE(rowsOf(file, QStringLiteral("balances")), balancesBefore);
}

/**
 * The statements of the schema resource run again on every step, so they have
 * to be harmless a second time. A record written twice would raise the version
 * without anything having changed.
 */
void StorageStandingOrdersTest::aSecondOpenChangesNeitherVersionNorRecord()
{
    const auto file = storageFile();

    QVERIFY(createStorage());

    const auto records = rowsOf(file, QStringLiteral("migrations"));
    QCOMPARE(scalarOf(file, versionStatement()).toInt(), 5);

    QVERIFY(createStorage());

    QCOMPARE(rowsOf(file, QStringLiteral("migrations")), records);
    QCOMPARE(scalarOf(file, versionStatement()).toInt(), 5);
    QCOMPARE(indexCountOf(file, password()), 5);
}

/**
 * A file that carries a record this build does not know was written by a newer
 * one. Opening it would run statements against a schema whose shape is unknown.
 */
void StorageStandingOrdersTest::aFileFromANewerBuildIsRefused()
{
    const auto file = storageFile();

    QVERIFY(createStorage());
    QVERIFY(runStatement(file,
                         QStringLiteral("INSERT INTO migrations (name) VALUES "
                                        "('0006_from_a_newer_build');")));

    Storage storage(applicationInfo());
    QVERIFY(!storage.setKey(password()).isError());
    storage.setStorageFile(file);

    QVERIFY(storage.initialize(true).isError());
}

/**
 * A file that lost a column of the new table is named as such rather than
 * failing later on a bind that says nothing. Without the entry in the list of
 * expected columns the check would walk past the table altogether.
 */
void StorageStandingOrdersTest::aTableWithoutAnExpectedColumnIsRefused()
{
    const auto file = storageFile();

    QVERIFY(createStorage());

    // The table is rebuilt without one column, under the name the schema
    // resource uses, so the create of the next run passes over it.
    QVERIFY(runStatement(file, QStringLiteral("DROP TABLE standing_orders;")));
    QVERIFY(runStatement(file,
                         QStringLiteral("CREATE TABLE standing_orders (id integer not null "
                                        "primary key autoincrement, account_id integer, "
                                        "unique_account_id unsigned integer, fi_id varchar, "
                                        "unique_id unsigned integer, fingerprint varchar not "
                                        "null, identified_by integer not null default 2);")));

    Storage storage(applicationInfo());
    QVERIFY(!storage.setKey(password()).isError());
    storage.setStorageFile(file);

    const auto error = storage.initialize(true);
    QVERIFY(error.isError());
    QCOMPARE(error.code(), ErrorCode::SchemaMismatch);
}

void StorageStandingOrdersTest::twentyOrdersBecomeTwentyRows()
{
    const auto file = storageFile();

    Storage storage(applicationInfo());
    QVERIFY(openStorage(storage, file));
    QVERIFY(storeTestAccount(storage));

    const auto orders = StandingOrderHelpers::orderRun(testAccountId, 20);
    QCOMPARE(storeAndWait(storage, orders), 20);

    const auto readBack = readAndWait(storage,
                                      {.type = Storage::StorageStandingOrder,
                                       .accountId = testAccountId,
                                       .limit = 100});
    QCOMPARE(readBack.size(), 20);
    QCOMPARE(readBack.at(0)->itemType(), QStringLiteral("StandingOrder"));

    storage.close();

    QCOMPARE(rowsOf(file, QStringLiteral("standing_orders")), 20);
}

void StorageStandingOrdersTest::theSameTwentyWrittenAgainLeaveTwenty()
{
    const auto file = storageFile();

    Storage storage(applicationInfo());
    QVERIFY(openStorage(storage, file));
    QVERIFY(storeTestAccount(storage));

    QCOMPARE(storeAndWait(storage, StandingOrderHelpers::orderRun(testAccountId, 20)), 20);

    // A second fetch of the same account reports the same orders. None of them
    // is a new one.
    QCOMPARE(storeAndWait(storage, StandingOrderHelpers::orderRun(testAccountId, 20)), 0);

    storage.close();

    QCOMPARE(rowsOf(file, QStringLiteral("standing_orders")), 20);
}

/**
 * The identifier of the institution comes first. An order whose amount changed
 * carries a different fingerprint and is still the same order, so the row is
 * updated rather than written a second time.
 */
void StorageStandingOrdersTest::anOrderWithAnIdentifierIsFoundByIt()
{
    const auto file = storageFile();

    Storage storage(applicationInfo());
    QVERIFY(openStorage(storage, file));
    QVERIFY(storeTestAccount(storage));

    QCOMPARE(storeAndWait(storage,
                          BankingItems{
                              makeOrder(QStringLiteral("Miete"), 42.5, QStringLiteral("DA-0815"))}),
             1);

    QCOMPARE(storeAndWait(storage,
                          BankingItems{
                              makeOrder(QStringLiteral("Miete"), 55.0, QStringLiteral("DA-0815"))}),
             0);

    storage.close();

    QCOMPARE(rowsOf(file, QStringLiteral("standing_orders")), 1);
    QCOMPARE(scalarOf(file, QStringLiteral("SELECT `value` FROM standing_orders;")).toDouble(),
             55.0);
    QCOMPARE(scalarOf(file, QStringLiteral("SELECT identified_by FROM standing_orders;")).toInt(),
             1);
}

/**
 * Without an identifier the fingerprint carries the recognition. An order whose
 * amount changed is a different one to it, and that is the limit the fallback
 * has.
 */
void StorageStandingOrdersTest::anOrderWithoutAnIdentifierIsFoundByItsFingerprint()
{
    const auto file = storageFile();

    Storage storage(applicationInfo());
    QVERIFY(openStorage(storage, file));
    QVERIFY(storeTestAccount(storage));

    QCOMPARE(storeAndWait(storage, BankingItems{makeOrder(QStringLiteral("Miete"))}), 1);
    QCOMPARE(storeAndWait(storage, BankingItems{makeOrder(QStringLiteral("Miete"))}), 0);

    QCOMPARE(rowsOf(file, QStringLiteral("standing_orders")), 1);
    QCOMPARE(scalarOf(file, QStringLiteral("SELECT identified_by FROM standing_orders;")).toInt(),
             2);

    QCOMPARE(storeAndWait(storage, BankingItems{makeOrder(QStringLiteral("Miete"), 55.0)}), 1);

    storage.close();

    QCOMPARE(rowsOf(file, QStringLiteral("standing_orders")), 2);
}

/**
 * An institution may report an order without an identifier once and with one
 * the next time. The fingerprint finds the row, the identifier is written into
 * it, and no second order comes into being.
 */
void StorageStandingOrdersTest::anOrderThatGainsAnIdentifierStaysOne()
{
    const auto file = storageFile();

    Storage storage(applicationInfo());
    QVERIFY(openStorage(storage, file));
    QVERIFY(storeTestAccount(storage));

    QCOMPARE(storeAndWait(storage, BankingItems{makeOrder(QStringLiteral("Miete"))}), 1);
    QCOMPARE(scalarOf(file, QStringLiteral("SELECT identified_by FROM standing_orders;")).toInt(),
             2);

    QCOMPARE(storeAndWait(storage,
                          BankingItems{
                              makeOrder(QStringLiteral("Miete"), 42.5, QStringLiteral("DA-0815"))}),
             0);

    storage.close();

    QCOMPARE(rowsOf(file, QStringLiteral("standing_orders")), 1);
    QCOMPARE(scalarOf(file, QStringLiteral("SELECT fi_id FROM standing_orders;")).toString(),
             QStringLiteral("DA-0815"));
    QCOMPARE(scalarOf(file, QStringLiteral("SELECT identified_by FROM standing_orders;")).toInt(),
             1);
}

/**
 * The uniqueness of the identifier is not assumed. Where a holding carries two
 * rows under one, the fingerprint decides between them, and an order that
 * matches neither is written rather than overwriting one of them.
 *
 * No path of the application produces such a holding: a second order under an
 * identifier that is already there updates the row it finds. The state is set up
 * by SQL for that reason.
 */
void StorageStandingOrdersTest::twoRowsUnderOneIdentifierAreToldApartByTheirFingerprint()
{
    const auto file = storageFile();

    Storage storage(applicationInfo());
    QVERIFY(openStorage(storage, file));
    QVERIFY(storeTestAccount(storage));

    QCOMPARE(storeAndWait(storage,
                          BankingItems{makeOrder(QStringLiteral("Miete")),
                                       makeOrder(QStringLiteral("Strom"), 99.0)}),
             2);

    QVERIFY(runStatement(file,
                         QStringLiteral("UPDATE standing_orders SET fi_id = 'DA-0815', "
                                        "identified_by = 1;")));

    // The one whose content matches is the one that is updated. The next
    // execution moved, which is no part of the fingerprint, so the delivered
    // order is still the stored one. The other row carries the same identifier
    // and is left alone.
    auto delivered = StandingOrderSpec{};
    delivered.uniqueAccountId = testAccountId;
    delivered.purpose = QStringLiteral("Strom");
    delivered.value = 99.0;
    delivered.fiId = QStringLiteral("DA-0815");
    delivered.nextDate = QDate(2026, 4, 1);

    QCOMPARE(storeAndWait(storage, BankingItems{StandingOrderHelpers::fromBackend(delivered)}), 0);

    QCOMPARE(rowsOf(file, QStringLiteral("standing_orders")), 2);
    QCOMPARE(scalarOf(file,
                      QStringLiteral("SELECT next_date FROM standing_orders WHERE purpose = "
                                     "'Strom';"))
                 .toDate(),
             QDate(2026, 4, 1));
    QCOMPARE(scalarOf(file,
                      QStringLiteral("SELECT next_date FROM standing_orders WHERE purpose = "
                                     "'Miete';"))
                 .toDate(),
             QDate(2026, 3, 1));

    // One that matches neither of the two leaves both standing.
    QCOMPARE(storeAndWait(storage,
                          BankingItems{makeOrder(QStringLiteral("Versicherung"),
                                                 12.0,
                                                 QStringLiteral("DA-0815"))}),
             1);

    storage.close();

    QCOMPARE(rowsOf(file, QStringLiteral("standing_orders")), 3);
}

/**
 * An order the bank no longer reports is marked and kept. Nothing of the
 * holding is removed, which is what lets a mark be taken back.
 */
void StorageStandingOrdersTest::anOrderTheFetchNoLongerReportsIsMarkedAndStays()
{
    const auto file = storageFile();

    Storage storage(applicationInfo());
    QVERIFY(openStorage(storage, file));
    QVERIFY(storeTestAccount(storage));

    const auto run = StandingOrderRun{.accountId = testAccountId, .succeeded = true};

    QCOMPARE(storeAndWait(storage,
                          BankingItems{makeOrder(QStringLiteral("Miete")),
                                       makeOrder(QStringLiteral("Strom"), 99.0)},
                          run),
             2);

    QCOMPARE(storeAndWait(storage, BankingItems{makeOrder(QStringLiteral("Miete"))}, run), 0);

    const auto readBack = readAndWait(storage,
                                      {.type = Storage::StorageStandingOrder,
                                       .accountId = testAccountId,
                                       .limit = 100});
    QCOMPARE(readBack.size(), 1);

    storage.close();

    QCOMPARE(rowsOf(file, QStringLiteral("standing_orders")), 2);
    QCOMPARE(scalarOf(file,
                      QStringLiteral("SELECT COUNT(*) FROM standing_orders WHERE ended_at IS NOT "
                                     "NULL;"))
                 .toInt(),
             1);
    QCOMPARE(scalarOf(file,
                      QStringLiteral("SELECT purpose FROM standing_orders WHERE ended_at IS NOT "
                                     "NULL;"))
                 .toString(),
             QStringLiteral("Strom"));
}

/**
 * The heaviest case of this path. A fetch that went through and reported no
 * order at all is an answer of the bank, not a failure, and it differs from an
 * aborted one in nothing but the flag that says the fetch succeeded.
 */
void StorageStandingOrdersTest::aSuccessfulFetchWithoutAnyOrderMarksTheWholeHolding()
{
    const auto file = storageFile();

    Storage storage(applicationInfo());
    QVERIFY(openStorage(storage, file));
    QVERIFY(storeTestAccount(storage));

    const auto run = StandingOrderRun{.accountId = testAccountId, .succeeded = true};

    QCOMPARE(storeAndWait(storage, StandingOrderHelpers::orderRun(testAccountId, 5), run), 5);

    QCOMPARE(storeAndWait(storage, BankingItems{}, run), 0);

    const auto readBack = readAndWait(storage,
                                      {.type = Storage::StorageStandingOrder,
                                       .accountId = testAccountId,
                                       .limit = 100});
    QVERIFY(readBack.isEmpty());

    storage.close();

    QCOMPARE(rowsOf(file, QStringLiteral("standing_orders")), 5);
    QCOMPARE(scalarOf(file,
                      QStringLiteral("SELECT COUNT(*) FROM standing_orders WHERE ended_at IS "
                                     "NULL;"))
                 .toInt(),
             0);
}

void StorageStandingOrdersTest::anOrderThatComesBackLosesTheMark()
{
    const auto file = storageFile();

    Storage storage(applicationInfo());
    QVERIFY(openStorage(storage, file));
    QVERIFY(storeTestAccount(storage));

    const auto run = StandingOrderRun{.accountId = testAccountId, .succeeded = true};

    QCOMPARE(storeAndWait(storage, BankingItems{makeOrder(QStringLiteral("Miete"))}, run), 1);
    QCOMPARE(storeAndWait(storage, BankingItems{}, run), 0);

    QCOMPARE(scalarOf(file,
                      QStringLiteral("SELECT COUNT(*) FROM standing_orders WHERE ended_at IS NOT "
                                     "NULL;"))
                 .toInt(),
             1);

    // The same order again. It is updated whole, and the mark goes with it.
    QCOMPARE(storeAndWait(storage, BankingItems{makeOrder(QStringLiteral("Miete"))}, run), 0);

    const auto readBack = readAndWait(storage,
                                      {.type = Storage::StorageStandingOrder,
                                       .accountId = testAccountId,
                                       .limit = 100});
    QCOMPARE(readBack.size(), 1);

    storage.close();

    QCOMPARE(rowsOf(file, QStringLiteral("standing_orders")), 1);
    QCOMPARE(scalarOf(file,
                      QStringLiteral("SELECT COUNT(*) FROM standing_orders WHERE ended_at IS "
                                     "NULL;"))
                 .toInt(),
             1);
}

/**
 * A fetch that was aborted, that failed, that the bank refused, or an account
 * that was passed over: in none of them does the application know what the
 * institution holds, so none of them marks anything.
 */
void StorageStandingOrdersTest::aFetchThatDidNotSucceedMarksNothing()
{
    const auto file = storageFile();

    Storage storage(applicationInfo());
    QVERIFY(openStorage(storage, file));
    QVERIFY(storeTestAccount(storage));

    QCOMPARE(storeAndWait(storage,
                          StandingOrderHelpers::orderRun(testAccountId, 5),
                          {.accountId = testAccountId, .succeeded = true}),
             5);

    QCOMPARE(storeAndWait(storage, BankingItems{}, {.accountId = testAccountId, .succeeded = false}),
             0);

    storage.close();

    QCOMPARE(scalarOf(file,
                      QStringLiteral("SELECT COUNT(*) FROM standing_orders WHERE ended_at IS "
                                     "NULL;"))
                 .toInt(),
             5);
}

/**
 * The two live in tables of their own, and the read chooses the table by the
 * type alone. A booking view neither shows an order nor counts one.
 */
void StorageStandingOrdersTest::aReadOverTransactionsHandsBackNoStandingOrder()
{
    const auto file = storageFile();

    Storage storage(applicationInfo());
    QVERIFY(openStorage(storage, file));
    QVERIFY(storeTestAccount(storage));

    QCOMPARE(storeAndWait(storage, TransactionHelpers::transactionRun(testAccountId, 7)), 7);

    const auto before = readAndWait(storage,
                                    {.type = Storage::StorageTransaction,
                                     .accountId = testAccountId,
                                     .limit = 100});
    QCOMPARE(before.size(), 7);

    QCOMPARE(storeAndWait(storage, StandingOrderHelpers::orderRun(testAccountId, 5)), 5);

    const auto after = readAndWait(storage,
                                   {.type = Storage::StorageTransaction,
                                    .accountId = testAccountId,
                                    .limit = 100});
    QCOMPARE(after.size(), 7);

    for (const auto &item : after) {
        QCOMPARE(item->itemType(), QStringLiteral("Transaction"));
    }

    storage.close();

    QCOMPARE(rowsOf(file, QStringLiteral("transactions")), 7);
    QCOMPARE(rowsOf(file, QStringLiteral("standing_orders")), 5);
}

/**
 * The documented limit of the fallback. Two orders of one account that agree in
 * every field the fingerprint is formed over cannot be told apart, so they
 * become one row. Where the institution assigns identifiers the case does not
 * arise.
 */
void StorageStandingOrdersTest::twoIdenticalOrdersWithoutAnIdentifierBecomeOne()
{
    const auto file = storageFile();

    Storage storage(applicationInfo());
    QVERIFY(openStorage(storage, file));
    QVERIFY(storeTestAccount(storage));

    QCOMPARE(storeAndWait(storage,
                          BankingItems{makeOrder(QStringLiteral("Miete")),
                                       makeOrder(QStringLiteral("Miete"))}),
             1);

    storage.close();

    QCOMPARE(rowsOf(file, QStringLiteral("standing_orders")), 1);
}

void StorageStandingOrdersTest::aStorageThatIsNotOpenAnswersWithAFailure()
{
    Storage storage(applicationInfo());

    QVERIFY(storage.storeItems(BankingItems{makeOrder(QStringLiteral("Miete"))}).isError());
    QVERIFY(storage.receiveItems({.type = Storage::StorageStandingOrder}).isError());
}

} // namespace olbaflinx::core::storage::tests

QTEST_MAIN(olbaflinx::core::storage::tests::StorageStandingOrdersTest)

#include "tst_storage_standingorders.moc"
