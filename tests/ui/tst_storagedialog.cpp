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

#include "ui/Storage/StorageDialog.h"

#include "core/ApplicationInfo.h"
#include "core/Storage/Storage.h"
#include "ui/Storage/NewStorageItem.h"

#include <QtTest/QtTest>

#include <QtWidgets/QLabel>

#include <memory>

using namespace olbaflinx::core;
using namespace olbaflinx::core::storage;
using namespace olbaflinx::ui::storage;

namespace olbaflinx::ui::storage::tests {

class StorageDialogTest final : public QObject
{
    Q_OBJECT

private:
    static ApplicationInfo applicationInfo()
    {
        return {QStringLiteral("de.chm-projects.olbaflinx.test"),
                QStringLiteral("OlbaFlinxStorageDialogTest"),
                QStringLiteral("1.0.0")};
    }

    // Twelve characters with a lower and an upper case letter, a digit and a
    // special character, which is what the core asks of a pass phrase.
    static QString password() { return QStringLiteral("Aa1!Aa1!Aa1!"); }

    static QStringList storedPaths(const Storage &storage)
    {
        return storage
            .setting(QStringLiteral("Paths"), QStringLiteral("Items"), QStringList())
            .toStringList();
    }

    std::unique_ptr<QTemporaryDir> workingDirectory;
    QByteArray previousHome;

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void cleanup();
    void repeatedReloadKeepsTheInfoLabelUsable();
    void dialogDoesNotCloseTheStorageItDoesNotOwn();
    void aCreatedStorageIsStillThereAfterTheOverviewIsBuiltAgain();
    void aTakenNameGetsANumberBehindASeparator();
    void anEntryWhoseFileIsGoneDoesNotShowUp();
};

void StorageDialogTest::initTestCase()
{
    // Keeps QSettings out of the real user configuration, see QStandardPaths docs.
    QStandardPaths::setTestModeEnabled(true);

    // Test mode alone puts the settings below the home directory, and
    // Storage::storagePath() derives from the same location. Pointing HOME at a
    // temporary directory keeps the settings and every storage file this test
    // creates inside it.
    workingDirectory = std::make_unique<QTemporaryDir>();
    QVERIFY(workingDirectory->isValid());

    previousHome = qgetenv("HOME");
    qputenv("HOME", workingDirectory->path().toLocal8Bit());
}

void StorageDialogTest::cleanupTestCase()
{
    qputenv("HOME", previousHome);
    workingDirectory.reset();
}

/**
 * The storage files are removed between the test functions, the settings file is
 * not. Every function that cares about the list writes it first, and removing
 * the file out from under QSettings would leave its own cache in charge.
 */
void StorageDialogTest::cleanup()
{
    const Storage storage(applicationInfo());

    QDir directory(storage.storagePath());
    const auto files = directory.entryList({QStringLiteral("*.olbflx")}, QDir::Files);

    for (const auto &file : files) {
        QVERIFY(directory.remove(file));
    }
}

/**
 * removeStorageInfo() used to release the label without setting the member to
 * nullptr. The second run therefore hit a released pointer. The fault only ever
 * shows up as a crash, never as a failed assertion.
 *
 * The path is reachable through reload(), because loadStorageItems() calls
 * removeStorageInfo() and addStorageInfo() one after the other for an empty
 * list.
 */
void StorageDialogTest::repeatedReloadKeepsTheInfoLabelUsable()
{
    Storage storage(applicationInfo());
    storage.storeSetting("Paths", QStringList(), "Items");

    StorageDialog dialog(&storage);
    dialog.initialize(nullptr);

    dialog.reload();
    dialog.reload();

    const auto labels = dialog.findChildren<QLabel *>();
    const bool hasInfoLabel = std::any_of(labels.cbegin(), labels.cend(), [](const QLabel *label) {
        return label->textFormat() == Qt::RichText && label->text().contains("OlbaFlinx");
    });

    QVERIFY(hasInfoLabel);
}

/**
 * The dialog used to co-own the storage and released it in its own destructor.
 * It now stays usable once the window is gone.
 */
void StorageDialogTest::dialogDoesNotCloseTheStorageItDoesNotOwn()
{
    Storage storage(applicationInfo());
    storage.storeSetting("Paths", QStringList(), "Items");

    {
        StorageDialog dialog(&storage);
        dialog.initialize(nullptr);
    }

    storage.storeSetting("Probe", QStringList(), "Lifetime");

    QCOMPARE(storage.setting("Probe", "Lifetime", QStringList()).toStringList().size(), 0);
}

/**
 * FR-049, SC-013. The list used to be read and written by nobody, so a storage
 * that had just been created was gone the next time the overview was built.
 *
 * The check goes out through storeSetting and back in through setting, because
 * the two take key, value and group in a different order and a swapped pair
 * would file the list under a name the reading side never looks at.
 */
void StorageDialogTest::aCreatedStorageIsStillThereAfterTheOverviewIsBuiltAgain()
{
    Storage storage(applicationInfo());
    storage.storeSetting(QStringLiteral("Paths"), QStringList(), QStringLiteral("Items"));

    StorageDialog dialog(&storage);
    dialog.initialize(nullptr);

    QVERIFY(dialog.createStorage(QStringLiteral("Privat"), password()));

    const QStringList paths = storedPaths(storage);
    QCOMPARE(paths.size(), 1);
    QVERIFY(paths.first().endsWith(QStringLiteral("/Privat.olbflx")));
    QVERIFY(QFileInfo::exists(paths.first()));

    dialog.reload();

    const auto entries = dialog.findChildren<NewStorageItem *>();
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries.first()->filePath(), paths.first());
}

/**
 * FR-047, US1a scenarios 5 and 6. The name that the message announces is the one
 * the storage has to be created under, so both sides ask the same function.
 *
 * The message itself is modal and stays out of this test; what it announces and
 * what gets created are checked here, the cancel path of scenario 7 is walked by
 * hand.
 */
void StorageDialogTest::aTakenNameGetsANumberBehindASeparator()
{
    Storage storage(applicationInfo());
    storage.storeSetting(QStringLiteral("Paths"), QStringList(), QStringLiteral("Items"));

    StorageDialog dialog(&storage);
    dialog.initialize(nullptr);

    QCOMPARE(dialog.availableName(QStringLiteral("Privat")), QStringLiteral("Privat"));

    QVERIFY(dialog.createStorage(QStringLiteral("Privat"), password()));
    QCOMPARE(dialog.availableName(QStringLiteral("Privat")), QStringLiteral("Privat-2"));

    QVERIFY(dialog.createStorage(QStringLiteral("Privat-2"), password()));
    QCOMPARE(dialog.availableName(QStringLiteral("Privat")), QStringLiteral("Privat-3"));

    const QStringList paths = storedPaths(storage);
    QCOMPARE(paths.size(), 2);
    QVERIFY(paths.at(1).endsWith(QStringLiteral("/Privat-2.olbflx")));
}

/**
 * FR-050, SC-013. A file that was removed outside the application leaves the
 * list instead of standing in the overview as an entry that cannot be opened.
 */
void StorageDialogTest::anEntryWhoseFileIsGoneDoesNotShowUp()
{
    Storage storage(applicationInfo());
    storage.storeSetting(QStringLiteral("Paths"), QStringList(), QStringLiteral("Items"));

    StorageDialog dialog(&storage);
    dialog.initialize(nullptr);

    QVERIFY(dialog.createStorage(QStringLiteral("Privat"), password()));
    QCOMPARE(dialog.findChildren<NewStorageItem *>().size(), 1);

    QVERIFY(QFile::remove(storedPaths(storage).first()));

    dialog.reload();

    QCOMPARE(dialog.findChildren<NewStorageItem *>().size(), 0);
    QVERIFY(storedPaths(storage).isEmpty());
}

} // namespace olbaflinx::ui::storage::tests

QTEST_MAIN(olbaflinx::ui::storage::tests::StorageDialogTest)

#include "tst_storagedialog.moc"
