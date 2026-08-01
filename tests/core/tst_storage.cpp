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

private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void isValidReturnsFalseWithoutInitialization();
    void initializeRejectsEmptyStorageFile();
    void initializeRejectsEmptyPassword();
    void initializeCreatesUsableStorage();
    void changeKeyMakesOldPasswordInvalid();
    void settingReturnsTheDefaultForAnUnknownKey();
    void storeSettingPersistsValueUnderGroup();
    void storeItemPersistsAccountAndEmitsFinished();
    void storeItemKeepsBalanceAndReferenceAccounts();
    void initializeRejectsAFileFromANewerVersion();
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

    qDeleteAll(referenceAccounts);

    storage.close();
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

} // namespace olbaflinx::core::storage::tests

QTEST_MAIN(olbaflinx::core::storage::tests::StorageTest)

#include "tst_storage.moc"
