/**
 * Copyright (C) 2021, Alexander Saal <developer@olbaflinx.chm-projects.de>
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
#include <QtCore/QDir>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtTest/QtTest>

#include "core/SingleApplication/SingleApplication.h"
#include "core/Storage/Account/Account.h"
#include "core/Storage/Account/AccountBalance.h"
#include "core/Storage/Transaction/Transaction.h"

#include "core/Storage/VaultStorage.h"

#include "BaseTest.h"

using namespace olbaflinx::core;
using namespace olbaflinx::core::storage;

namespace olbaflinx::core::storage::tests {

using namespace olbaflinx::core::tests;

class StorageTest : public QObject
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

    void testStoreAccount();
    void testStoreAccountNull();
    void testStoreAccountFailed();

    void testCreateAccountValid();
    void testCreateAccountInvalid();
};

StorageTest::StorageTest()
    : storageFile(QDir::tempPath().append("/olbaflinx_test.obfx"))
    , storagePassword("M'yF13\"stP\\$44W0$3d/")
{
    SingleApplication::setApplicationName("OlbaFlinx");
    SingleApplication::setApplicationVersion("1.0.0");
    SingleApplication::setOrganizationName("de.chm-projects.olbaflinx.test");
    SingleApplication::setOrganizationDomain("https://olbaflinx.chm-projects.de");
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
    VaultStorage::instance()->close();

    if (QFile::exists(storageFile)) {
        QFile::remove(storageFile);
    }

    storageFile.clear();
    storagePassword.clear();
}

void StorageTest::testInitializingWithoutData()
{
    auto storage = VaultStorage::instance();
    storage->close();
    QVERIFY(!storage->isStorageValid());
}

void StorageTest::testInitializingWithNoStorageFile()
{
    auto storage = VaultStorage::instance();
    storage->setDatabaseKey("", storagePassword);
    storage->initialize(true);
    QVERIFY(!storage->isStorageValid());
    storage->close();
}

void StorageTest::testInitializingWithNoPassword()
{
    auto tmpStorage = QDir::tempPath().append("/testInitializingWithNoPassword.obfx");
    auto storage = VaultStorage::instance();
    storage->setDatabaseKey(tmpStorage, "");
    storage->initialize(true);
    QVERIFY(!storage->isStorageValid());
    storage->close();

    bool removed = QFile(tmpStorage).remove();
    QVERIFY(removed);
}

void StorageTest::testInitializing()
{
    auto tmpStorage = QDir::tempPath().append("/testInitializing.obfx");
    auto storage = VaultStorage::instance();

    storage->setDatabaseKey(tmpStorage, storagePassword);
    storage->initialize(true);
    QVERIFY(storage->isStorageValid());
    storage->close();

    bool removed = QFile(tmpStorage).remove();
    QVERIFY(removed);
}

void StorageTest::testChangePassword()
{
    auto tmpStorage = QDir::tempPath().append("/testChangePassword.obfx");
    auto storage = VaultStorage::instance();
    storage->setDatabaseKey(tmpStorage, storagePassword);
    storage->initialize(true);

    bool changed = storage->changeKey(storagePassword,
                                      "eve3yth1ng h4$ 4n end only the s4u$a4ge h4$ 2");
    QVERIFY(changed);
    storage->close();

    storage->setDatabaseKey(tmpStorage, storagePassword);
    storage->initialize(true);
    QVERIFY(!storage->isStorageValid());

    storage->setDatabaseKey(tmpStorage, "eve3yth1ng h4$ 4n end only the s4u$a4ge h4$ 2");
    storage->initialize(true);
    QVERIFY(storage->isStorageValid());
    storage->close();

    bool removed = QFile(tmpStorage).remove();
    QVERIFY(removed);
}

void StorageTest::testStoreSettingWithEmptyStorageFilePath()
{
    auto storage = VaultStorage::instance();
    storage->storeSetting("Paths", QStringList(), "Vaults");

    auto vaults = storage->setting("Paths", "Vaults", QStringList()).toStringList();

    QCOMPARE(vaults.size(), 0);
}

void StorageTest::testStoreSettingWithStorageFilePath()
{
    auto storage = VaultStorage::instance();

    QStringList vaults;
    vaults << "/tmp/test1"
           << "/tmp/test2";
    storage->storeSetting("Paths", vaults, "Vaults");

    vaults = storage->setting("Paths", "Vaults", QStringList()).toStringList();

    QCOMPARE(vaults.size(), 2);
    QCOMPARE(vaults.at(0), "/tmp/test1");
    QCOMPARE(vaults.at(1), "/tmp/test2");
}

void StorageTest::testStoreAccount()
{
    auto tmpStorage = QDir::tempPath().append("/testAccount.obfx");
    auto storage = VaultStorage::instance();

    storage->setDatabaseKey(tmpStorage, storagePassword);
    storage->initialize(true);
    QVERIFY(storage->isStorageValid());

    const auto account = BaseTest::createFakeAccount();
    QVERIFY(account->isValid());

    storage->addAccount(account);
    auto accounts = storage->accounts();
    QCOMPARE(accounts.size(), 1);
    QCOMPARE(account->toString(), accounts.at(0)->toString());
    QVERIFY(accounts.at(0)->isValid());

    storage->close();

    qDeleteAll(accounts);
    accounts.clear();

    delete account;

    bool removed = QFile(tmpStorage).remove();
    QVERIFY(removed);
}

void StorageTest::testStoreAccountNull()
{
    auto tmpStorage = QDir::tempPath().append("/testAccount.obfx");
    auto storage = VaultStorage::instance();

    storage->setDatabaseKey(tmpStorage, "");
    storage->initialize(true);
    QVERIFY(!storage->isStorageValid());

    const auto account = Q_NULLPTR;
    storage->addAccount(account);
    const auto accounts = storage->accounts();
    QCOMPARE(accounts.size(), 0);

    storage->close();

    bool removed = QFile(tmpStorage).remove();
    QVERIFY(removed);
}

void StorageTest::testStoreAccountFailed()
{
    auto tmpStorage = QDir::tempPath().append("/testAccount.obfx");
    auto storage = VaultStorage::instance();

    storage->setDatabaseKey(tmpStorage, "");
    storage->initialize(true);
    QVERIFY(!storage->isStorageValid());

    const auto account = BaseTest::createFakeAccount();
    storage->addAccount(account);
    const auto accounts = storage->accounts();
    QCOMPARE(accounts.size(), 0);

    storage->close();

    delete account;

    bool removed = QFile(tmpStorage).remove();
    QVERIFY(removed);
}

void StorageTest::testCreateAccountValid()
{
    auto account = BaseTest::createFakeAccount();
    QVERIFY(account->isValid());
    delete account;
}

void StorageTest::testCreateAccountInvalid()
{
    auto account = BaseTest::createFakeAccount(AB_AccountType_Unspecified);
    QVERIFY(!account->isValid());
    delete account;
}

} // namespace olbaflinx::core::storage::tests

QTEST_MAIN(storage::tests::StorageTest)

#include "StorageTest.moc"
