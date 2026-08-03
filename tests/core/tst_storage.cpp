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
#include "core/Error.h"
#include "core/Storage/Storage.h"

#include "TestHelpers.h"

#include <QtCore/QList>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtTest/QtTest>

#include <memory>

using namespace olbaflinx::core;
using namespace olbaflinx::core::banking;
using namespace olbaflinx::core::banking::account;
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
        return {QStringLiteral("de.chm-projects.olbaflinx.test"),
                QStringLiteral("OlbaFlinxStorageTest"),
                QStringLiteral("1.0.0")};
    }

    static QString password() { return QStringLiteral("M'yF13\"stP\\$44W0$3d/"); }

    QString storageFile() const
    {
        return workingDirectory->filePath(QStringLiteral("storage.obfx"));
    }

    /**
     * Runs one statement against the store, past Storage, and hands back the
     * first value of the first row. An invalid QVariant means the file would not
     * open, the statement failed, or it returned no row.
     *
     * The tests use it for what Storage offers no way to ask: what a column
     * actually holds after a write, and how many rows a table carries.
     */
    static QVariant scalarOf(const QString &file, const QString &statement)
    {
        auto value = QVariant();

        {
            auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLCIPHER"),
                                                      QStringLiteral("StorageTestDirect"));
            database.setDatabaseName(file);

            if (database.open()) {
                auto key = password();
                key.replace(QLatin1Char('\''), QLatin1StringView("''"));

                QSqlQuery query(database);
                if (query.exec(QStringLiteral("PRAGMA key='%1';").arg(key)) && query.exec(statement)
                    && query.next()) {
                    value = query.value(0);
                }

                database.close();
            }
        }
        QSqlDatabase::removeDatabase(QStringLiteral("StorageTestDirect"));

        return value;
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
    void storeItemPersistsAccountAndEmitsFinished();
    void storeItemKeepsBalanceAndReferenceAccounts();
    void anAccountSurvivesAReopenWithEveryVisibleProperty();
    void storingTheSameAccountTwiceLeavesOneRow();
    void anUpdateOfTheBankDetailsLeavesTheStateAlone();
    void deselectingKeepsTheRowAndItsTransactions();
    void initializeRejectsAFileFromANewerVersion();
    void receiveItemsReturnsBeforeTheItemsArrive();
    void receiveItemsSignalsArriveInOrderAndInTheCallingThread();
    void receiveItemsRefusesASecondRunWhileOneIsGoing();
};

void StorageTest::initTestCase()
{
    // Keeps QSettings out of the real user configuration, see QStandardPaths docs.
    QStandardPaths::setTestModeEnabled(true);
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

    QCOMPARE(schemaVersionOf(file), 3);
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

void StorageTest::storeItemPersistsAccountAndEmitsFinished()
{
    Storage storage(applicationInfo());

    QSignalSpy itemsSpy(&storage, &Storage::itemsReceived);
    QSignalSpy finishedSpy(&storage, &Storage::finished);

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

    // Every completed write reports finished once, which is where two of the
    // three emissions come from.
    QCOMPARE(finishedSpy.count(), 2);

    storage.receiveItems(Storage::StorageAccount);

    QVERIFY(itemsSpy.wait());
    QCOMPARE(itemsSpy.count(), 1);
    QCOMPARE(finishedSpy.count(), 3);

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

    storage.receiveItems(Storage::StorageAccount);

    QVERIFY(itemsSpy.wait());
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
 * What US1 promises the user: the accounts chosen in the wizard are there again
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

    storage.receiveItems(Storage::StorageAccount);
    QVERIFY(itemsSpy.wait());

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
    // them. Epic 1 has no way to fetch any, so the test puts them there.
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
    QSignalSpy finishedSpy(&storage, &Storage::finished);

    const int finishedBefore = finishedSpy.count();

    storage.receiveItems(Storage::StorageAccount);

    // Straight after the call. No event has been processed yet, so nothing can
    // have been delivered even if the worker were already done.
    QCOMPARE(itemsSpy.count(), 0);
    QCOMPARE(finishedSpy.count(), finishedBefore);

    QVERIFY(itemsSpy.wait());
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

    connect(&storage, &Storage::progressChanged, &storage, [&](int) {
        if (order.isEmpty() || order.last() != QStringLiteral("progress")) {
            order << QStringLiteral("progress");
        }
        threads << QThread::currentThread();
    });

    connect(&storage, &Storage::itemsReceived, &storage, [&](const BankingItems &) {
        order << QStringLiteral("items");
        threads << QThread::currentThread();
    });

    QSignalSpy finishedSpy(&storage, &Storage::finished);
    connect(&storage, &Storage::finished, &storage, [&]() {
        order << QStringLiteral("finished");
        threads << QThread::currentThread();
    });

    storage.receiveItems(Storage::StorageAccount);

    QVERIFY(finishedSpy.wait());

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

    QSignalSpy errorSpy(&storage, &Storage::errorOccurred);
    QSignalSpy itemsSpy(&storage, &Storage::itemsReceived);

    storage.receiveItems(Storage::StorageAccount);

    // The first run is still going, this thread has not processed an event since
    // it started. The refusal comes back in this thread, before any waiting.
    storage.receiveItems(Storage::StorageAccount);

    QCOMPARE(errorSpy.count(), 1);
    QCOMPARE(errorSpy.takeFirst().at(0).value<ErrorCode>(), ErrorCode::InvalidInput);

    // The first run is unaffected and still delivers.
    QVERIFY(itemsSpy.wait());
    QCOMPARE(itemsSpy.count(), 1);

    storage.close();
}

} // namespace olbaflinx::core::storage::tests

QTEST_MAIN(olbaflinx::core::storage::tests::StorageTest)

#include "tst_storage.moc"
