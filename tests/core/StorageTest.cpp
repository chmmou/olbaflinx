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

#include "Storage/Storage.h"
#include "SingleApplication/SingleApplication.h"

#include <QtCore/QDir>
#include <QtCore/QList>
#include <QtTest/QtTest>

using namespace olbaflinx::core;
using namespace olbaflinx::core::storage;

namespace olbaflinx::core::storage::tests
{

class StorageTest: public QObject
{

Q_OBJECT
public:
    StorageTest();
    ~StorageTest() override;

private:
    QList<int> metaTypeIds;
    QString storageFile;
    QString storagePassword;

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void testInitializingWithoutUser();
    void testInitializingWithUserNoStorageFile();
    void testInitializingWithUserNoPassword();
    void testInitializingWithUser();
    void testInitializing();
    void testChangePassword();
    void testStoreSettingWithEmptyStorageFilePath();
    void testStoreSettingWithStorageFilePath();
};

StorageTest::StorageTest()
    : metaTypeIds(QList<int>()),
      storageFile(QDir::tempPath().append("/olbaflinx_test.storage")),
      storagePassword("M'yF13\"stP\\$44W0$3d/")
{
    SingleApplication::setApplicationName("OlbaFlinx");
    SingleApplication::setApplicationVersion("1.0.0");
    SingleApplication::setOrganizationName("de.chm-projects.olbaflinx");
    SingleApplication::setOrganizationDomain("https://olbaflinx.chm-projects.de");
}

StorageTest::~StorageTest() = default;

void StorageTest::initTestCase()
{
    metaTypeIds << qRegisterMetaType<Storage::ErrorType>("Storage::ErrorType");
}

void StorageTest::cleanupTestCase()
{
    for (const int id: metaTypeIds) {
        QMetaType::unregisterType(id);
    }
    metaTypeIds.clear();

    if (QFile::exists(storageFile)) {
        QFile::remove(storageFile);
    }

    storageFile.clear();
    storagePassword.clear();
}

void StorageTest::testInitializingWithoutUser()
{
    auto storage = Storage::instance();
    QSignalSpy spy(storage, &Storage::errorOccurred);
    storage->setUser(nullptr);
    QCOMPARE(spy.count(), 1);
}

void StorageTest::testInitializingWithUserNoStorageFile()
{
    auto storage = Storage::instance();
    QSignalSpy spy(storage, &Storage::errorOccurred);
    StorageUser user;
    user.setFilePath("");
    storage->setUser(&user);
    QCOMPARE(spy.count(), 1);

    QList<QVariant> arguments = spy.takeFirst();
    auto type = qvariant_cast<Storage::ErrorType>(arguments.at(1));
    QVERIFY(type == Storage::FileNotFoundError);
}

void StorageTest::testInitializingWithUserNoPassword()
{
    auto storage = Storage::instance();
    QSignalSpy spy(storage, &Storage::errorOccurred);
    StorageUser user;
    user.setFilePath(storageFile);
    user.setPassword("");
    storage->setUser(&user);
    QCOMPARE(spy.count(), 1);

    QList<QVariant> arguments = spy.takeFirst();
    auto type = qvariant_cast<Storage::ErrorType>(arguments.at(1));
    QVERIFY(type == Storage::PasswordError);
}

void StorageTest::testInitializingWithUser()
{
    auto storage = Storage::instance();
    QSignalSpy spy(storage, &Storage::errorOccurred);
    StorageUser user;
    user.setFilePath(storageFile);
    user.setPassword(storagePassword);
    storage->setUser(&user);

    QCOMPARE(spy.count(), 0);
}

void StorageTest::testInitializing()
{
    auto storage = Storage::instance();
    QSignalSpy spy(storage, &Storage::errorOccurred);

    StorageUser user;
    user.setFilePath(storageFile);
    user.setPassword(storagePassword);
    storage->setUser(&user);

    bool initialized = storage->initialize();
    QVERIFY(initialized);

    initialized = storage->isInitialized();
    QVERIFY(initialized);

    QCOMPARE(spy.count(), 0);
}

void StorageTest::testChangePassword()
{
    auto storage = Storage::instance();
    QSignalSpy spy(storage, &Storage::errorOccurred);

    StorageUser user;
    user.setFilePath(storageFile);
    user.setPassword(storagePassword);
    storage->setUser(&user);

    bool initialized = storage->changePassword(
        storagePassword,
        "eve3yth1ng h4$ 4n end only the s4u$a4ge h4$ 2"
    );
    QVERIFY(initialized);

    QCOMPARE(spy.count(), 0);
}

void StorageTest::testStoreSettingWithEmptyStorageFilePath()
{
    auto storage = Storage::instance();
    storage->storeSetting("Vaults", QStringList(), "Storage");

    auto vaults = storage->setting(
        "Vaults",
        "Storage",
        QStringList()
    ).toStringList();

    QCOMPARE(vaults.size(), 0);
}

void StorageTest::testStoreSettingWithStorageFilePath()
{
    auto storage = Storage::instance();

    QStringList vaults;
    vaults << "/tmp/test1" << "/tmp/test2";
    storage->storeSetting("Vaults", vaults, "Storage");

    vaults = storage->setting(
        "Vaults",
        "Storage",
        QStringList()
    ).toStringList();

    QCOMPARE(vaults.size(), 2);
    QCOMPARE(vaults.at(0), "/tmp/test1");
    QCOMPARE(vaults.at(1), "/tmp/test2");
}

}

QTEST_MAIN(tests::StorageTest)

#include "StorageTest.moc"
