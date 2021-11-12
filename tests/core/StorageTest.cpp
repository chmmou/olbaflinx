/**
 * Copyright (c) 2021, Alexander Saal <alexander.saal@chm-projects.de>
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
    void testStorageInitializingWithoutUser();
    void testStorageInitializingWithUserNoStorageFile();
    void testStorageInitializingWithUserNoPassword();
    void testStorageInitializingWithUser();
    void testStorageInitializing();
    void testStorageChangePassword();
};

StorageTest::StorageTest()
    : metaTypeIds(QList<int>()),
      storageFile(QDir::tempPath().append("/olbaflinx_test.storage")),
      storagePassword("M'yF13\"stP\\$44W0$3d/")
{
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

void StorageTest::testStorageInitializingWithoutUser()
{
    auto storage = Storage::instance();
    QSignalSpy spy(storage, &Storage::errorOccurred);
    storage->setUser(nullptr);
    QCOMPARE(spy.count(), 1);
}

void StorageTest::testStorageInitializingWithUserNoStorageFile()
{
    auto storage = Storage::instance();
    QSignalSpy spy(storage, &Storage::errorOccurred);
    StorageUser user;
    user.filePath = "";
    storage->setUser(&user);
    QCOMPARE(spy.count(), 1);

    QList<QVariant> arguments = spy.takeFirst();
    auto type = qvariant_cast<Storage::ErrorType>(arguments.at(1));
    QVERIFY(type == Storage::FileNotFoundError);
}

void StorageTest::testStorageInitializingWithUserNoPassword()
{
    auto storage = Storage::instance();
    QSignalSpy spy(storage, &Storage::errorOccurred);
    StorageUser user;
    user.filePath = storageFile;
    user.password = "";
    storage->setUser(&user);
    QCOMPARE(spy.count(), 1);

    QList<QVariant> arguments = spy.takeFirst();
    auto type = qvariant_cast<Storage::ErrorType>(arguments.at(1));
    QVERIFY(type == Storage::PasswordError);
}

void StorageTest::testStorageInitializingWithUser()
{
    auto storage = Storage::instance();
    QSignalSpy spy(storage, &Storage::errorOccurred);
    StorageUser user;
    user.filePath = storageFile;
    user.password = storagePassword;
    storage->setUser(&user);

    QCOMPARE(spy.count(), 0);
}

void StorageTest::testStorageInitializing()
{
    auto storage = Storage::instance();
    QSignalSpy spy(storage, &Storage::errorOccurred);

    StorageUser user;
    user.filePath = storageFile;
    user.password = storagePassword;
    storage->setUser(&user);

    bool initialized = storage->initialize();
    QVERIFY(initialized);

    initialized = storage->isInitialized();
    QVERIFY(initialized);

    QCOMPARE(spy.count(), 0);
}

void StorageTest::testStorageChangePassword()
{
    auto storage = Storage::instance();
    QSignalSpy spy(storage, &Storage::errorOccurred);

    StorageUser user;
    user.filePath = storageFile;
    user.password = storagePassword;
    storage->setUser(&user);

    bool initialized = storage->changePassword(
        storagePassword,
        "eve3yth1ng h4$ 4n end only the s4u$a4ge h4$ 2"
    );
    QVERIFY(initialized);

    QCOMPARE(spy.count(), 0);
}

}

QTEST_MAIN(tests::StorageTest)

#include "StorageTest.moc"