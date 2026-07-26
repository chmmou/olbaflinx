/**
 * Copyright (C) 2021-2025, Alexander Saal <developer@olbaflinx.chm-projects.de>
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

#include "core/Banking/Account/Account.h"

#include "core/Storage/Storage.h"

#include "BaseTest.h"

#include <QtCore/QList>
#include <QtTest/QtTest>

using namespace olbaflinx::core;
using namespace olbaflinx::core::storage;

using namespace olbaflinx::core::banking;
using namespace olbaflinx::core::banking::account;

namespace olbaflinx::core::storage::tests {

using namespace olbaflinx::core::tests;

class StorageTest final : public QObject
{
    Q_OBJECT

public:
    StorageTest();
    ~StorageTest() override;

private:
    QString storageFile;
    QString storagePassword;

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void testInitializingWithoutData();
    void testInitializingWithNoStorageFile();
    void testInitializingWithNoPassword();
    void testInitializing();
    void testChangePassword();
    void testStoreSettingWithEmptyStorageFilePath();
    void testStoreSettingWithStorageFilePath();

    void testStoreItems();
};

StorageTest::StorageTest()
    : storageFile(QDir::tempPath().append("/olbaflinx_test.obfx"))
    , storagePassword("M'yF13\"stP\\$44W0$3d/")
{
    QCoreApplication::setApplicationName("OlbaFlinx");
    QCoreApplication::setApplicationVersion("1.0.0");
    QCoreApplication::setOrganizationName("de.chm-projects.olbaflinx.test");
    QCoreApplication::setOrganizationDomain("https://olbaflinx.chm-projects.de");
}

StorageTest::~StorageTest() = default;

void StorageTest::initTestCase()
{
    if (QFile::exists(storageFile)) {
        QFile::remove(storageFile);
    }
}

void StorageTest::cleanupTestCase()
{
    Storage::instance()->close();

    if (QFile::exists(storageFile)) {
        QFile::remove(storageFile);
    }

    storageFile.clear();
    storagePassword.clear();
}

void StorageTest::testInitializingWithoutData()
{
    auto storage = Storage::instance();
    storage->close();
    QVERIFY(!storage->isValid());
}

void StorageTest::testInitializingWithNoStorageFile()
{
    auto storage = Storage::instance();
    storage->setKey(storagePassword);
    storage->setStorageFile("");
    (void) storage->initialize(true);

    QVERIFY(!storage->isValid());
    storage->close();
}

void StorageTest::testInitializingWithNoPassword()
{
    auto tmpStorage = QDir::tempPath().append("/testInitializingWithNoPassword.obfx");
    auto storage = Storage::instance();

    storage->setKey("");
    storage->setStorageFile(tmpStorage);
    (void) storage->initialize(true);

    QVERIFY(!storage->isValid());
    storage->close();

    bool removed = QFile(tmpStorage).remove();
    QVERIFY(removed);
}

void StorageTest::testInitializing()
{
    const auto tmpStorage = QDir::tempPath().append("/testInitializing.obfx");
    const auto storage = Storage::instance();

    storage->setKey(storagePassword);
    storage->setStorageFile(tmpStorage);

    (void) storage->initialize(true);

    QVERIFY(storage->isValid());
    storage->close();

    const bool removed = QFile(tmpStorage).remove();
    QVERIFY(removed);
}

void StorageTest::testChangePassword()
{
    auto tmpStorage = QDir::tempPath().append("/testChangePassword.obfx");
    auto storage = Storage::instance();
    storage->setKey(storagePassword);
    storage->setStorageFile(tmpStorage);

    (void) storage->initialize(true);

    bool changed = storage->changeKey(storagePassword,
                                      "eve3yth1ng h4$ 4n end only the s4u$a4ge h4$ 2");
    QVERIFY(changed);
    storage->close();

    storage->setKey(storagePassword);
    storage->setStorageFile(tmpStorage);

    (void) storage->initialize(true);

    QVERIFY(!storage->isValid());

    storage->setKey("eve3yth1ng h4$ 4n end only the s4u$a4ge h4$ 2");
    storage->setStorageFile(tmpStorage);

    (void) storage->initialize(true);
    QVERIFY(storage->isValid());
    storage->close();

    bool removed = QFile(tmpStorage).remove();
    QVERIFY(removed);
}

void StorageTest::testStoreSettingWithEmptyStorageFilePath()
{
    auto storage = Storage::instance();
    storage->storeSetting("Paths", QStringList(), "Vaults");

    auto vaults = storage->setting("Paths", "Vaults", QStringList()).toStringList();

    QCOMPARE(vaults.size(), 0);
}

void StorageTest::testStoreSettingWithStorageFilePath()
{
    auto storage = Storage::instance();

    QStringList vaults;
    vaults << "/tmp/test1" << "/tmp/test2";

    storage->storeSetting("Paths", vaults, "Vaults");

    vaults = storage->setting("Paths", "Vaults", QStringList()).toStringList();

    QCOMPARE(vaults.size(), 2);
    QCOMPARE(vaults.at(0), "/tmp/test1");
    QCOMPARE(vaults.at(1), "/tmp/test2");
}

void StorageTest::testStoreItems()
{
    auto tmpStorage = QDir::tempPath().append("/testAccount.obfx");

    QFile(tmpStorage).remove();
    //QVERIFY(removed);

    auto storage = Storage::instance();

    QSignalSpy spyItem(storage, &Storage::itemsReceived);
    QSignalSpy spyFinished(storage, &Storage::finished);

    storage->setKey(storagePassword);
    storage->setStorageFile(tmpStorage);

    (void) storage->initialize(true);
    QVERIFY(storage->isValid());

    const auto account1 = BaseTest::createFakeAccount();
    QVERIFY(account1->isValid());

    const auto account2 = BaseTest::createFakeAccount();
    QVERIFY(account2->isValid());

    (void) storage->storeItem(account1);
    (void) storage->storeItem(account2);

    storage->receiveItems(Storage::StorageAccount);
    // Make sure the signal was emitted exactly one time
    QCOMPARE(spyItem.count(), 1);

    // Make sure the signal was emitted exactly three time because of store and retrieve items
    QCOMPARE(spyFinished.count(), 3);

    auto arguments = spyItem.takeFirst(); // take the first signal
    auto list = qvariant_cast<QList<BankingItem *>>(arguments[0]);
    QCOMPARE(list.size(), 2);

    storage->close();

    storage->deleteLater();

    delete account1;
    delete account2;
}

} // namespace olbaflinx::core::storage::tests

QTEST_MAIN(storage::tests::StorageTest)

#include "StorageTest.moc"
