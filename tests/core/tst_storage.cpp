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
#include "core/Banking/Transaction/Transaction.h"
#include "core/Error.h"
#include "core/Storage/Storage.h"

#include "TestHelpers.h"
#include "TransactionHelpers.h"

#include <QtCore/QList>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtTest/QtTest>

#include <chrono>
#include <memory>

using namespace olbaflinx::core;
using namespace olbaflinx::core::banking;
using namespace olbaflinx::core::banking::account;
using namespace olbaflinx::core::banking::transaction;
using namespace olbaflinx::core::storage;

namespace olbaflinx::core::storage::tests {

using namespace olbaflinx::core::tests;

/**
 * Opening, closing, keys, settings and the round trip of a record. Every test
 * function gets a temporary directory of its own, so no two runs of this binary
 * share a file and nothing survives the run.
 */
class StorageTest final : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> workingDirectory;

    static ApplicationInfo applicationInfo()
    {
        return TestHelpers::applicationInfo(QStringLiteral("OlbaFlinxStorageTest"));
    }

    static QString password() { return TestHelpers::password(); }

    /**
     * How long a spy waits for a signal a worker thread has to produce first.
     *
     * QSignalSpy::wait defaults to five seconds. Writing five accounts costs
     * three tables and a transaction each and takes a little over that, so the
     * default is too tight to rely on. The number is an upper bound and not a
     * wait: the call returns the moment the signal arrives, so no test sleeps
     * for it.
     */
    static constexpr auto workerTimeout = std::chrono::seconds{30};

    QString storageFile() const
    {
        return workingDirectory->filePath(QStringLiteral("storage.obfx"));
    }

    static QVariant scalarOf(const QString &file, const QString &statement)
    {
        return TestHelpers::storageScalar(file, password(), statement);
    }

    static bool putTransactions(const QString &file,
                                quint32 uniqueAccountId,
                                int count,
                                const QString &purpose)
    {
        return TransactionHelpers::putTransactions(file, password(), uniqueAccountId, count, purpose);
    }

    static bool putTransaction(const QString &file,
                               quint32 uniqueAccountId,
                               const QString &purpose,
                               const QString &remoteName,
                               const QDate &date,
                               double value)
    {
        return TransactionHelpers::putTransaction(file,
                                                  password(),
                                                  uniqueAccountId,
                                                  purpose,
                                                  remoteName,
                                                  date,
                                                  value);
    }

    /**
     * The schema version a file carries, read the way Storage reads it: the
     * number in the first four characters of the highest applied migration.
     */
    static int schemaVersionOf(const QString &file)
    {
        return scalarOf(file,
                        QStringLiteral("SELECT COALESCE(MAX(CAST(substr(name, 1, 4) AS INTEGER)), "
                                       "0) FROM migrations WHERE migrated = 1;"))
            .toInt();
    }

private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void isValidReturnsFalseWithoutInitialization();
    void initializeRejectsEmptyStorageFile();
    void initializeRejectsEmptyPassword();
    void initializeCreatesUsableStorage();
    void initializeRunsTwiceAndLeavesTheSchemaAtItsVersion();
    void changeKeyMakesOldPasswordInvalid();
    void settingReturnsTheDefaultForAnUnknownKey();
    void storeSettingPersistsValueUnderGroup();
    void storeItemPersistsAccountWithoutEmittingASignal();
    void storeItemKeepsBalanceAndReferenceAccounts();
    void anAccountSurvivesAReopenWithEveryVisibleProperty();
    void storingTheSameAccountTwiceLeavesOneRow();
    void anUpdateOfTheBankDetailsLeavesTheStateAlone();
    void deselectingKeepsTheRowAndItsTransactions();
    void initializeRejectsAFileFromANewerVersion();
    void receiveItemsReturnsBeforeTheItemsArrive();
    void receiveItemsSignalsArriveInOrderAndInTheCallingThread();
    void receiveItemsRefusesASecondRunWhileOneIsGoing();
    void aReadForOneAccountLeavesTheTransactionsOfAnotherOut();
    void aReadForOneAccountReturnsBeforeItsItemsAndSignalsInTheCallingThread();
    void aReadReportsHowManyRecordsMatchBeforeItReportsTheRecords();
    void aReadWithoutAMatchReportsZeroBeforeItReportsThatNothingWasFound();
    void aSearchTextReachesBeyondThePageThatWasLoaded();
    void aPeriodLetsNoTransactionOutsideItThrough();
    void aRestrictionToIncomingLetsNoDebitThrough();
    void aPercentSignInTheSearchTextIsLookedForAsACharacter();
    void closingWhileAReadIsGoingDropsItsResult();
    void closingWhileAWriteIsGoingDropsItsCount();
    void storeItemsReturnsBeforeTheAccountsAreWritten();
    void storeItemsSignalsArriveInOrderAndInTheCallingThread();
    void storeItemsRefusesASecondRunWhileOneIsGoing();
    void manyWritesLeaveNoConnectionBehind();
    void aReadAndAWriteEachReportOnlyTheirOwnEnd();
    void aRunWithoutRecordsReportsToWhoeverListensAfterTheCall();
    void anOpenStorageSaysSoAndAClosedOneDoesNot();
};

void StorageTest::initTestCase()
{
    // Keeps QSettings out of the real user configuration, see QStandardPaths docs.
    QStandardPaths::setTestModeEnabled(true);

    // Test mode alone puts the locations below ~/.qttest, which is a directory
    // of the user like any other and survives the run. HOME goes into a
    // temporary directory, so that nothing this binary writes outlives it.
    QVERIFY(TestHelpers::useTemporaryHome());
}

/**
 * init and cleanup run around every test function, not once per class. That is
 * what makes the order of the functions irrelevant.
 */
void StorageTest::init()
{
    workingDirectory = std::make_unique<QTemporaryDir>();
    QVERIFY(workingDirectory->isValid());
}

void StorageTest::cleanup()
{
    workingDirectory.reset();
}

void StorageTest::isValidReturnsFalseWithoutInitialization()
{
    Storage storage(applicationInfo());
    storage.close();

    QVERIFY(!storage.isValid());
}

void StorageTest::initializeRejectsEmptyStorageFile()
{
    Storage storage(applicationInfo());
    QVERIFY(!storage.setKey(password()).isError());
    storage.setStorageFile(QString());

    // Without a file there is no connection to open. The call used to report
    // that through a signal nobody listened to.
    const auto error = storage.initialize(true);

    QVERIFY(error.isError());
    QCOMPARE(error.code(), ErrorCode::DatabaseFailure);
    QVERIFY(!storage.isValid());

    storage.close();
}

/**
 * An empty key opens a file, it just leaves it unencrypted. The core used to
 * take it and leave the storage unusable, which only isValid then reported.
 * setKey refuses it, so the file is never created in the first place.
 */
void StorageTest::initializeRejectsEmptyPassword()
{
    const auto file = storageFile();

    Storage storage(applicationInfo());

    const auto error = storage.setKey(QString());

    QVERIFY(error.isError());
    QCOMPARE(error.code(), ErrorCode::InvalidInput);

    storage.setStorageFile(file);

    QVERIFY(!storage.isValid());
    storage.close();

    QVERIFY(!QFile::exists(file));
}

void StorageTest::initializeCreatesUsableStorage()
{
    const auto file = storageFile();

    Storage storage(applicationInfo());
    QVERIFY(!storage.setKey(password()).isError());
    storage.setStorageFile(file);

    QVERIFY(!storage.initialize(true).isError());
    QVERIFY(storage.isValid());

    storage.close();

    QVERIFY(QFile::exists(file));
}

/**
 * setupTables runs the whole schema resource again on every version step, so
 * every statement in it has to do nothing the second time round. Opening the
 * same file twice is what puts that to the test: the second open replays the
 * statements against a file that already carries what they create.
 *
 * The version is read afterwards because a statement that fails silently would
 * leave the file behind at its old number.
 */
void StorageTest::initializeRunsTwiceAndLeavesTheSchemaAtItsVersion()
{
    const auto file = storageFile();

    for (int run = 0; run < 2; ++run) {
        Storage storage(applicationInfo());
        QVERIFY(!storage.setKey(password()).isError());
        storage.setStorageFile(file);

        QVERIFY(!storage.initialize(true).isError());
        QVERIFY(storage.isValid());

        storage.close();
    }

    QCOMPARE(schemaVersionOf(file), 4);
}

void StorageTest::changeKeyMakesOldPasswordInvalid()
{
    const auto file = storageFile();
    const auto newPassword = QStringLiteral("eve3yth1ng h4$ 4n end only the s4u$a4ge h4$ 2");

    Storage storage(applicationInfo());
    QVERIFY(!storage.setKey(password()).isError());
    storage.setStorageFile(file);

    QVERIFY(!storage.initialize(true).isError());
    QVERIFY(!storage.changeKey(password(), newPassword).isError());

    storage.close();

    QVERIFY(!storage.setKey(password()).isError());
    storage.setStorageFile(file);

    // The old key no longer opens the file. Applying the schema fails, and that
    // now reaches the caller instead of ending in an unheard signal.
    QVERIFY(storage.initialize(true).isError());
    QVERIFY(!storage.isValid());

    QVERIFY(!storage.setKey(newPassword).isError());
    storage.setStorageFile(file);

    QVERIFY(!storage.initialize(true).isError());
    QVERIFY(storage.isValid());

    storage.close();
}

void StorageTest::settingReturnsTheDefaultForAnUnknownKey()
{
    Storage storage(applicationInfo());
    storage.storeSetting(QStringLiteral("Paths"), QStringList(), QStringLiteral("Vaults"));

    const auto vaults = storage
                            .setting(QStringLiteral("Paths"),
                                     QStringLiteral("Vaults"),
                                     QStringList())
                            .toStringList();

    QCOMPARE(vaults.size(), 0);
}

void StorageTest::storeSettingPersistsValueUnderGroup()
{
    Storage storage(applicationInfo());

    const QStringList written = {workingDirectory->filePath(QStringLiteral("first.obfx")),
                                 workingDirectory->filePath(QStringLiteral("second.obfx"))};

    storage.storeSetting(QStringLiteral("Paths"), written, QStringLiteral("Vaults"));

    const auto read = storage
                          .setting(QStringLiteral("Paths"), QStringLiteral("Vaults"), QStringList())
                          .toStringList();

    QCOMPARE(read, written);
}

/**
 * storeItem answers through its return value and emits nothing. A signal beside
 * it would reach the receivers of a run of storeItems, which this is not, and
 * would tell one of them that a write of its own had ended.
 */
void StorageTest::storeItemPersistsAccountWithoutEmittingASignal()
{
    Storage storage(applicationInfo());

    QSignalSpy itemsSpy(&storage, &Storage::itemsReceived);
    QSignalSpy readFinishedSpy(&storage, &Storage::readFinished);
    QSignalSpy writeFinishedSpy(&storage, &Storage::writeFinished);
    QSignalSpy writeFailedSpy(&storage, &Storage::writeFailed);

    QVERIFY(!storage.setKey(password()).isError());
    storage.setStorageFile(storageFile());

    QVERIFY(!storage.initialize(true).isError());
    QVERIFY(storage.isValid());

    const auto first = TestHelpers::createFakeAccount();
    QVERIFY(first->isValid());

    const auto second = TestHelpers::createFakeAccount();
    QVERIFY(second->isValid());

    QVERIFY(!storage.storeItem(first.get()).isError());
    QVERIFY(!storage.storeItem(second.get()).isError());

    QCOMPARE(writeFinishedSpy.count(), 0);
    QCOMPARE(writeFailedSpy.count(), 0);
    QCOMPARE(readFinishedSpy.count(), 0);

    QVERIFY(!storage.receiveItems({.type = Storage::StorageAccount}).isError());

    QVERIFY(itemsSpy.wait(workerTimeout));
    QCOMPARE(itemsSpy.count(), 1);
    QCOMPARE(readFinishedSpy.count(), 1);

    // The read ended, the write path stayed silent throughout.
    QCOMPARE(writeFinishedSpy.count(), 0);

    const auto items = qvariant_cast<BankingItems>(itemsSpy.takeFirst().at(0));
    QCOMPARE(items.size(), 2);

    // The receiver owns the records. They have to survive the return from the
    // signal; Storage used to release them right afterwards.
    QVERIFY(items.at(0) != nullptr);
    QCOMPARE(items.at(0)->itemType(), QStringLiteral("Account"));

    storage.close();
}

/**
 * The balance and the reference accounts of an account live in tables of their
 * own. Both used to be handed to bindValue under keys the insert statement did
 * not name, where they were dropped without a word.
 */
void StorageTest::storeItemKeepsBalanceAndReferenceAccounts()
{
    Storage storage(applicationInfo());

    QSignalSpy itemsSpy(&storage, &Storage::itemsReceived);

    QVERIFY(!storage.setKey(password()).isError());
    storage.setStorageFile(storageFile());

    QVERIFY(!storage.initialize(true).isError());

    const auto account = Account::fromMap(TestHelpers::createFakeAccountMapWithReferenceAccount());
    QVERIFY(account->isValid());

    const auto balance = account->balance();
    QVERIFY(balance > 0.0);

    QVERIFY(!storage.storeItem(account.get()).isError());

    QVERIFY(!storage.receiveItems({.type = Storage::StorageAccount}).isError());

    QVERIFY(itemsSpy.wait(workerTimeout));
    QCOMPARE(itemsSpy.count(), 1);

    const auto items = qvariant_cast<BankingItems>(itemsSpy.takeFirst().at(0));
    QCOMPARE(items.size(), 1);

    const auto readBack = std::dynamic_pointer_cast<Account>(items.at(0));
    QVERIFY(readBack != nullptr);
    QCOMPARE(readBack->balance(), balance);
    QCOMPARE(readBack->uniqueId(), account->uniqueId());

    const auto referenceAccounts = readBack->referenceAccounts();
    QCOMPARE(referenceAccounts.size(), 1);
    QCOMPARE(referenceAccounts.at(0)->iban(), QStringLiteral("DE02120300000000202051"));
    QCOMPARE(referenceAccounts.at(0)->ownerName(), QStringLiteral("Erika Müller-Groß"));

    storage.close();
}

/**
 * What the user is promised: the accounts chosen in the wizard are there again
 * on the next start, with everything the interface shows of them.
 */
void StorageTest::anAccountSurvivesAReopenWithEveryVisibleProperty()
{
    const auto file = storageFile();

    const auto written = Account::fromMap(TestHelpers::createFakeAccountMap());
    QVERIFY(written != nullptr);
    QVERIFY(written->isValid());

    {
        Storage storage(applicationInfo());
        QVERIFY(!storage.setKey(password()).isError());
        storage.setStorageFile(file);
        QVERIFY(!storage.initialize(true).isError());

        QVERIFY(!storage.storeItem(written.get()).isError());
        storage.close();
    }

    Storage storage(applicationInfo());
    QSignalSpy itemsSpy(&storage, &Storage::itemsReceived);

    QVERIFY(!storage.setKey(password()).isError());
    storage.setStorageFile(file);
    QVERIFY(!storage.initialize(true).isError());

    QVERIFY(!storage.receiveItems({.type = Storage::StorageAccount}).isError());
    QVERIFY(itemsSpy.wait(workerTimeout));

    const auto items = qvariant_cast<BankingItems>(itemsSpy.takeFirst().at(0));
    QCOMPARE(items.size(), 1);

    const auto readBack = std::dynamic_pointer_cast<Account>(items.at(0));
    QVERIFY(readBack != nullptr);

    QCOMPARE(readBack->accountName(), written->accountName());
    QCOMPARE(readBack->ownerName(), written->ownerName());
    QCOMPARE(readBack->bankName(), written->bankName());
    QCOMPARE(readBack->iban(), written->iban());
    QCOMPARE(readBack->bic(), written->bic());
    QCOMPARE(readBack->accountNumber(), written->accountNumber());
    QCOMPARE(readBack->currency(), written->currency());
    QCOMPARE(readBack->balance(), written->balance());

    storage.close();
}

/**
 * A second run of the wizard hands over the same accounts again. Each of them
 * has to end up in the row it already has, which is what the unique index on
 * unique_id and the upsert built on it are for.
 */
void StorageTest::storingTheSameAccountTwiceLeavesOneRow()
{
    const auto file = storageFile();

    Storage storage(applicationInfo());
    QVERIFY(!storage.setKey(password()).isError());
    storage.setStorageFile(file);
    QVERIFY(!storage.initialize(true).isError());

    const auto account = Account::fromMap(TestHelpers::createFakeAccountMap());
    QVERIFY(account != nullptr);

    QVERIFY(!storage.storeItem(account.get()).isError());
    QVERIFY(!storage.storeItem(account.get()).isError());

    storage.close();

    QCOMPARE(scalarOf(file, QStringLiteral("SELECT COUNT(*) FROM accounts;")).toInt(), 1);
}

/**
 * The update carries what the bank reports and nothing else. An account the user
 * deselected must not become visible again merely because the wizard offered it
 * once more, so the state stays out of the statement that writes the rest.
 */
void StorageTest::anUpdateOfTheBankDetailsLeavesTheStateAlone()
{
    const auto file = storageFile();
    const auto map = TestHelpers::createFakeAccountMap();
    const auto uniqueId = map.value(QStringLiteral("unique_id")).toUInt();

    Storage storage(applicationInfo());
    QVERIFY(!storage.setKey(password()).isError());
    storage.setStorageFile(file);
    QVERIFY(!storage.initialize(true).isError());

    {
        const auto deselected = Account::fromMap(map);
        QVERIFY(deselected != nullptr);
        deselected->setActive(false);

        QVERIFY(!storage.storeItem(deselected.get()).isError());
    }

    const auto stateAfterDeselect = scalarOf(file,
                                             QStringLiteral("SELECT active FROM accounts WHERE "
                                                            "unique_id = %1;")
                                                 .arg(uniqueId));
    QCOMPARE(stateAfterDeselect.toInt(), 0);

    // The same account as the bank now reports it. Nobody decided about its
    // state this time round, so nothing about the state is handed over.
    auto updated = map;
    updated[QStringLiteral("account_name")] = QStringLiteral("Girokonto neu");
    updated[QStringLiteral("owner_name")] = QStringLiteral("Erika Müller-Groß");

    const auto offeredAgain = Account::fromMap(updated);
    QVERIFY(offeredAgain != nullptr);

    QVERIFY(!storage.storeItem(offeredAgain.get()).isError());
    storage.close();

    QCOMPARE(scalarOf(file,
                      QStringLiteral("SELECT account_name FROM accounts WHERE unique_id = %1;")
                          .arg(uniqueId))
                 .toString(),
             QStringLiteral("Girokonto neu"));
    QCOMPARE(scalarOf(file,
                      QStringLiteral("SELECT owner_name FROM accounts WHERE unique_id = %1;")
                          .arg(uniqueId))
                 .toString(),
             QStringLiteral("Erika Müller-Groß"));

    QCOMPARE(scalarOf(file,
                      QStringLiteral("SELECT active FROM accounts WHERE unique_id = %1;")
                          .arg(uniqueId))
                 .toInt(),
             0);
}

/**
 * Deselecting an account keeps it. Its transactions hang on the id of its row,
 * and that id is what an INSERT OR REPLACE would have thrown away. Choosing the
 * account again therefore finds the same transactions.
 */
void StorageTest::deselectingKeepsTheRowAndItsTransactions()
{
    const auto file = storageFile();
    const auto map = TestHelpers::createFakeAccountMap();
    const auto uniqueId = map.value(QStringLiteral("unique_id")).toUInt();

    Storage storage(applicationInfo());
    QVERIFY(!storage.setKey(password()).isError());
    storage.setStorageFile(file);
    QVERIFY(!storage.initialize(true).isError());

    const auto account = Account::fromMap(map);
    QVERIFY(account != nullptr);
    QVERIFY(!storage.storeItem(account.get()).isError());

    const auto accountId
        = scalarOf(file,
                   QStringLiteral("SELECT id FROM accounts WHERE unique_id = %1;").arg(uniqueId))
              .toInt();
    QVERIFY(accountId > 0);

    // Transactions of their own, hung on the account the way the storage hangs
    // them. Written straight into the file rather than fetched, so that the
    // test needs no bank.
    QVERIFY(scalarOf(file,
                     QStringLiteral("INSERT INTO transactions (account_id, purpose) VALUES (%1, "
                                    "'Miete'), (%1, 'Gehalt') RETURNING account_id;")
                         .arg(accountId))
                .isValid());

    const auto transactionsOfTheAccount = QStringLiteral("SELECT COUNT(*) FROM transactions WHERE "
                                                         "account_id = %1;")
                                              .arg(accountId);
    QCOMPARE(scalarOf(file, transactionsOfTheAccount).toInt(), 2);

    account->setActive(false);
    QVERIFY(!storage.storeItem(account.get()).isError());

    QCOMPARE(scalarOf(file, QStringLiteral("SELECT COUNT(*) FROM accounts;")).toInt(), 1);
    QCOMPARE(scalarOf(file, transactionsOfTheAccount).toInt(), 2);

    account->setActive(true);
    QVERIFY(!storage.storeItem(account.get()).isError());

    storage.close();

    QCOMPARE(scalarOf(file,
                      QStringLiteral("SELECT id FROM accounts WHERE unique_id = %1;").arg(uniqueId))
                 .toInt(),
             accountId);
    QCOMPARE(scalarOf(file,
                      QStringLiteral("SELECT active FROM accounts WHERE unique_id = %1;")
                          .arg(uniqueId))
                 .toInt(),
             1);
    QCOMPARE(scalarOf(file, transactionsOfTheAccount).toInt(), 2);
}

/**
 * A file whose schema is newer than this build understands is refused before
 * anything reads from it or writes to it. Columns this build does not know would
 * otherwise be ignored on read and dropped on the next write.
 */
void StorageTest::initializeRejectsAFileFromANewerVersion()
{
    {
        Storage storage(applicationInfo());
        QVERIFY(!storage.setKey(password()).isError());
        storage.setStorageFile(storageFile());

        QVERIFY(!storage.initialize(true).isError());
        storage.close();
    }

    {
        auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLCIPHER"),
                                                  QStringLiteral("StorageTestFuture"));
        database.setDatabaseName(storageFile());

        QVERIFY(database.open());

        auto key = password();
        key.replace(QLatin1Char('\''), QLatin1StringView("''"));

        QSqlQuery query(database);
        QVERIFY(query.exec(QStringLiteral("PRAGMA key='%1';").arg(key)));
        QVERIFY(query.exec(
            QStringLiteral("INSERT INTO migrations (name) VALUES ('0099_from_the_future');")));

        database.close();
    }
    QSqlDatabase::removeDatabase(QStringLiteral("StorageTestFuture"));

    Storage storage(applicationInfo());
    QVERIFY(!storage.setKey(password()).isError());
    storage.setStorageFile(storageFile());

    const auto error = storage.initialize(true);

    QVERIFY(error.isError());
    QCOMPARE(error.code(), ErrorCode::SchemaMismatch);

    storage.close();
}

/**
 * The read used to hold the calling thread for as long as it took. A window of a
 * thousand accounts costs a second query per account for its balance and its
 * reference accounts, and the interface was frozen for all of it.
 *
 * The call now returns before the result is there. Nothing has arrived at the
 * moment it comes back; that is the whole point, and it is what this function
 * pins down.
 */
void StorageTest::receiveItemsReturnsBeforeTheItemsArrive()
{
    Storage storage(applicationInfo());

    QVERIFY(!storage.setKey(password()).isError());
    storage.setStorageFile(storageFile());
    QVERIFY(!storage.initialize(true).isError());

    for (int i = 0; i < 5; ++i) {
        const auto account = TestHelpers::createFakeAccount();
        QVERIFY(!storage.storeItem(account.get()).isError());
    }

    QSignalSpy itemsSpy(&storage, &Storage::itemsReceived);
    QSignalSpy finishedSpy(&storage, &Storage::readFinished);

    const int finishedBefore = finishedSpy.count();

    QVERIFY(!storage.receiveItems({.type = Storage::StorageAccount}).isError());

    // Straight after the call. No event has been processed yet, so nothing can
    // have been delivered even if the worker were already done.
    QCOMPARE(itemsSpy.count(), 0);
    QCOMPARE(finishedSpy.count(), finishedBefore);

    QVERIFY(itemsSpy.wait(workerTimeout));
    QCOMPARE(itemsSpy.count(), 1);

    storage.close();
}

/**
 * The signals belong to the thread that called, not to the one that read. A
 * receiver connected to them may touch the interface, which is only allowed
 * there. The order matters too: whoever waits for finished has to be able to
 * assume that the items have already been handed over.
 */
void StorageTest::receiveItemsSignalsArriveInOrderAndInTheCallingThread()
{
    Storage storage(applicationInfo());

    QVERIFY(!storage.setKey(password()).isError());
    storage.setStorageFile(storageFile());
    QVERIFY(!storage.initialize(true).isError());

    for (int i = 0; i < 3; ++i) {
        const auto account = TestHelpers::createFakeAccount();
        QVERIFY(!storage.storeItem(account.get()).isError());
    }

    QThread *const callingThread = QThread::currentThread();
    QStringList order;
    QList<QThread *> threads;

    connect(&storage, &Storage::readProgressChanged, &storage, [&](int) {
        if (order.isEmpty() || order.last() != QStringLiteral("progress")) {
            order << QStringLiteral("progress");
        }
        threads << QThread::currentThread();
    });

    connect(&storage, &Storage::itemsReceived, &storage, [&](const BankingItems &) {
        order << QStringLiteral("items");
        threads << QThread::currentThread();
    });

    QSignalSpy finishedSpy(&storage, &Storage::readFinished);
    connect(&storage, &Storage::readFinished, &storage, [&]() {
        order << QStringLiteral("finished");
        threads << QThread::currentThread();
    });

    QVERIFY(!storage.receiveItems({.type = Storage::StorageAccount}).isError());

    QVERIFY(finishedSpy.wait(workerTimeout));

    QCOMPARE(order,
             QStringList{} << QStringLiteral("progress") << QStringLiteral("items")
                           << QStringLiteral("finished"));

    QVERIFY(!threads.isEmpty());
    for (QThread *const thread : std::as_const(threads)) {
        QCOMPARE(thread, callingThread);
    }

    storage.close();
}

/**
 * The failure case for the second run. Two readers on one storage are not
 * provided for, and the watcher of the first would be lost. Refused with an
 * error rather than left to chance.
 *
 * The refusal goes to whoever asked and to nobody else. It used to travel as
 * readFailed and readFinished, which are the signals of a run: whoever was
 * waiting for the run that was still going took them for its end, parted from
 * the records it had asked for, and never saw them arrive.
 */
void StorageTest::receiveItemsRefusesASecondRunWhileOneIsGoing()
{
    Storage storage(applicationInfo());

    QVERIFY(!storage.setKey(password()).isError());
    storage.setStorageFile(storageFile());
    QVERIFY(!storage.initialize(true).isError());

    for (int i = 0; i < 5; ++i) {
        const auto account = TestHelpers::createFakeAccount();
        QVERIFY(!storage.storeItem(account.get()).isError());
    }

    QSignalSpy errorSpy(&storage, &Storage::readFailed);
    QSignalSpy finishedSpy(&storage, &Storage::readFinished);
    QSignalSpy itemsSpy(&storage, &Storage::itemsReceived);

    QVERIFY(!storage.receiveItems({.type = Storage::StorageAccount}).isError());

    // The first run is still going, this thread has not processed an event since
    // it started. The refusal comes back in this thread, before any waiting.
    const auto refused = storage.receiveItems({.type = Storage::StorageAccount});

    QVERIFY(refused.isError());
    QCOMPARE(refused.code(), ErrorCode::Busy);

    QCOMPARE(errorSpy.count(), 0);
    QCOMPARE(finishedSpy.count(), 0);

    // The first run is unaffected and still delivers.
    QVERIFY(itemsSpy.wait(workerTimeout));
    QCOMPARE(itemsSpy.count(), 1);
    QCOMPARE(finishedSpy.count(), 1);

    storage.close();
}

/**
 * The filter goes over the identifier the institution assigns, not over the row
 * id of the accounts table. Every transaction below carries the same account_id
 * and they differ in unique_account_id alone, so a read over the wrong column
 * brings all of them back.
 */
void StorageTest::aReadForOneAccountLeavesTheTransactionsOfAnotherOut()
{
    const auto file = storageFile();

    constexpr quint32 ownAccount = 815;
    constexpr quint32 otherAccount = 4711;

    Storage storage(applicationInfo());

    QVERIFY(!storage.setKey(password()).isError());
    storage.setStorageFile(file);
    QVERIFY(!storage.initialize(true).isError());

    QVERIFY(putTransactions(file, ownAccount, 2, QStringLiteral("Miete")));
    QVERIFY(putTransactions(file, otherAccount, 3, QStringLiteral("Gehalt")));

    QSignalSpy itemsSpy(&storage, &Storage::itemsReceived);

    QVERIFY(!storage.receiveItems({.type = Storage::StorageTransaction, .accountId = ownAccount})
                 .isError());

    QVERIFY(itemsSpy.wait(workerTimeout));

    const auto items = qvariant_cast<BankingItems>(itemsSpy.takeFirst().at(0));
    QCOMPARE(items.size(), 2);

    for (const auto &item : std::as_const(items)) {
        const auto transaction = std::dynamic_pointer_cast<Transaction>(item);
        QVERIFY(transaction != nullptr);
        QCOMPARE(transaction->uniqueAccountId(), ownAccount);
    }

    storage.close();
}

/**
 * What the read owes its caller does not change because the query grew a filter.
 * The call comes back before anything has arrived, and every signal reaches the
 * thread that called, in the order a receiver may rely on.
 */
void StorageTest::aReadForOneAccountReturnsBeforeItsItemsAndSignalsInTheCallingThread()
{
    const auto file = storageFile();

    constexpr quint32 ownAccount = 815;

    Storage storage(applicationInfo());

    QVERIFY(!storage.setKey(password()).isError());
    storage.setStorageFile(file);
    QVERIFY(!storage.initialize(true).isError());

    QVERIFY(putTransactions(file, ownAccount, 60, QStringLiteral("Miete")));

    QThread *const callingThread = QThread::currentThread();
    QStringList order;
    QList<QThread *> threads;

    connect(&storage, &Storage::readProgressChanged, &storage, [&](int) {
        if (order.isEmpty() || order.last() != QStringLiteral("progress")) {
            order << QStringLiteral("progress");
        }
        threads << QThread::currentThread();
    });

    connect(&storage, &Storage::itemsCounted, &storage, [&](int) {
        order << QStringLiteral("count");
        threads << QThread::currentThread();
    });

    connect(&storage, &Storage::itemsReceived, &storage, [&](const BankingItems &) {
        order << QStringLiteral("items");
        threads << QThread::currentThread();
    });

    QSignalSpy finishedSpy(&storage, &Storage::readFinished);
    connect(&storage, &Storage::readFinished, &storage, [&]() {
        order << QStringLiteral("finished");
        threads << QThread::currentThread();
    });

    QVERIFY(!storage.receiveItems({.type = Storage::StorageTransaction, .accountId = ownAccount})
                 .isError());

    // Straight after the call. No event has been processed yet, so nothing can
    // have been delivered even if the worker were already done.
    QVERIFY(order.isEmpty());

    QVERIFY(finishedSpy.wait(workerTimeout));

    QCOMPARE(order,
             QStringList{} << QStringLiteral("progress") << QStringLiteral("count")
                           << QStringLiteral("items") << QStringLiteral("finished"));

    QVERIFY(!threads.isEmpty());
    for (QThread *const thread : std::as_const(threads)) {
        QCOMPARE(thread, callingThread);
    }

    storage.close();
}

/**
 * The number the filter bar shows counts the whole holding under the condition,
 * not the window that was read. The account below holds more transactions than
 * one window carries, so a count taken from the window would answer the size of
 * the window instead.
 */
void StorageTest::aReadReportsHowManyRecordsMatchBeforeItReportsTheRecords()
{
    const auto file = storageFile();

    constexpr quint32 ownAccount = 815;
    constexpr quint32 otherAccount = 4711;
    constexpr int ownTransactions = 120;
    constexpr int window = 50;

    Storage storage(applicationInfo());

    QVERIFY(!storage.setKey(password()).isError());
    storage.setStorageFile(file);
    QVERIFY(!storage.initialize(true).isError());

    QVERIFY(putTransactions(file, ownAccount, ownTransactions, QStringLiteral("Miete")));
    QVERIFY(putTransactions(file, otherAccount, 30, QStringLiteral("Gehalt")));

    QStringList order;

    connect(&storage, &Storage::itemsCounted, &storage, [&](int) {
        order << QStringLiteral("count");
    });

    connect(&storage, &Storage::itemsReceived, &storage, [&](const BankingItems &) {
        order << QStringLiteral("items");
    });

    QSignalSpy countSpy(&storage, &Storage::itemsCounted);
    QSignalSpy itemsSpy(&storage, &Storage::itemsReceived);

    QVERIFY(!storage
                 .receiveItems(
                     {.type = Storage::StorageTransaction, .accountId = ownAccount, .limit = window})
                 .isError());

    QVERIFY(itemsSpy.wait(workerTimeout));

    QCOMPARE(countSpy.count(), 1);
    QCOMPARE(countSpy.takeFirst().at(0).toInt(), ownTransactions);
    QCOMPARE(qvariant_cast<BankingItems>(itemsSpy.takeFirst().at(0)).size(), window);

    QCOMPARE(order, QStringList{} << QStringLiteral("count") << QStringLiteral("items"));

    storage.close();
}

/**
 * A condition that no record satisfies is not a failure of the count. It reports
 * zero, and it does so before the storage reports that it found nothing, so that
 * whoever picks the empty state to show already holds the number.
 */
void StorageTest::aReadWithoutAMatchReportsZeroBeforeItReportsThatNothingWasFound()
{
    const auto file = storageFile();

    constexpr quint32 ownAccount = 815;
    constexpr quint32 otherAccount = 4711;

    Storage storage(applicationInfo());

    QVERIFY(!storage.setKey(password()).isError());
    storage.setStorageFile(file);
    QVERIFY(!storage.initialize(true).isError());

    QVERIFY(putTransactions(file, otherAccount, 30, QStringLiteral("Gehalt")));

    QStringList order;

    connect(&storage, &Storage::itemsCounted, &storage, [&](int) {
        order << QStringLiteral("count");
    });

    connect(&storage, &Storage::readFailed, &storage, [&](ErrorCode, const QString &) {
        order << QStringLiteral("error");
    });

    QSignalSpy countSpy(&storage, &Storage::itemsCounted);
    QSignalSpy errorSpy(&storage, &Storage::readFailed);

    QVERIFY(!storage.receiveItems({.type = Storage::StorageTransaction, .accountId = ownAccount})
                 .isError());

    QVERIFY(errorSpy.wait(workerTimeout));

    QCOMPARE(countSpy.count(), 1);
    QCOMPARE(countSpy.takeFirst().at(0).toInt(), 0);
    QCOMPARE(errorSpy.takeFirst().at(0).value<ErrorCode>(), ErrorCode::NotFound);

    QCOMPARE(order, QStringList{} << QStringLiteral("count") << QStringLiteral("error"));

    storage.close();
}

/**
 * The filter belongs in the query and not over the rows that were read. Three
 * thousand transactions, a window of fifty, and the one the text matches sits
 * far behind that window: a filter over the loaded rows would answer that
 * nothing was found while it lay untouched in the storage.
 *
 * The call still returns before the records arrive, as every read does.
 */
void StorageTest::aSearchTextReachesBeyondThePageThatWasLoaded()
{
    const auto file = storageFile();

    constexpr quint32 ownAccount = 815;

    Storage storage(applicationInfo());

    QVERIFY(!storage.setKey(password()).isError());
    storage.setStorageFile(file);
    QVERIFY(!storage.initialize(true).isError());

    QVERIFY(putTransactions(file, ownAccount, 3000, QStringLiteral("Buchung")));
    QVERIFY(putTransaction(file,
                           ownAccount,
                           QStringLiteral("Rückzahlung Möbelkauf"),
                           QStringLiteral("Erika Musterfrau"),
                           QDate(2026, 2, 17),
                           42.5));

    QSignalSpy itemsSpy(&storage, &Storage::itemsReceived);
    QSignalSpy countSpy(&storage, &Storage::itemsCounted);

    QVERIFY(!storage
                 .receiveItems({.type = Storage::StorageTransaction,
                                .accountId = ownAccount,
                                .text = QStringLiteral("Möbelkauf")})
                 .isError());

    // The calling thread has not processed an event since the call.
    QCOMPARE(itemsSpy.count(), 0);

    QVERIFY(itemsSpy.wait(workerTimeout));

    const auto items = qvariant_cast<BankingItems>(itemsSpy.takeFirst().at(0));
    QCOMPARE(items.size(), 1);

    const auto transaction = std::dynamic_pointer_cast<Transaction>(items.at(0));
    QVERIFY(transaction != nullptr);
    QCOMPARE(transaction->purpose(), QStringLiteral("Rückzahlung Möbelkauf"));

    // The count stands under the same condition as the read.
    QCOMPARE(countSpy.count(), 1);
    QCOMPARE(countSpy.takeFirst().at(0).toInt(), 1);

    storage.close();
}

/**
 * Both bounds of the period are inclusive, and nothing outside gets through. An
 * invalid date leaves its side open rather than standing for a date of its own.
 */
void StorageTest::aPeriodLetsNoTransactionOutsideItThrough()
{
    const auto file = storageFile();

    constexpr quint32 ownAccount = 815;

    Storage storage(applicationInfo());

    QVERIFY(!storage.setKey(password()).isError());
    storage.setStorageFile(file);
    QVERIFY(!storage.initialize(true).isError());

    const auto name = QStringLiteral("Erika Musterfrau");

    QVERIFY(putTransaction(file, ownAccount, QStringLiteral("Januar"), name, QDate(2026, 1, 31), 1));
    QVERIFY(putTransaction(file, ownAccount, QStringLiteral("Februar"), name, QDate(2026, 2, 1), 1));
    QVERIFY(putTransaction(file, ownAccount, QStringLiteral("Ende"), name, QDate(2026, 2, 28), 1));
    QVERIFY(putTransaction(file, ownAccount, QStringLiteral("März"), name, QDate(2026, 3, 1), 1));

    QSignalSpy itemsSpy(&storage, &Storage::itemsReceived);

    QVERIFY(!storage
                 .receiveItems({.type = Storage::StorageTransaction,
                                .accountId = ownAccount,
                                .from = QDate(2026, 2, 1),
                                .to = QDate(2026, 2, 28)})
                 .isError());

    QVERIFY(itemsSpy.wait(workerTimeout));

    const auto items = qvariant_cast<BankingItems>(itemsSpy.takeFirst().at(0));
    QCOMPARE(items.size(), 2);

    for (const auto &item : std::as_const(items)) {
        const auto transaction = std::dynamic_pointer_cast<Transaction>(item);
        QVERIFY(transaction != nullptr);
        QVERIFY(transaction->date() >= QDate(2026, 2, 1));
        QVERIFY(transaction->date() <= QDate(2026, 2, 28));
    }

    storage.close();
}

/**
 * The sign of the value is what tells the direction. A booking of nought is
 * neither of the two, and neither restriction lets it through.
 */
void StorageTest::aRestrictionToIncomingLetsNoDebitThrough()
{
    const auto file = storageFile();

    constexpr quint32 ownAccount = 815;

    Storage storage(applicationInfo());

    QVERIFY(!storage.setKey(password()).isError());
    storage.setStorageFile(file);
    QVERIFY(!storage.initialize(true).isError());

    const auto name = QStringLiteral("Erika Musterfrau");
    const auto date = QDate(2026, 2, 17);

    QVERIFY(putTransaction(file, ownAccount, QStringLiteral("Gehalt"), name, date, 2500.0));
    QVERIFY(putTransaction(file, ownAccount, QStringLiteral("Miete"), name, date, -750.0));
    QVERIFY(putTransaction(file, ownAccount, QStringLiteral("Nullbuchung"), name, date, 0.0));

    QSignalSpy itemsSpy(&storage, &Storage::itemsReceived);

    QVERIFY(!storage
                 .receiveItems({.type = Storage::StorageTransaction,
                                .accountId = ownAccount,
                                .direction = Storage::Direction::Incoming})
                 .isError());

    QVERIFY(itemsSpy.wait(workerTimeout));

    const auto items = qvariant_cast<BankingItems>(itemsSpy.takeFirst().at(0));
    QCOMPARE(items.size(), 1);

    const auto transaction = std::dynamic_pointer_cast<Transaction>(items.at(0));
    QVERIFY(transaction != nullptr);
    QCOMPARE(transaction->purpose(), QStringLiteral("Gehalt"));

    storage.close();
}

/**
 * A percent sign the user types is a character to him. Bound into a LIKE it
 * would be a placeholder for anything, and the filter would answer with the
 * whole holding instead of the one booking that carries it.
 */
void StorageTest::aPercentSignInTheSearchTextIsLookedForAsACharacter()
{
    const auto file = storageFile();

    constexpr quint32 ownAccount = 815;

    Storage storage(applicationInfo());

    QVERIFY(!storage.setKey(password()).isError());
    storage.setStorageFile(file);
    QVERIFY(!storage.initialize(true).isError());

    const auto name = QStringLiteral("Erika Musterfrau");
    const auto date = QDate(2026, 2, 17);

    QVERIFY(putTransaction(file, ownAccount, QStringLiteral("Zinsen 3% p.a."), name, date, 12.5));

    // Without the escaping the pattern would read as "a 3 followed by anything",
    // and this one would come back with it.
    QVERIFY(putTransaction(file, ownAccount, QStringLiteral("Rechnung 302"), name, date, -30.0));
    QVERIFY(putTransaction(file, ownAccount, QStringLiteral("Gehalt Februar"), name, date, 2500.0));

    QSignalSpy itemsSpy(&storage, &Storage::itemsReceived);

    QVERIFY(!storage
                 .receiveItems({.type = Storage::StorageTransaction,
                                .accountId = ownAccount,
                                .text = QStringLiteral("3%")})
                 .isError());

    QVERIFY(itemsSpy.wait(workerTimeout));

    const auto items = qvariant_cast<BankingItems>(itemsSpy.takeFirst().at(0));
    QCOMPARE(items.size(), 1);

    const auto transaction = std::dynamic_pointer_cast<Transaction>(items.at(0));
    QVERIFY(transaction != nullptr);
    QVERIFY(transaction->purpose().startsWith(QStringLiteral("Zinsen")));

    storage.close();
}

/**
 * Closing does not stop the worker, and its result belongs to a file nobody has
 * open any more. Handed on, the accounts of the storage that was closed would
 * appear under the name of the one opened next, and the user would take a
 * foreign holding for his own.
 *
 * The completion is still reported. A caller waiting for it would otherwise wait
 * for a run that is over.
 */
void StorageTest::closingWhileAReadIsGoingDropsItsResult()
{
    Storage storage(applicationInfo());

    QVERIFY(!storage.setKey(password()).isError());
    storage.setStorageFile(storageFile());
    QVERIFY(!storage.initialize(true).isError());

    for (int i = 0; i < 5; ++i) {
        const auto account = TestHelpers::createFakeAccount();
        QVERIFY(!storage.storeItem(account.get()).isError());
    }

    QSignalSpy itemsSpy(&storage, &Storage::itemsReceived);
    QSignalSpy finishedSpy(&storage, &Storage::readFinished);

    QVERIFY(!storage.receiveItems({.type = Storage::StorageAccount}).isError());

    // Nothing has been delivered yet, this thread has not processed an event
    // since the run started.
    QCOMPARE(itemsSpy.count(), 0);

    storage.close();

    QVERIFY(finishedSpy.wait(workerTimeout));
    QCOMPARE(itemsSpy.count(), 0);
}

/**
 * The same for the write. What it wrote is in the file it wrote to, and that one
 * is not the file that is open afterwards: a count reported here is read as the
 * outcome of the storage that stands, and whoever refreshes on it asks a
 * connection that is gone.
 *
 * The end is reported either way, so that nobody waits for a run that is over.
 */
void StorageTest::closingWhileAWriteIsGoingDropsItsCount()
{
    Storage storage(applicationInfo());

    QVERIFY(!storage.setKey(password()).isError());
    storage.setStorageFile(storageFile());
    QVERIFY(!storage.initialize(true).isError());

    auto accounts = BankingItems();
    for (int i = 0; i < 5; ++i) {
        accounts << TestHelpers::createFakeAccount();
    }

    QSignalSpy storedSpy(&storage, &Storage::itemsStored);
    QSignalSpy finishedSpy(&storage, &Storage::writeFinished);

    QVERIFY(!storage.storeItems(accounts).isError());

    // Nothing has been reported yet, this thread has not processed an event
    // since the run started.
    QCOMPARE(storedSpy.count(), 0);

    storage.close();

    QVERIFY(finishedSpy.wait(workerTimeout));
    QCOMPARE(storedSpy.count(), 0);
}

/**
 * What the write owes its caller: the thread that started it goes on. The wizard
 * hands over the accounts of a whole institution at once, and each of them costs
 * three tables and a transaction.
 */
void StorageTest::storeItemsReturnsBeforeTheAccountsAreWritten()
{
    Storage storage(applicationInfo());

    QVERIFY(!storage.setKey(password()).isError());
    storage.setStorageFile(storageFile());
    QVERIFY(!storage.initialize(true).isError());

    auto accounts = BankingItems();
    for (int i = 0; i < 5; ++i) {
        accounts << TestHelpers::createFakeAccount();
    }

    QSignalSpy storedSpy(&storage, &Storage::itemsStored);
    QSignalSpy finishedSpy(&storage, &Storage::writeFinished);

    QVERIFY(!storage.storeItems(accounts).isError());

    // Straight after the call. No event has been processed yet, so nothing can
    // have been delivered even if the worker were already done.
    QCOMPARE(storedSpy.count(), 0);
    QCOMPARE(finishedSpy.count(), 0);

    QVERIFY(storedSpy.wait(workerTimeout));
    QCOMPARE(storedSpy.count(), 1);
    QCOMPARE(storedSpy.takeFirst().at(0).toInt(), 5);

    QCOMPARE(scalarOf(storageFile(), QStringLiteral("SELECT COUNT(*) FROM accounts;")).toInt(), 5);

    storage.close();
}

/**
 * The signals belong to the thread that called, not to the one that wrote. A
 * receiver connected to them puts a message on the screen, which is only allowed
 * there. Whoever waits for finished has to be able to assume that the count has
 * already arrived.
 */
void StorageTest::storeItemsSignalsArriveInOrderAndInTheCallingThread()
{
    Storage storage(applicationInfo());

    QVERIFY(!storage.setKey(password()).isError());
    storage.setStorageFile(storageFile());
    QVERIFY(!storage.initialize(true).isError());

    auto accounts = BankingItems();
    for (int i = 0; i < 3; ++i) {
        accounts << TestHelpers::createFakeAccount();
    }

    QThread *const callingThread = QThread::currentThread();
    QStringList order;
    QList<QThread *> threads;

    connect(&storage, &Storage::writeProgressChanged, &storage, [&](int) {
        if (order.isEmpty() || order.last() != QStringLiteral("progress")) {
            order << QStringLiteral("progress");
        }
        threads << QThread::currentThread();
    });

    connect(&storage, &Storage::itemsStored, &storage, [&](int) {
        order << QStringLiteral("stored");
        threads << QThread::currentThread();
    });

    QSignalSpy finishedSpy(&storage, &Storage::writeFinished);
    connect(&storage, &Storage::writeFinished, &storage, [&]() {
        order << QStringLiteral("finished");
        threads << QThread::currentThread();
    });

    QVERIFY(!storage.storeItems(accounts).isError());

    QVERIFY(finishedSpy.wait(workerTimeout));

    QCOMPARE(order,
             QStringList{} << QStringLiteral("progress") << QStringLiteral("stored")
                           << QStringLiteral("finished"));

    QVERIFY(!threads.isEmpty());
    for (QThread *const thread : std::as_const(threads)) {
        QCOMPARE(thread, callingThread);
    }

    storage.close();
}

/**
 * Two writers on one storage would be two transactions on one file, and the
 * watcher of the first would be lost. Refused with an error, the way a second
 * read is.
 */
void StorageTest::storeItemsRefusesASecondRunWhileOneIsGoing()
{
    Storage storage(applicationInfo());

    QVERIFY(!storage.setKey(password()).isError());
    storage.setStorageFile(storageFile());
    QVERIFY(!storage.initialize(true).isError());

    auto accounts = BankingItems();
    for (int i = 0; i < 5; ++i) {
        accounts << TestHelpers::createFakeAccount();
    }

    QSignalSpy errorSpy(&storage, &Storage::writeFailed);
    QSignalSpy finishedSpy(&storage, &Storage::writeFinished);
    QSignalSpy storedSpy(&storage, &Storage::itemsStored);

    QVERIFY(!storage.storeItems(accounts).isError());

    // The first run is still going, this thread has not processed an event since
    // it started. The refusal comes back in this thread, before any waiting.
    const auto refused = storage.storeItems(accounts);

    QVERIFY(refused.isError());
    QCOMPARE(refused.code(), ErrorCode::Busy);

    // The counterpart of the read: the refusal reaches its caller, and no
    // receiver of the run that is still going hears a thing.
    QCOMPARE(errorSpy.count(), 0);
    QCOMPARE(finishedSpy.count(), 0);
    QCOMPARE(storedSpy.count(), 0);

    // The first run is unaffected and still delivers.
    QVERIFY(storedSpy.wait(workerTimeout));
    QCOMPARE(storedSpy.takeFirst().at(0).toInt(), 5);
    QCOMPARE(finishedSpy.count(), 1);

    storage.close();
}

/**
 * A write runs on a second connection of its own and unregisters it at the end.
 * Twenty runs therefore leave as many connections behind as none do. The read
 * path is held by the same kind of check.
 */
void StorageTest::manyWritesLeaveNoConnectionBehind()
{
    Storage storage(applicationInfo());

    QVERIFY(!storage.setKey(password()).isError());
    storage.setStorageFile(storageFile());
    QVERIFY(!storage.initialize(true).isError());

    const auto connectionsWithoutAWrite = QSqlDatabase::connectionNames().size();

    for (int i = 0; i < 20; ++i) {
        QSignalSpy storedSpy(&storage, &Storage::itemsStored);

        auto accounts = BankingItems();
        accounts << TestHelpers::createFakeAccount();

        QVERIFY(!storage.storeItems(accounts).isError());
        QVERIFY(storedSpy.wait(workerTimeout));
    }

    QCOMPARE(QSqlDatabase::connectionNames().size(), connectionsWithoutAWrite);

    storage.close();
}

/**
 * The two runs hang on watchers of their own and do not lock against each other,
 * so either of them may end while the other is still going. Each therefore ends
 * with a signal of its own.
 *
 * Both used to end in one parameterless finished(), and the three receivers in
 * the application took whichever arrived for their own. A user scrolling the
 * booking table while a fetch was writing ended the write phase of that fetch,
 * and he read that his fetch could not be stored while its bookings lay in the
 * file.
 */
void StorageTest::aReadAndAWriteEachReportOnlyTheirOwnEnd()
{
    Storage storage(applicationInfo());

    QVERIFY(!storage.setKey(password()).isError());
    storage.setStorageFile(storageFile());
    QVERIFY(!storage.initialize(true).isError());

    const auto account = TestHelpers::createFakeAccount();
    QVERIFY(!storage.storeItem(account.get()).isError());

    QSignalSpy readFinishedSpy(&storage, &Storage::readFinished);
    QSignalSpy writeFinishedSpy(&storage, &Storage::writeFinished);

    QVERIFY(!storage.receiveItems({.type = Storage::StorageAccount}).isError());

    QVERIFY(readFinishedSpy.wait(workerTimeout));
    QCOMPARE(readFinishedSpy.count(), 1);

    // The read said nothing about a write. Nobody waiting for one was answered.
    QCOMPARE(writeFinishedSpy.count(), 0);

    QVERIFY(!storage.storeItems(BankingItems{TestHelpers::createFakeAccount()}).isError());

    QVERIFY(writeFinishedSpy.wait(workerTimeout));
    QCOMPARE(writeFinishedSpy.count(), 1);

    // And the write said nothing about a read.
    QCOMPARE(readFinishedSpy.count(), 1);

    storage.close();
}

/**
 * A caller connects after the call, because a call that starts no run reports
 * through its return value alone. The run of nought records is the one that
 * starts nothing and reports anyway, so its two signals have to wait for the
 * event loop like those of every other run.
 */
void StorageTest::aRunWithoutRecordsReportsToWhoeverListensAfterTheCall()
{
    Storage storage(applicationInfo());

    QVERIFY(!storage.setKey(password()).isError());
    storage.setStorageFile(storageFile());
    QVERIFY(!storage.initialize(true).isError());

    QVERIFY(!storage.storeItems({}).isError());

    QSignalSpy storedSpy(&storage, &Storage::itemsStored);
    QSignalSpy finishedSpy(&storage, &Storage::writeFinished);

    QVERIFY(finishedSpy.wait(workerTimeout));

    QCOMPARE(storedSpy.count(), 1);
    QCOMPARE(storedSpy.takeFirst().at(0).toInt(), 0);
    QCOMPARE(finishedSpy.count(), 1);

    storage.close();
}

/**
 * The cheap answer, so that a caller reaching the storage after it was closed
 * can tell that apart from a file worth warning the user about.
 */
void StorageTest::anOpenStorageSaysSoAndAClosedOneDoesNot()
{
    Storage storage(applicationInfo());

    QVERIFY(!storage.isOpen());

    QVERIFY(!storage.setKey(password()).isError());
    storage.setStorageFile(storageFile());
    QVERIFY(!storage.initialize(true).isError());

    QVERIFY(storage.isOpen());

    storage.close();

    QVERIFY(!storage.isOpen());
}

} // namespace olbaflinx::core::storage::tests

QTEST_MAIN(olbaflinx::core::storage::tests::StorageTest)

#include "tst_storage.moc"
