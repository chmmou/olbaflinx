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

#include <QtTest/QtTest>

#include <limits>

using namespace olbaflinx::core;
using namespace olbaflinx::core::banking;
using namespace olbaflinx::core::storage;

namespace olbaflinx::core::storage::tests {

using namespace olbaflinx::core::tests;

/**
 * The key as it reaches SQLCipher, and the policy a key has to satisfy before it
 * gets there. The pass phrase used to run through an escape routine that read
 * every character through QChar::toLatin1, which answers with a signed char here.
 * Its range check for the upper half of Latin-1 could never be true, so every
 * character outside 32 to 126 was dropped from the key silently. The store still
 * opened, because the same loss happened on every open.
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

    static bool accepts(const QString &password)
    {
        const Storage storage(applicationInfo());
        return storage.minPasswordGuidelines().match(password).hasMatch();
    }

    /**
     * The longest key the policy allows. Built here so that the length stays in
     * one place, see the decision on key handling from 2026-07-30.
     */
    static constexpr int MaximumPasswordLength = 128;

    static QString maximumLengthPassword()
    {
        return QStringLiteral("Ab1!")
               + QString(MaximumPasswordLength - 4, QLatin1Char('c'));
    }

private Q_SLOTS:
    void initTestCase();

    void passwordUnlocksStorage_data();
    void passwordUnlocksStorage();
    void truncatedPasswordDoesNotUnlockStorage();
    void wrongPasswordIsRejected();
    void storageFileIsNotPlaintextSqlite();
    void changeKeyPreservesData();
    void changeKeyLeavesOldKeyInvalid();
    void receiveItemsRejectsInvalidWindow_data();
    void receiveItemsRejectsInvalidWindow();

    void minPasswordGuidelinesReturnsValidPattern();
    void minPasswordGuidelinesIsStable();
    void passwordPolicyBoundsLengthAtTheDecidedMaximum();
    void passwordPolicyAccepts_data();
    void passwordPolicyAccepts();
    void passwordPolicyRejects_data();
    void passwordPolicyRejects();
};

void StorageKeyTest::initTestCase()
{
    // Keeps QSettings out of the real user configuration, see QStandardPaths docs.
    QStandardPaths::setTestModeEnabled(true);

    QVERIFY(workingDirectory.isValid());
}

/**
 * Every character a pass phrase may carry has to survive the way into SQLCipher.
 * The single quote is the one character that could end the string literal the key
 * travels in; it is doubled. The double quote and the backslash carry no meaning
 * there and stay as they are. Everything outside 32 to 126 used to be dropped.
 */
void StorageKeyTest::passwordUnlocksStorage_data()
{
    QTest::addColumn<QString>("password");

    QTest::newRow("ascii") << QStringLiteral("Kennwort-2026!aA");
    QTest::newRow("quotes") << QStringLiteral("M'yF13\"stP\\$44W0$3d/");
    QTest::newRow("umlauts") << QStringLiteral("Paßwort-Ümlaut-2026");
    QTest::newRow("cjk") << QStringLiteral("密码-Passwort-2026");
    // Outside the basic multilingual plane, so a surrogate pair in UTF-16.
    QTest::newRow("emoji") << QStringLiteral("Schlüssel-2026!aA\U0001F511");
    QTest::newRow("maxLength") << maximumLengthPassword();
}

void StorageKeyTest::passwordUnlocksStorage()
{
    QFETCH(QString, password);

    const auto file = storageFile(QTest::currentDataTag());

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

        const auto account = TestHelpers::createFakeAccount();
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
void StorageKeyTest::receiveItemsRejectsInvalidWindow_data()
{
    QTest::addColumn<int>("offset");
    QTest::addColumn<int>("limit");

    QTest::newRow("negativeOffset") << -1 << 50;
    QTest::newRow("excessiveLimit") << 0 << std::numeric_limits<int>::max();
}

void StorageKeyTest::receiveItemsRejectsInvalidWindow()
{
    QFETCH(int, offset);
    QFETCH(int, limit);

    const auto file = storageFile(QTest::currentDataTag());

    Storage storage(applicationInfo());
    storage.setKey(QStringLiteral("Paßwort-Ümlaut-2026"));
    storage.setStorageFile(file);

    QVERIFY(!storage.initialize(true).isError());

    QSignalSpy errorSpy(&storage, &Storage::errorOccurred);

    storage.receiveItems(Storage::StorageAccount, offset, limit);

    QCOMPARE(errorSpy.count(), 1);
    QCOMPARE(errorSpy.takeFirst().at(0).value<ErrorCode>(), ErrorCode::InvalidInput);

    storage.close();
}

/**
 * The pattern used to be built by a macro on every call. A typo in the escaping
 * would only have shown up as a never matching password. The check on
 * QRegularExpression::isValid catches that at the source.
 */
void StorageKeyTest::minPasswordGuidelinesReturnsValidPattern()
{
    const Storage storage(applicationInfo());

    const QRegularExpression pattern = storage.minPasswordGuidelines();

    QVERIFY(pattern.isValid());
    QVERIFY(!pattern.pattern().isEmpty());
    QCOMPARE(pattern.errorString(), QStringLiteral("no error"));
}

/**
 * The pattern is now held in a function local static. Two calls have to yield
 * the same pattern, otherwise the compiled form is not shared.
 */
void StorageKeyTest::minPasswordGuidelinesIsStable()
{
    const Storage storage(applicationInfo());

    const QRegularExpression first = storage.minPasswordGuidelines();
    const QRegularExpression second = storage.minPasswordGuidelines();

    QCOMPARE(first.pattern(), second.pattern());
    QCOMPARE(first, second);
}

/**
 * The two rows named maxLength and tooLong sit on either side of the bound. This
 * holds the bound itself in place, so that a change to the policy shows up here
 * and not as a row that quietly tests nothing.
 */
void StorageKeyTest::passwordPolicyBoundsLengthAtTheDecidedMaximum()
{
    QCOMPARE(maximumLengthPassword().length(), MaximumPasswordLength);
}

/**
 * The backslash, 0x5C, used to reach the class of special characters only through
 * a range that also covered every digit and every capital letter. Removing that
 * range would have taken the backslash out along with the digits, so it now
 * stands in the class on its own. The rows below hold that in place, together
 * with the two characters outside ASCII the class names explicitly.
 */
void StorageKeyTest::passwordPolicyAccepts_data()
{
    QTest::addColumn<QString>("password");

    QTest::newRow("documentedExample") << QStringLiteral("M'yF13\"stP\\$44W0$3d/");
    QTest::newRow("umlautAsSpecialChar") << QStringLiteral("Paßwort-Ümlaut-2026");
    QTest::newRow("backslash") << QStringLiteral("Passwort\\mit1X");
    QTest::newRow("euroSign") << QStringLiteral("Passwort€mit1X");
    QTest::newRow("maxLength") << maximumLengthPassword();
}

void StorageKeyTest::passwordPolicyAccepts()
{
    QFETCH(QString, password);

    QVERIFY(accepts(password));
}

/**
 * The class of special characters used to carry the sequence '#-_', which a
 * character class reads as a range from 0x23 to 0x5F. That covers every digit and
 * every capital letter, so the lookahead for a special character matched on those
 * alone and asked for nothing beyond the two lookaheads before it.
 *
 * An unbounded length is an unchecked size, see QT-SEC-004. There used to be no
 * upper bound at all.
 */
void StorageKeyTest::passwordPolicyRejects_data()
{
    QTest::addColumn<QString>("password");

    QTest::newRow("digitsOnlyAsSpecialChar") << QStringLiteral("Abcdefgh1234");
    QTest::newRow("tooShort") << QStringLiteral("Abcdef1!");
    QTest::newRow("tooLong") << maximumLengthPassword() + QLatin1Char('c');
}

void StorageKeyTest::passwordPolicyRejects()
{
    QFETCH(QString, password);

    QVERIFY(!accepts(password));
}

} // namespace olbaflinx::core::storage::tests

QTEST_MAIN(olbaflinx::core::storage::tests::StorageKeyTest)

#include "tst_storage_key.moc"
