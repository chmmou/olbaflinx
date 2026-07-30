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

#include "BaseTest.h"

#include <QtTest/QtTest>

#include <limits>

using namespace olbaflinx::core;
using namespace olbaflinx::core::banking;
using namespace olbaflinx::core::storage;

namespace olbaflinx::core::storage::tests {

using namespace olbaflinx::core::tests;

/**
 * The key as it reaches SQLCipher. The pass phrase used to run through an escape
 * routine that read every character through QChar::toLatin1, which answers with a
 * signed char here. Its range check for the upper half of Latin-1 could never be
 * true, so every character outside 32 to 126 was dropped from the key silently.
 * The store still opened, because the same loss happened on every open.
 */
class StorageKeyTest final : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir workingDirectory;

    static ApplicationInfo applicationInfo()
    {
        return {QStringLiteral("de.chm-projects.olbaflinx.test"),
                QStringLiteral("OlbaFlinxStorageKeyTest"),
                QStringLiteral("1.0.0")};
    }

    QString storageFile(const QString &name) const
    {
        return workingDirectory.filePath(name + QStringLiteral(".obfx"));
    }

    /**
     * Creates a store under the given key and closes it again.
     */
    void createStorage(const QString &file, const QString &key)
    {
        Storage storage(applicationInfo());
        storage.setKey(key);
        storage.setStorageFile(file);

        QVERIFY(!storage.initialize(true).isError());
        QVERIFY(storage.isValid());

        storage.close();
    }

    /**
     * Whether the given key opens the store. Applying the schema is what fails on
     * a wrong key, the PRAGMA itself always succeeds.
     */
    static bool opens(const QString &file, const QString &key)
    {
        Storage storage(applicationInfo());
        storage.setKey(key);
        storage.setStorageFile(file);

        const bool opened = !storage.initialize(true).isError() && storage.isValid();
        storage.close();

        return opened;
    }

private Q_SLOTS:
    void initTestCase();

    void passwordWithUmlautsUnlocksStorage();
    void passwordWithQuoteUnlocksStorage();
    void passwordWithCjkUnlocksStorage();
    void truncatedPasswordDoesNotUnlockStorage();
    void wrongPasswordIsRejected();
    void storageFileIsNotPlaintextSqlite();
    void changeKeyPreservesData();
    void changeKeyLeavesOldKeyInvalid();
    void receiveItemsRejectsNegativeOffset();
    void receiveItemsRejectsExcessiveLimit();
};

void StorageKeyTest::initTestCase()
{
    // Keeps QSettings out of the real user configuration, see QStandardPaths docs.
    QStandardPaths::setTestModeEnabled(true);

    QVERIFY(workingDirectory.isValid());
}

void StorageKeyTest::passwordWithUmlautsUnlocksStorage()
{
    const auto file = storageFile("umlauts");
    const auto password = QStringLiteral("Paßwort-Ümlaut-2026");

    createStorage(file, password);

    QVERIFY(opens(file, password));
}

/**
 * The single quote is the one character that could end the string literal the key
 * travels in. It is doubled, the double quote and the backslash carry no meaning
 * there and stay as they are.
 */
void StorageKeyTest::passwordWithQuoteUnlocksStorage()
{
    const auto file = storageFile("quotes");
    const auto password = QStringLiteral("M'yF13\"stP\\$44W0$3d/");

    createStorage(file, password);

    QVERIFY(opens(file, password));
}

void StorageKeyTest::passwordWithCjkUnlocksStorage()
{
    const auto file = storageFile("cjk");
    const auto password = QStringLiteral("密码-Passwort-2026");

    createStorage(file, password);

    QVERIFY(opens(file, password));
}

/**
 * The actual proof. Under the old routine the store was keyed with the pass
 * phrase stripped of everything outside 32 to 126, so the stripped form opened it
 * as well. It must not any more.
 */
void StorageKeyTest::truncatedPasswordDoesNotUnlockStorage()
{
    const auto file = storageFile("truncated");
    const auto password = QStringLiteral("Paßwort-Ümlaut-2026");
    const auto truncated = QStringLiteral("Pawort-mlaut-2026");

    createStorage(file, password);

    QVERIFY(!opens(file, truncated));
}

void StorageKeyTest::wrongPasswordIsRejected()
{
    const auto file = storageFile("wrongPassword");

    createStorage(file, QStringLiteral("Paßwort-Ümlaut-2026"));

    Storage storage(applicationInfo());
    storage.setKey(QStringLiteral("Something-Else-2026"));
    storage.setStorageFile(file);

    const auto error = storage.initialize(true);

    QVERIFY(error.isError());
    QVERIFY(!error.message().isEmpty());
    QVERIFY(!storage.isValid());

    storage.close();
}

/**
 * An unencrypted SQLite file starts with the string "SQLite format 3". Whoever
 * reads that here holds a store that never got a key.
 */
void StorageKeyTest::storageFileIsNotPlaintextSqlite()
{
    const auto file = storageFile("encrypted");

    createStorage(file, QStringLiteral("Paßwort-Ümlaut-2026"));

    QFile storageFileHandle(file);
    QVERIFY(storageFileHandle.open(QIODevice::ReadOnly));

    const QByteArray header = storageFileHandle.read(16);
    storageFileHandle.close();

    QVERIFY(!header.startsWith(QByteArrayLiteral("SQLite format 3")));
}

void StorageKeyTest::changeKeyPreservesData()
{
    const auto file = storageFile("changeKeyData");
    const auto oldPassword = QStringLiteral("Paßwort-Ümlaut-2026");
    const auto newPassword = QStringLiteral("Neues-Paßwort-Ümlaut-2027");

    {
        Storage storage(applicationInfo());
        storage.setKey(oldPassword);
        storage.setStorageFile(file);

        QVERIFY(!storage.initialize(true).isError());

        const auto account = BaseTest::createFakeAccount();
        QVERIFY(!storage.storeItem(account.get()).isError());

        QVERIFY(!storage.changeKey(oldPassword, newPassword).isError());
        storage.close();
    }

    Storage storage(applicationInfo());
    storage.setKey(newPassword);
    storage.setStorageFile(file);

    QVERIFY(!storage.initialize(true).isError());
    QVERIFY(storage.isValid());

    QSignalSpy itemsSpy(&storage, &Storage::itemsReceived);
    storage.receiveItems(Storage::StorageAccount);

    QCOMPARE(itemsSpy.count(), 1);
    QCOMPARE(qvariant_cast<BankingItems>(itemsSpy.takeFirst().at(0)).size(), 1);

    storage.close();
}

void StorageKeyTest::changeKeyLeavesOldKeyInvalid()
{
    const auto file = storageFile("changeKeyOldInvalid");
    const auto oldPassword = QStringLiteral("Paßwort-Ümlaut-2026");
    const auto newPassword = QStringLiteral("Neues-Paßwort-Ümlaut-2027");

    {
        Storage storage(applicationInfo());
        storage.setKey(oldPassword);
        storage.setStorageFile(file);

        QVERIFY(!storage.initialize(true).isError());
        QVERIFY(!storage.changeKey(oldPassword, newPassword).isError());
        storage.close();
    }

    QVERIFY(!opens(file, oldPassword));
    QVERIFY(opens(file, newPassword));
}

/**
 * The window used to travel into the statement unchecked.
 */
void StorageKeyTest::receiveItemsRejectsNegativeOffset()
{
    const auto file = storageFile("negativeOffset");

    Storage storage(applicationInfo());
    storage.setKey(QStringLiteral("Paßwort-Ümlaut-2026"));
    storage.setStorageFile(file);

    QVERIFY(!storage.initialize(true).isError());

    QSignalSpy errorSpy(&storage, &Storage::errorOccurred);

    storage.receiveItems(Storage::StorageAccount, -1, 50);

    QCOMPARE(errorSpy.count(), 1);
    QCOMPARE(errorSpy.takeFirst().at(0).value<ErrorCode>(), ErrorCode::InvalidInput);

    storage.close();
}

void StorageKeyTest::receiveItemsRejectsExcessiveLimit()
{
    const auto file = storageFile("excessiveLimit");

    Storage storage(applicationInfo());
    storage.setKey(QStringLiteral("Paßwort-Ümlaut-2026"));
    storage.setStorageFile(file);

    QVERIFY(!storage.initialize(true).isError());

    QSignalSpy errorSpy(&storage, &Storage::errorOccurred);

    storage.receiveItems(Storage::StorageAccount, 0, std::numeric_limits<int>::max());

    QCOMPARE(errorSpy.count(), 1);
    QCOMPARE(errorSpy.takeFirst().at(0).value<ErrorCode>(), ErrorCode::InvalidInput);

    storage.close();
}

} // namespace olbaflinx::core::storage::tests

QTEST_MAIN(olbaflinx::core::storage::tests::StorageKeyTest)

#include "tst_storage_key.moc"
