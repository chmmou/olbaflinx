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
#include "ui/Storage/NewStorageDialog.h"
#include "ui/Storage/NewStorageItem.h"

#include "TestHelpers.h"

#include <QtTest/QtTest>

#include <QtCore/QTimer>

#include <QtGui/QAccessible>
#include <QtGui/QAccessibleInterface>

#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>

#include <chrono>
#include <functional>
#include <memory>

using namespace olbaflinx::core;
using namespace olbaflinx::core::storage;
using namespace olbaflinx::ui::storage;

namespace olbaflinx::ui::storage::tests {

using namespace olbaflinx::core::tests;

class StorageDialogTest final : public QObject
{
    Q_OBJECT

private:
    static ApplicationInfo applicationInfo()
    {
        return TestHelpers::applicationInfo(QStringLiteral("OlbaFlinxStorageDialogTest"));
    }

    static QString password() { return TestHelpers::minimalPassword(); }

    static QLineEdit *fieldOf(const QWidget *dialog, const QString &name)
    {
        return dialog->findChild<QLineEdit *>(name);
    }

    static QStringList storedPaths(const Storage &storage)
    {
        return storage.setting(QStringLiteral("Paths"), QStringLiteral("Items"), QStringList())
            .toStringList();
    }

    /**
     * Opens the password change dialog of an entry and hands it to a check.
     * Answers whether the dialog appeared and the check ran.
     *
     * The dialog runs an event loop of its own, so nothing after the call gets to
     * see it. A timer looks for the modal window instead, runs the check while it
     * stands and closes it afterwards.
     */
    static bool withPasswordChangeDialog(NewStorageItem *entry,
                                         const std::function<void(QDialog *)> &check)
    {
        bool checked = false;
        bool gaveUp = false;

        // A dialog that never comes would leave the call below waiting forever.
        QTimer::singleShot(std::chrono::seconds(5), entry, [&gaveUp] { gaveUp = true; });

        QTimer driver;
        driver.setInterval(0);

        connect(&driver, &QTimer::timeout, entry, [&] {
            auto *modal = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if (modal == nullptr && !gaveUp) {
                return;
            }

            driver.stop();

            if (modal == nullptr) {
                return;
            }

            if (!gaveUp) {
                check(modal);
                checked = true;
            }

            modal->reject();
        });

        driver.start();
        QMetaObject::invokeMethod(entry, "showPasswordChangeDialog");

        return checked;
    }

    /**
     * The single entry of an overview, or null when it does not hold exactly
     * one storage.
     */
    static NewStorageItem *singleEntryOf(const StorageDialog &dialog)
    {
        const auto entries = dialog.findChildren<NewStorageItem *>();
        return entries.size() == 1 ? entries.first() : nullptr;
    }

private Q_SLOTS:
    void initTestCase();
    void cleanup();
    void repeatedReloadKeepsTheInfoLabelUsable();
    void dialogDoesNotCloseTheStorageItDoesNotOwn();
    void aCreatedStorageIsStillThereAfterTheOverviewIsBuiltAgain();
    void aTakenNameGetsANumberBehindASeparator();
    void theWelcomeTextStaysAwayWhenThereIsAnEntry();
    void deletingTheLastStorageBringsTheWelcomeTextBack();
    void cancellingTheConflictMessageLeavesTheDialogStanding();
    void anEntryWhoseFileIsGoneDoesNotShowUp();
    void anEntryOutsideTheStorageDirectoryDoesNotShowUp();
    void aNameThatLeavesTheDirectoryCreatesNothing();
    void anEntryReportsWhetherItsBackupWasWritten();
    void openingWithoutAWindowIsRefusedAndSaidSo();
    void theButtonsOfAnEntryAreWiredToItsSlots();
    void anEntryReportsItselfAsAGroupUnderItsName();
    void everyControlOfAnEntryCarriesAName_data();
    void everyControlOfAnEntryCarriesAName();
    void bothFieldsOfThePasswordChangeDialogHideTheirContent();
    void thePasswordChangeDialogOpensWithTheFocusOnTheCurrentPassword();
    void theInformationEntrySaysThatItDoesNotAct();
};

void StorageDialogTest::initTestCase()
{
    // Keeps QSettings out of the real user configuration, see QStandardPaths docs.
    QStandardPaths::setTestModeEnabled(true);

    // Test mode alone puts the locations below ~/.qttest, which is a directory
    // of the user like any other and survives the run. HOME goes into a
    // temporary directory, so that nothing this binary writes outlives it.
    QVERIFY(TestHelpers::useTemporaryHome());
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
 * The list used to be read and written by nobody, so a storage that had just
 * been created was gone the next time the overview was built.
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
 * The name that the message announces is the one the storage has to be created
 * under, so both sides ask the same function.
 *
 * The message itself is modal and stays out of this test; what it announces and
 * what gets created are checked here. Cancelling it has a test of its own.
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
 * The welcome text belongs to the empty overview and to nothing else. It is
 * built in initialize, before the window is shown, and it is a child of the
 * dialog rather than of the layout. Where a stored entry exists, it never
 * reaches the layout, and a child that no layout places sits at the top left
 * corner on top of whatever is there.
 *
 * The case only exists since the list of storages survives a restart. Before
 * that the overview was empty at every start.
 */
void StorageDialogTest::theWelcomeTextStaysAwayWhenThereIsAnEntry()
{
    Storage storage(applicationInfo());
    storage.storeSetting(QStringLiteral("Paths"), QStringList(), QStringLiteral("Items"));

    {
        StorageDialog first(&storage);
        first.initialize(nullptr);
        QVERIFY(first.createStorage(QStringLiteral("Privat"), password()));
    }

    // A second dialog over the same settings is what a restart looks like.
    StorageDialog dialog(&storage);
    dialog.initialize(nullptr);

    QCOMPARE(dialog.findChildren<NewStorageItem *>().size(), 1);

    const auto labels = dialog.findChildren<QLabel *>();
    const auto welcome = std::find_if(labels.cbegin(), labels.cend(), [](const QLabel *label) {
        return label->text().contains(QStringLiteral("<h1>"));
    });

    QVERIFY(welcome != labels.cend());
    QVERIFY(!(*welcome)->isVisibleTo(&dialog));
}

/**
 * The other half of the same rule: an overview that has just lost its last
 * entry is empty, and an empty overview says what a data vault is for.
 *
 * Deleting used to drop the entry and nothing else, so the overview never
 * rebuilt and the text never reached the layout. It stood there anyway, because
 * it was sitting free in the corner; the two faults hid each other.
 */
void StorageDialogTest::deletingTheLastStorageBringsTheWelcomeTextBack()
{
    Storage storage(applicationInfo());
    storage.storeSetting(QStringLiteral("Paths"), QStringList(), QStringLiteral("Items"));

    StorageDialog dialog(&storage);
    dialog.initialize(nullptr);

    QVERIFY(dialog.createStorage(QStringLiteral("Privat"), password()));

    const auto entries = dialog.findChildren<NewStorageItem *>();
    QCOMPARE(entries.size(), 1);

    QVERIFY(QFile::remove(entries.first()->filePath()));

    // The way the entry reports that its file is gone. The dialog rebuilds from
    // there, and it does so through the event loop, because the entry that sent
    // this is one of the widgets the rebuild releases.
    Q_EMIT entries.first()->storageDeleted(true, entries.first(), QString());

    QTRY_COMPARE(dialog.findChildren<NewStorageItem *>().size(), 0);

    const auto labels = dialog.findChildren<QLabel *>();
    const auto welcome = std::find_if(labels.cbegin(), labels.cend(), [](const QLabel *label) {
        return label->text().contains(QStringLiteral("<h1>"));
    });

    QVERIFY(welcome != labels.cend());
    QVERIFY((*welcome)->isVisibleTo(&dialog));

    // Visible is not enough: the label was visible before this was fixed, but as
    // a free child of the dialog sitting in the corner. Only a layout moves it
    // into the contents of the scroll area.
    QVERIFY((*welcome)->parentWidget() != nullptr);
    QCOMPARE((*welcome)->parentWidget()->objectName(), QStringLiteral("scrollAreaStorageContents"));

    QVERIFY(storedPaths(storage).isEmpty());
}

/**
 * Cancelling the conflict message has to bring the entry dialog back with what
 * was entered, not end the command. That only holds because the dialog is shown
 * again in a loop instead of being built again.
 *
 * Two modal loops stack up here and addStorage() does not return until both are
 * gone, so the test drives them from a timer. The timer reacts to whichever
 * window is modal rather than to a moment it could compute: the second
 * appearance of the entry dialog is a state, not a point in time.
 */
void StorageDialogTest::cancellingTheConflictMessageLeavesTheDialogStanding()
{
    Storage storage(applicationInfo());
    storage.storeSetting(QStringLiteral("Paths"), QStringList(), QStringLiteral("Items"));

    StorageDialog dialog(&storage);
    dialog.initialize(nullptr);

    QVERIFY(dialog.createStorage(QStringLiteral("Privat"), password()));

    bool wasFilledIn = false;
    bool messageWasCancelled = false;
    bool gaveUp = false;
    QString nameAfterCancelling;
    QString passwordAfterCancelling;

    // A window that never comes would leave addStorage() waiting forever. This
    // turns that into a failed test instead of a hanging one.
    QTimer::singleShot(std::chrono::seconds(5), &dialog, [&gaveUp] { gaveUp = true; });

    QTimer driver;
    driver.setInterval(0);

    connect(&driver, &QTimer::timeout, &dialog, [&] {
        auto *modal = QApplication::activeModalWidget();
        if (modal == nullptr) {
            return;
        }

        if (gaveUp) {
            modal->close();
            return;
        }

        if (auto *message = qobject_cast<QMessageBox *>(modal)) {
            messageWasCancelled = true;
            message->button(QMessageBox::No)->click();
            return;
        }

        auto *entry = qobject_cast<NewStorageDialog *>(modal);
        if (entry == nullptr) {
            return;
        }

        if (!wasFilledIn) {
            wasFilledIn = true;

            fieldOf(entry, QStringLiteral("lineEditStorageName"))->setText(QStringLiteral("Privat"));
            fieldOf(entry, QStringLiteral("lineEditPassword"))->setText(password());
            fieldOf(entry, QStringLiteral("lineEditPasswordConfirm"))->setText(password());

            entry->findChild<QPushButton *>(QStringLiteral("pushButtonOk"))->click();
            return;
        }

        // Ok leaves the dialog modal for as long as its loop takes to unwind, so
        // seeing it again before the message means the driver is early.
        if (!messageWasCancelled) {
            return;
        }

        nameAfterCancelling = fieldOf(entry, QStringLiteral("lineEditStorageName"))->text();
        passwordAfterCancelling = fieldOf(entry, QStringLiteral("lineEditPassword"))->text();

        driver.stop();
        entry->reject();
    });

    driver.start();
    dialog.addStorage();

    QVERIFY(!gaveUp);
    QVERIFY(messageWasCancelled);
    QCOMPARE(nameAfterCancelling, QStringLiteral("Privat"));
    QCOMPARE(passwordAfterCancelling, password());

    // The storage that was there is the only one, and the announced name was not
    // used behind the user's back.
    const QStringList paths = storedPaths(storage);
    QCOMPARE(paths.size(), 1);
    QVERIFY(paths.first().endsWith(QStringLiteral("/Privat.olbflx")));
    QCOMPARE(dialog.availableName(QStringLiteral("Privat")), QStringLiteral("Privat-2"));
}

/**
 * A file that was removed outside the application leaves the list instead of
 * standing in the overview as an entry that cannot be opened.
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

/**
 * The list of storages is kept in a plain settings file, which is an input from
 * outside the process like any other. Every entry of the overview reaches
 * QFile::remove, QFile::copy and setStorageFile from that list, and it used to
 * be filtered on nothing but whether the file exists.
 *
 * An entry pointing at a private key of the user therefore stood in the
 * overview like a vault of his, and the entry for removing it removed that file.
 */
void StorageDialogTest::anEntryOutsideTheStorageDirectoryDoesNotShowUp()
{
    QTemporaryDir elsewhere;
    QVERIFY(elsewhere.isValid());

    const QString outsideFile = elsewhere.filePath(QStringLiteral("id_ed25519"));

    {
        QFile file(outsideFile);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(QByteArrayLiteral("not a storage")), 13);
    }

    Storage storage(applicationInfo());
    storage.storeSetting(QStringLiteral("Paths"), QStringList(), QStringLiteral("Items"));

    StorageDialog dialog(&storage);
    dialog.initialize(nullptr);

    QVERIFY(dialog.createStorage(QStringLiteral("Privat"), password()));
    QCOMPARE(dialog.findChildren<NewStorageItem *>().size(), 1);

    auto paths = storedPaths(storage);
    QCOMPARE(paths.size(), 1);

    paths.append(outsideFile);
    storage.storeSetting(QStringLiteral("Paths"), paths, QStringLiteral("Items"));

    dialog.reload();

    // The vault is shown, the foreign file is not, and it leaves the list on the
    // way. Nothing of it was touched: it is still there and still holds what it
    // held.
    QCOMPARE(dialog.findChildren<NewStorageItem *>().size(), 1);
    QCOMPARE(storedPaths(storage).size(), 1);
    QVERIFY(!storedPaths(storage).contains(outsideFile));

    QVERIFY(QFileInfo::exists(outsideFile));
    QCOMPARE(QFileInfo(outsideFile).size(), 13);
}

/**
 * The dialog refuses a name with a path separator in it, so this cannot be
 * reached through the window. createStorage can be called without it, and a name
 * that walks up a directory would write the file anywhere the process may write.
 */
void StorageDialogTest::aNameThatLeavesTheDirectoryCreatesNothing()
{
    Storage storage(applicationInfo());
    storage.storeSetting(QStringLiteral("Paths"), QStringList(), QStringLiteral("Items"));

    StorageDialog dialog(&storage);
    dialog.initialize(nullptr);

    QVERIFY(!dialog.createStorage(QStringLiteral("../Privat"), password()));

    QVERIFY(storedPaths(storage).isEmpty());
    QVERIFY(!QFileInfo::exists(QStringLiteral("%1/../Privat.olbflx").arg(storage.storagePath())));
}

/**
 * An entry is a widget of its own that draws a title, a password field and two
 * buttons. A plain widget reports itself as a client area and carries no name,
 * which leaves a tool with a nameless box where a storage should be.
 */
void StorageDialogTest::anEntryReportsItselfAsAGroupUnderItsName()
{
    Storage storage(applicationInfo());
    storage.storeSetting(QStringLiteral("Paths"), QStringList(), QStringLiteral("Items"));

    StorageDialog dialog(&storage);
    dialog.initialize(nullptr);

    QVERIFY(dialog.createStorage(QStringLiteral("Privat"), password()));
    dialog.reload();

    const auto entries = dialog.findChildren<NewStorageItem *>();
    QCOMPARE(entries.size(), 1);

    QAccessibleInterface *accessible = QAccessible::queryAccessibleInterface(entries.first());
    QVERIFY(accessible != nullptr);

    QCOMPARE(accessible->role(), QAccessible::Grouping);
    QCOMPARE(accessible->text(QAccessible::Name), QStringLiteral("Privat"));
}

/**
 * Two of the three carry an icon and no text, and the field has no label beside
 * it. Nothing in the form says what any of them does.
 */
void StorageDialogTest::everyControlOfAnEntryCarriesAName_data()
{
    QTest::addColumn<QString>("objectName");
    QTest::addColumn<QAccessible::Role>("role");

    QTest::newRow("menu") << QStringLiteral("btnStorageMenu") << QAccessible::Button;
    QTest::newRow("open") << QStringLiteral("btnOpenStorage") << QAccessible::Button;
    QTest::newRow("password") << QStringLiteral("leStoragePassword") << QAccessible::EditableText;
}

void StorageDialogTest::everyControlOfAnEntryCarriesAName()
{
    QFETCH(QString, objectName);
    QFETCH(QAccessible::Role, role);

    Storage storage(applicationInfo());
    storage.storeSetting(QStringLiteral("Paths"), QStringList(), QStringLiteral("Items"));

    StorageDialog dialog(&storage);
    dialog.initialize(nullptr);

    QVERIFY(dialog.createStorage(QStringLiteral("Privat"), password()));
    dialog.reload();

    const auto entries = dialog.findChildren<NewStorageItem *>();
    QCOMPARE(entries.size(), 1);

    auto *control = entries.first()->findChild<QWidget *>(objectName);
    QVERIFY(control != nullptr);

    QAccessibleInterface *accessible = QAccessible::queryAccessibleInterface(control);
    QVERIFY(accessible != nullptr);

    QVERIFY(!accessible->text(QAccessible::Name).isEmpty());
    QCOMPARE(accessible->role(), role);
}

/**
 * Both fields used to show the pass phrase of the storage in clear text and to
 * hand it out through the accessibility interface, where a reading aid speaks
 * it. The dialog beside this one was fixed for the same reason.
 */
void StorageDialogTest::bothFieldsOfThePasswordChangeDialogHideTheirContent()
{
    Storage storage(applicationInfo());
    storage.storeSetting(QStringLiteral("Paths"), QStringList(), QStringLiteral("Items"));

    StorageDialog dialog(&storage);
    dialog.initialize(nullptr);

    QVERIFY(dialog.createStorage(QStringLiteral("Privat"), password()));
    dialog.reload();

    auto *entry = singleEntryOf(dialog);
    QVERIFY(entry != nullptr);

    // Read out of the dialog while it stands, because it takes its fields with
    // it when it closes.
    QList<QLineEdit::EchoMode> modes;
    QList<bool> reportedAsPasswordField;
    QList<bool> handedOutTheSecret;

    const QString secret = password();

    QVERIFY(withPasswordChangeDialog(entry, [&](QDialog *pwdChangeDialog) {
        for (const auto &name :
             {QStringLiteral("lineEditCurrentPassword"), QStringLiteral("lineEditNewPassword")}) {
            auto *field = pwdChangeDialog->findChild<QLineEdit *>(name);
            if (field == nullptr) {
                return;
            }

            field->setText(secret);
            modes.append(field->echoMode());

            QAccessibleInterface *accessible = QAccessible::queryAccessibleInterface(field);
            if (accessible == nullptr) {
                return;
            }

            reportedAsPasswordField.append(accessible->state().passwordEdit);
            handedOutTheSecret.append(accessible->text(QAccessible::Value).contains(secret));
        }
    }));

    QCOMPARE(modes, QList<QLineEdit::EchoMode>({QLineEdit::Password, QLineEdit::Password}));
    QCOMPARE(reportedAsPasswordField, QList<bool>({true, true}));
    QCOMPARE(handedOutTheSecret, QList<bool>({false, false}));
}

/**
 * The current pass phrase is what the user begins with. The dialog is built in
 * code, and nothing there says where the focus starts.
 */
void StorageDialogTest::thePasswordChangeDialogOpensWithTheFocusOnTheCurrentPassword()
{
    Storage storage(applicationInfo());
    storage.storeSetting(QStringLiteral("Paths"), QStringList(), QStringLiteral("Items"));

    StorageDialog dialog(&storage);
    dialog.initialize(nullptr);

    QVERIFY(dialog.createStorage(QStringLiteral("Privat"), password()));
    dialog.reload();

    auto *entry = singleEntryOf(dialog);
    QVERIFY(entry != nullptr);

    bool focusIsOnTheCurrentPassword = false;

    QVERIFY(withPasswordChangeDialog(entry, [&](QDialog *pwdChangeDialog) {
        auto *field = pwdChangeDialog->findChild<QLineEdit *>(
            QStringLiteral("lineEditCurrentPassword"));

        focusIsOnTheCurrentPassword = field != nullptr && pwdChangeDialog->focusWidget() == field;
    }));

    QVERIFY(focusIsOnTheCurrentPassword);
}

/**
 * The entry used to answer with "Not implemented yet!", which told a tool that
 * the command can be invoked. It cannot, and the state is where that belongs.
 */
void StorageDialogTest::theInformationEntrySaysThatItDoesNotAct()
{
    Storage storage(applicationInfo());
    storage.storeSetting(QStringLiteral("Paths"), QStringList(), QStringLiteral("Items"));

    StorageDialog dialog(&storage);
    dialog.initialize(nullptr);

    QVERIFY(dialog.createStorage(QStringLiteral("Privat"), password()));
    dialog.reload();

    auto *entry = singleEntryOf(dialog);
    QVERIFY(entry != nullptr);

    // The menu is built on demand and pops up without a loop of its own.
    QMetaObject::invokeMethod(entry, "showMenu");

    auto *menu = entry->findChild<QMenu *>();
    QVERIFY(menu != nullptr);

    const auto actions = menu->actions();
    QVERIFY(!actions.isEmpty());

    // An action carries no interface of its own; the menu holds one child per
    // action, in the order they were added.
    QAccessibleInterface *menuInterface = QAccessible::queryAccessibleInterface(menu);
    QVERIFY(menuInterface != nullptr);

    QAccessibleInterface *informationEntry = menuInterface->child(0);
    QVERIFY(informationEntry != nullptr);

    QVERIFY(!informationEntry->text(QAccessible::Name).isEmpty());
    QVERIFY(informationEntry->state().disabled);

    // The command below it acts, so the state says something.
    QAccessibleInterface *changeEntry = menuInterface->child(1);
    QVERIFY(changeEntry != nullptr);
    QVERIFY(!changeEntry->state().disabled);

    menu->close();
}

/**
 * The backup used to evaluate neither the directory it creates nor the copy it
 * makes, and it said nothing either way. QFile::copy does not overwrite, so a
 * name that is taken is a failure like any other, and the user was left holding
 * a backup that was never written - while the dialog that changes a password
 * points him at exactly such a backup.
 */
void StorageDialogTest::anEntryReportsWhetherItsBackupWasWritten()
{
    Storage storage(applicationInfo());
    storage.storeSetting(QStringLiteral("Paths"), QStringList(), QStringLiteral("Items"));

    StorageDialog dialog(&storage);
    dialog.initialize(nullptr);

    QVERIFY(dialog.createStorage(QStringLiteral("Privat"), password()));
    dialog.reload();

    auto *entry = singleEntryOf(dialog);
    QVERIFY(entry != nullptr);

    QSignalSpy messageSpy(&dialog, &StorageDialog::message);

    QMetaObject::invokeMethod(entry, "backupStorage");

    QCOMPARE(messageSpy.count(), 1);

    // The name of the vault alone does not tell the two apart: both messages
    // carry it. What the file says is the one that holds.
    QVERIFY(messageSpy.takeFirst().at(0).toString().contains(QStringLiteral("was written")));

    // The name of the vault is in the file name, so the backups of two vaults in
    // one directory can be told apart.
    const QDir backupDirectory(QStringLiteral("%1/backup").arg(storage.storagePath()));
    const auto written = backupDirectory.entryList({QStringLiteral("Privat-*")}, QDir::Files);

    QCOMPARE(written.size(), 1);

    // Cleaned up here: the backup directory is not among the files the cleanup
    // of this class removes.
    QVERIFY(backupDirectory.exists());
    QVERIFY(QFile::remove(backupDirectory.filePath(written.first())));

    // The failure path, which is what the reporting was put in for. The vault
    // file goes and the entry keeps pointing at it, so the copy has nothing to
    // take. A run that dropped the message here would leave the user believing
    // he holds a backup, and the password dialog points him at exactly that one.
    QVERIFY(QFile::remove(entry->filePath()));

    QMetaObject::invokeMethod(entry, "backupStorage");

    QCOMPARE(messageSpy.count(), 1);
    QVERIFY(messageSpy.takeFirst().at(0).toString().contains(QStringLiteral("no file to back up")));

    QVERIFY(backupDirectory.entryList({QStringLiteral("Privat-*")}, QDir::Files).isEmpty());
}

/**
 * The accounts of an opened storage go to the window and nowhere else. The
 * overview took the pointer it was given for one without asking, so a dialog
 * built without a window, which the tests do throughout, would have run into a
 * null pointer the moment a storage was opened through it.
 */
void StorageDialogTest::openingWithoutAWindowIsRefusedAndSaidSo()
{
    Storage storage(applicationInfo());
    storage.storeSetting(QStringLiteral("Paths"), QStringList(), QStringLiteral("Items"));

    StorageDialog dialog(&storage);
    dialog.initialize(nullptr);

    QVERIFY(dialog.createStorage(QStringLiteral("Privat"), password()));
    dialog.reload();

    auto *entry = singleEntryOf(dialog);
    QVERIFY(entry != nullptr);

    auto *field = fieldOf(entry, QStringLiteral("leStoragePassword"));
    QVERIFY(field != nullptr);
    field->setText(password());

    QSignalSpy messageSpy(&dialog, &StorageDialog::message);
    QSignalSpy openedSpy(&dialog, &StorageDialog::storageOpened);

    QMetaObject::invokeMethod(entry, "openVault");

    // Said, and nothing opened. Nothing is unlocked that cannot be shown.
    QCOMPARE(messageSpy.count(), 1);
    QVERIFY(!messageSpy.takeFirst().at(0).toString().isEmpty());
    QCOMPARE(openedSpy.count(), 0);
}

/**
 * Both buttons of an entry used to be wired in the form, which turns into a
 * SIGNAL()/SLOT() call in the generated header. A rename of either slot left the
 * build green and the buttons of every entry dead. They are wired in pointer
 * syntax now, and this holds the wiring itself.
 */
void StorageDialogTest::theButtonsOfAnEntryAreWiredToItsSlots()
{
    Storage storage(applicationInfo());
    storage.storeSetting(QStringLiteral("Paths"), QStringList(), QStringLiteral("Items"));

    StorageDialog dialog(&storage);
    dialog.initialize(nullptr);

    QVERIFY(dialog.createStorage(QStringLiteral("Privat"), password()));
    dialog.reload();

    auto *entry = singleEntryOf(dialog);
    QVERIFY(entry != nullptr);

    auto *const menuButton = entry->findChild<QPushButton *>(QStringLiteral("btnStorageMenu"));
    auto *const openButton = entry->findChild<QPushButton *>(QStringLiteral("btnOpenStorage"));

    QVERIFY(menuButton != nullptr);
    QVERIFY(openButton != nullptr);

    // The menu is built on demand, so its presence is what says the button
    // reached the slot.
    QVERIFY(entry->findChild<QMenu *>() == nullptr);

    menuButton->click();

    auto *const menu = entry->findChild<QMenu *>();
    QVERIFY(menu != nullptr);
    menu->close();

    QSignalSpy openedSpy(entry, &NewStorageItem::storageOpened);

    openButton->click();

    QCOMPARE(openedSpy.count(), 1);
}

} // namespace olbaflinx::ui::storage::tests

QTEST_MAIN(olbaflinx::ui::storage::tests::StorageDialogTest)

#include "tst_storagedialog.moc"
