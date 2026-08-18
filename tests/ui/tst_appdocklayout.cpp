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
#include "core/Logger/Logger.h"
#include "core/Storage/Storage.h"
#include "ui/App.h"
#include "ui/AppCentralWidget.h"
#include "ui/Storage/StorageDialog.h"

#include "TestHelpers.h"

#include <QtTest/QtTest>

#include <QtWidgets/QMenuBar>
#include <QtWidgets/QStatusBar>

#include <qtadvanceddocking-qt6/DockAreaWidget.h>
#include <qtadvanceddocking-qt6/DockManager.h>
#include <qtadvanceddocking-qt6/DockWidget.h>

using namespace olbaflinx::core;
using namespace olbaflinx::core::logger;
using namespace olbaflinx::core::storage;
using namespace olbaflinx::ui;

namespace olbaflinx::ui::tests {

using namespace olbaflinx::core::tests;

/**
 * The dock areas used to sit in the source as a comment, and the window showed
 * the two views side by side in a plain layout. These tests hold the arrangement
 * that took its place.
 */
class AppDockLayoutTest final : public QObject
{
    Q_OBJECT

private:
    static ApplicationInfo applicationInfo()
    {
        return TestHelpers::applicationInfo(QStringLiteral("OlbaFlinxAppDockLayoutTest"));
    }

    /**
     * Brings the window up on the page that carries the dock areas.
     *
     * Through the overview and not by setting the page: opening a storage is what
     * puts that page up in the application, and it is the same moment the commands
     * that need one are switched on.
     */
    static void showOnBankingPage(App &app)
    {
        app.initialize();

        auto *const overview = app.findChild<storage::StorageDialog *>();
        QVERIFY(overview != nullptr);
        Q_EMIT overview->storageOpened();

        app.show();
        QVERIFY(QTest::qWaitForWindowExposed(&app));
    }

    static ads::CDockWidget *areaOf(App &app, const QString &name)
    {
        auto *const manager = app.findChild<ads::CDockManager *>();
        return manager == nullptr ? nullptr : manager->findDockWidget(name);
    }

    /**
     * True while the accounts sit left of the transactions, which is where the
     * default arrangement puts them.
     */
    static bool accountsStandLeft(App &app)
    {
        auto *const accounts = areaOf(app, QStringLiteral("accountDock"));
        auto *const transactions = areaOf(app, QStringLiteral("transactionDock"));

        if (accounts == nullptr || transactions == nullptr
            || accounts->dockAreaWidget() == nullptr) {
            return false;
        }

        return accounts->dockAreaWidget()->mapTo(&app, QPoint(0, 0)).x()
               < transactions->dockAreaWidget()->mapTo(&app, QPoint(0, 0)).x();
    }

private Q_SLOTS:
    void initTestCase();
    void init();

    void theDefaultLayoutPutsTheAccountsLeftOfTheTransactions();
    void theAccountsSideCarriesNoWayToCloseIt();
    void noMenuEntryTogglesTheAccountsSide();
    void theWindowForcesNoMinimumSize();
    void bothSidesShowAtTheSizeTheContentAsksFor();
    void bothAreasCarryANameOfTheirOwn();

    void aChangedArrangementComesBackOnTheNextStart();
    void anUnusableArrangementLeavesTheDefaultStanding();
    void anUnusableArrangementSaysSoInTheStatusBar();
    void aRestoreCannotTakeTheAccountsAway_data();
    void aRestoreCannotTakeTheAccountsAway();
    void resettingBringsTheGroupingBack();
    void aResetOutlivesTheWindow();
    void resettingWaitsForAnOpenStorage();

    void theSizeIsWrittenOnceTheWindowComesToRest();
    void aWindowClosedRightAfterAResizeKeepsItsLastSize();
};

void AppDockLayoutTest::initTestCase()
{
    // Keeps QSettings out of the real user configuration, see QStandardPaths docs.
    QStandardPaths::setTestModeEnabled(true);

    // Test mode alone puts the locations below ~/.qttest, which is a directory
    // of the user like any other and survives the run. HOME goes into a
    // temporary directory, so that nothing this binary writes outlives it.
    QVERIFY(TestHelpers::useTemporaryHome());
}

/**
 * The saved arrangement lives in the settings and outlives a single window, so it
 * would carry from one test into the next.
 *
 * The geometry goes with it. A window that was resized writes its position and
 * its size on the way out, and initialize applies both: the functions that
 * measure where the dock areas sit would otherwise start on whatever size the
 * function before them left behind.
 */
void AppDockLayoutTest::init()
{
    Storage storage(applicationInfo());
    storage.storeSetting(QStringLiteral("DockLayout"), QByteArray(), QStringLiteral("App"));
    storage.storeSetting(QStringLiteral("Position"), QPoint(), QStringLiteral("App"));
    storage.storeSetting(QStringLiteral("Size"), QSize(), QStringLiteral("App"));
}

/**
 * The arrangement the window opens with: accounts on the left, transactions in
 * the centre. Measured against where the two areas end up on screen, not against
 * the order they were built in.
 */
void AppDockLayoutTest::theDefaultLayoutPutsTheAccountsLeftOfTheTransactions()
{
    Logger logger;
    Storage storage(applicationInfo());

    App app(&logger, &storage);
    showOnBankingPage(app);

    auto *const manager = app.findChild<ads::CDockManager *>();
    QVERIFY(manager != nullptr);

    auto *const accounts = manager->findDockWidget(QStringLiteral("accountDock"));
    auto *const transactions = manager->findDockWidget(QStringLiteral("transactionDock"));

    QVERIFY(accounts != nullptr);
    QVERIFY(transactions != nullptr);

    QCOMPARE(manager->centralWidget(), transactions);

    QVERIFY(accounts->dockAreaWidget() != nullptr);
    QVERIFY(transactions->dockAreaWidget() != nullptr);

    const int accountsLeft = accounts->dockAreaWidget()->mapTo(&app, QPoint(0, 0)).x();
    const int transactionsLeft = transactions->dockAreaWidget()->mapTo(&app, QPoint(0, 0)).x();

    QVERIFY(accountsLeft < transactionsLeft);
}

/**
 * The accounts side is part of the arrangement and not something the user can
 * put away. Without the tree there is nothing left to choose an account with.
 */
void AppDockLayoutTest::theAccountsSideCarriesNoWayToCloseIt()
{
    Logger logger;
    Storage storage(applicationInfo());

    App app(&logger, &storage);
    showOnBankingPage(app);

    auto *const manager = app.findChild<ads::CDockManager *>();
    QVERIFY(manager != nullptr);

    auto *const accounts = manager->findDockWidget(QStringLiteral("accountDock"));
    QVERIFY(accounts != nullptr);

    QVERIFY(!accounts->features().testFlag(ads::CDockWidget::DockWidgetClosable));
    QVERIFY(!accounts->isClosed());
}

/**
 * The other half of the same promise. The window used to carry a checkable entry
 * under View that did exactly what the feature above forbids; it was disabled and
 * wired to nothing, and it is gone.
 */
void AppDockLayoutTest::noMenuEntryTogglesTheAccountsSide()
{
    Logger logger;
    Storage storage(applicationInfo());

    App app(&logger, &storage);
    showOnBankingPage(app);

    auto *const manager = app.findChild<ads::CDockManager *>();
    QVERIFY(manager != nullptr);

    auto *const accounts = manager->findDockWidget(QStringLiteral("accountDock"));
    QVERIFY(accounts != nullptr);

    auto *const menuBar = app.menuBar();
    QVERIFY(menuBar != nullptr);

    const auto entries = menuBar->findChildren<QAction *>();
    QVERIFY(!entries.contains(accounts->toggleViewAction()));

    for (const QAction *entry : entries) {
        QVERIFY2(entry->objectName() != QStringLiteral("appAccountsViewAction"),
                 "the entry that hid the accounts side is back");
    }
}

/**
 * The window used to force 930x646, which a narrower screen cannot go below. What
 * it needs at least is what the views inside it need, and that follows from them.
 *
 * Not compared against zero: a window whose minimum size is nobody's business
 * still ends up with one, because Qt takes the smallest size the layout can live
 * with and writes it back. That is the size asked for here. A value written into
 * the form stands next to it and no longer matches it.
 */
void AppDockLayoutTest::theWindowForcesNoMinimumSize()
{
    Logger logger;
    Storage storage(applicationInfo());

    App app(&logger, &storage);
    showOnBankingPage(app);

    QCOMPARE(app.minimumSize(), app.minimumSizeHint());
}

/**
 * And what that leaves has to be usable: both views stand there, neither is
 * squeezed to nothing.
 */
void AppDockLayoutTest::bothSidesShowAtTheSizeTheContentAsksFor()
{
    Logger logger;
    Storage storage(applicationInfo());

    App app(&logger, &storage);
    showOnBankingPage(app);

    app.resize(app.minimumSizeHint());
    QCoreApplication::processEvents();

    auto *const manager = app.findChild<ads::CDockManager *>();
    QVERIFY(manager != nullptr);

    auto *const accounts = manager->findDockWidget(QStringLiteral("accountDock"));
    auto *const transactions = manager->findDockWidget(QStringLiteral("transactionDock"));

    QVERIFY(accounts != nullptr);
    QVERIFY(transactions != nullptr);

    QVERIFY(accounts->dockAreaWidget()->width() > 0);
    QVERIFY(transactions->dockAreaWidget()->width() > 0);
}

/**
 * Each area says what it shows. The name is what an assistive tool reads out and
 * what the tab carries, and it is not the key the saved arrangement is found
 * under: that one is set apart, because a title is translated and the key must
 * not travel with the language.
 */
void AppDockLayoutTest::bothAreasCarryANameOfTheirOwn()
{
    Logger logger;
    Storage storage(applicationInfo());

    App app(&logger, &storage);
    showOnBankingPage(app);

    auto *const manager = app.findChild<ads::CDockManager *>();
    QVERIFY(manager != nullptr);

    const auto names = QStringList{QStringLiteral("accountDock"), QStringLiteral("transactionDock")};

    for (const QString &name : names) {
        auto *const area = manager->findDockWidget(name);
        QVERIFY2(area != nullptr, qPrintable(name));

        QVERIFY2(!area->windowTitle().isEmpty(), qPrintable(name));
        QVERIFY2(area->windowTitle() != name, qPrintable(name));
    }
}

/**
 * The user moves the accounts to the other edge, and that is where they are the
 * next time he opens the window.
 *
 * Measured against what is saved and read back, not against pixels. The width of
 * an area follows the size of the window, and what the library puts around the
 * areas is its business.
 */
void AppDockLayoutTest::aChangedArrangementComesBackOnTheNextStart()
{
    Logger logger;
    Storage storage(applicationInfo());

    {
        App app(&logger, &storage);
        showOnBankingPage(app);

        QVERIFY(accountsStandLeft(app));

        auto *const manager = app.findChild<ads::CDockManager *>();
        auto *const accounts = manager->findDockWidget(QStringLiteral("accountDock"));
        auto *const transactions = manager->findDockWidget(QStringLiteral("transactionDock"));

        manager->addDockWidget(ads::RightDockWidgetArea, accounts, transactions->dockAreaWidget());

        QVERIFY(!accountsStandLeft(app));
    }

    App second(&logger, &storage);
    showOnBankingPage(second);

    QVERIFY(!accountsStandLeft(second));
}

/**
 * An arrangement that cannot be applied leads to the default one, not to a window
 * the user cannot work with. What tells the two apart is the restore itself: it
 * refuses the state and changes nothing on the way.
 */
void AppDockLayoutTest::anUnusableArrangementLeavesTheDefaultStanding()
{
    Logger logger;
    Storage storage(applicationInfo());

    storage.storeSetting(QStringLiteral("DockLayout"),
                         QByteArray("this is not an arrangement"),
                         QStringLiteral("App"));

    App app(&logger, &storage);
    showOnBankingPage(app);

    QVERIFY(accountsStandLeft(app));
    QVERIFY(!areaOf(app, QStringLiteral("accountDock"))->isClosed());
}

/**
 * And the user hears about it. The window puts itself right, so there is nothing
 * for him to do; a message that says so is still better than an arrangement that
 * is quietly gone.
 *
 * It waits for the page that carries the areas. On the overview it would be a
 * word about something that is not on screen, and the step that brings the areas
 * up clears whatever the previous page left standing.
 */
void AppDockLayoutTest::anUnusableArrangementSaysSoInTheStatusBar()
{
    Logger logger;
    Storage storage(applicationInfo());

    storage.storeSetting(QStringLiteral("DockLayout"),
                         QByteArray("this is not an arrangement"),
                         QStringLiteral("App"));

    App app(&logger, &storage);
    showOnBankingPage(app);

    const QString shown = app.statusBar()->currentMessage();

    QVERIFY(!shown.isEmpty());
    QVERIFY(!shown.contains(QLatin1Char('/')));
    QVERIFY(!shown.contains(QStringLiteral("DockLayout")));

    // Said once. A storage opened a second time is no second occasion.
    app.closeStorage();

    auto *const overview = app.findChild<storage::StorageDialog *>();
    QVERIFY(overview != nullptr);
    Q_EMIT overview->storageOpened();

    QCOMPARE(app.statusBar()->currentMessage(), QString());
}

/**
 * Two ways a restore takes the accounts away, and neither of them fails.
 *
 * The saved state carries an open or closed for every area and is applied without
 * asking whether the area may be closed at all. And an area the saved state does
 * not know is taken out of its dock area and hidden until somebody switches it
 * back on, which is a state the window offers no way out of.
 */
void AppDockLayoutTest::aRestoreCannotTakeTheAccountsAway_data()
{
    QTest::addColumn<bool>("closeIt");

    QTest::newRow("saved as closed") << true;
    QTest::newRow("not in the saved state at all") << false;
}

void AppDockLayoutTest::aRestoreCannotTakeTheAccountsAway()
{
    QFETCH(bool, closeIt);

    Logger logger;
    Storage storage(applicationInfo());

    {
        App app(&logger, &storage);
        showOnBankingPage(app);

        auto *const accounts = areaOf(app, QStringLiteral("accountDock"));
        QVERIFY(accounts != nullptr);

        if (closeIt) {
            // Goes through, although the area carries no close button: the flag
            // takes the button and not the state.
            accounts->toggleView(false);
            QVERIFY(accounts->isClosed());
        }
    }

    if (!closeIt) {
        // The state is plain XML, compression is switched off. Renaming the area
        // inside it is the shortest way to a state that is valid and simply does
        // not mention the accounts, which is what an older version would leave.
        QByteArray saved = storage
                               .setting(QStringLiteral("DockLayout"),
                                        QStringLiteral("App"),
                                        QByteArray())
                               .toByteArray();

        QVERIFY(saved.contains("accountDock"));
        saved.replace("accountDock", "vanishedDock");

        storage.storeSetting(QStringLiteral("DockLayout"), saved, QStringLiteral("App"));
    }

    App second(&logger, &storage);
    showOnBankingPage(second);

    auto *const accounts = areaOf(second, QStringLiteral("accountDock"));
    QVERIFY(accounts != nullptr);

    QVERIFY(!accounts->isClosed());
    QVERIFY(accounts->dockAreaWidget() != nullptr);
}

/**
 * The way out of an arrangement the user can no longer undo himself. The restore
 * accepts such an arrangement, so falling back to the default never sees it.
 */
void AppDockLayoutTest::resettingBringsTheGroupingBack()
{
    Logger logger;
    Storage storage(applicationInfo());

    App app(&logger, &storage);
    showOnBankingPage(app);

    auto *const manager = app.findChild<ads::CDockManager *>();
    auto *const accounts = manager->findDockWidget(QStringLiteral("accountDock"));
    auto *const transactions = manager->findDockWidget(QStringLiteral("transactionDock"));

    manager->addDockWidget(ads::RightDockWidgetArea, accounts, transactions->dockAreaWidget());
    QVERIFY(!accountsStandLeft(app));

    auto *const reset = app.findChild<QAction *>(QStringLiteral("appResetLayoutAction"));
    QVERIFY(reset != nullptr);
    QVERIFY(reset->isEnabled());

    reset->trigger();

    QVERIFY(accountsStandLeft(app));
}

/**
 * And it holds. Without saving it right away the next start would hand back the
 * very arrangement the user has just left.
 */
void AppDockLayoutTest::aResetOutlivesTheWindow()
{
    Logger logger;
    Storage storage(applicationInfo());

    {
        App app(&logger, &storage);
        showOnBankingPage(app);

        auto *const manager = app.findChild<ads::CDockManager *>();
        auto *const accounts = manager->findDockWidget(QStringLiteral("accountDock"));
        auto *const transactions = manager->findDockWidget(QStringLiteral("transactionDock"));

        manager->addDockWidget(ads::RightDockWidgetArea, accounts, transactions->dockAreaWidget());

        app.findChild<QAction *>(QStringLiteral("appResetLayoutAction"))->trigger();
    }

    App second(&logger, &storage);
    showOnBankingPage(second);

    QVERIFY(accountsStandLeft(second));
}

/**
 * The areas only stand on the second page, so the entry turns grey on the first
 * rather than moving out of the menu. An assistive tool can then say that the
 * command exists and that it does not apply here.
 */
void AppDockLayoutTest::resettingWaitsForAnOpenStorage()
{
    Logger logger;
    Storage storage(applicationInfo());

    App app(&logger, &storage);
    app.initialize();

    auto *const reset = app.findChild<QAction *>(QStringLiteral("appResetLayoutAction"));
    QVERIFY(reset != nullptr);
    QVERIFY(reset->isVisible());
    QVERIFY(!reset->isEnabled());

    auto *const overview = app.findChild<storage::StorageDialog *>();
    QVERIFY(overview != nullptr);
    Q_EMIT overview->storageOpened();

    QVERIFY(reset->isEnabled());

    // And it goes back to grey on the way out.
    app.closeStorage();
    QVERIFY(!reset->isEnabled());
}

/**
 * A drag across the screen raises one resize event per step, and each of them
 * used to write two settings. The write goes to QSettings and reaches the file
 * system, in the thread that draws the window.
 *
 * It is put off until the window comes to rest instead. The first size below is
 * therefore not in the settings while the second resize is still coming.
 */
void AppDockLayoutTest::theSizeIsWrittenOnceTheWindowComesToRest()
{
    Logger logger;
    Storage storage(applicationInfo());

    App app(&logger, &storage);
    showOnBankingPage(app);

    const auto storedSize = [&storage] {
        return storage.setting(QStringLiteral("Size"), QStringLiteral("App"), QSize()).toSize();
    };

    // Showing the window raises resize events of its own, and waiting for it to
    // be exposed turns the event loop for as long as that takes. On a loaded
    // machine the span the write is put off by runs out in there, so what this
    // measures against is set here rather than taken on trust.
    storage.storeSetting(QStringLiteral("Size"), QSize(), QStringLiteral("App"));
    QCOMPARE(storedSize(), QSize());

    app.resize(QSize(820, 560));

    // The event that carries the first size goes through, so that what stands
    // below is the span and not an event that never arrived.
    QCoreApplication::processEvents();

    app.resize(QSize(840, 580));

    // Neither of the two has been written. A run that wrote per event would hold
    // the first size here.
    QCOMPARE(storedSize(), QSize());

    // And the last one arrives once the window has stood still.
    QTRY_COMPARE_WITH_TIMEOUT(storedSize(), QSize(840, 580), 5000);
}

/**
 * The window that is closed right after a drag. Its write is still pending and
 * the timer will not fire any more, so the size would be lost.
 */
void AppDockLayoutTest::aWindowClosedRightAfterAResizeKeepsItsLastSize()
{
    Logger logger;
    Storage storage(applicationInfo());

    {
        App app(&logger, &storage);
        showOnBankingPage(app);

        app.resize(QSize(880, 600));

        // The event that puts the write off has to arrive, otherwise nothing is
        // pending when the window goes and the test would pass over a window
        // that never noticed the resize at all.
        QCoreApplication::processEvents();
    }

    QCOMPARE(storage.setting(QStringLiteral("Size"), QStringLiteral("App"), QSize()).toSize(),
             QSize(880, 600));
}

} // namespace olbaflinx::ui::tests

QTEST_MAIN(olbaflinx::ui::tests::AppDockLayoutTest)

#include "tst_appdocklayout.moc"
