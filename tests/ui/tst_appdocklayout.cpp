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

#include <QtTest/QtTest>

#include <QtWidgets/QMenuBar>

#include <qtadvanceddocking-qt6/DockAreaWidget.h>
#include <qtadvanceddocking-qt6/DockManager.h>
#include <qtadvanceddocking-qt6/DockWidget.h>

using namespace olbaflinx::core;
using namespace olbaflinx::core::logger;
using namespace olbaflinx::core::storage;
using namespace olbaflinx::ui;

namespace olbaflinx::ui::tests {

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
        return {QStringLiteral("de.chm-projects.olbaflinx.test"),
                QStringLiteral("OlbaFlinxAppDockLayoutTest"),
                QStringLiteral("1.0.0")};
    }

    /**
     * Brings the window up on the page that carries the dock areas.
     *
     * The areas are laid out whatever page stands, but their geometry only means
     * something once the page they sit on is the one on show.
     */
    static void showOnBankingPage(App &app)
    {
        app.initialize();

        auto *const central = app.findChild<AppCentralWidget *>();
        QVERIFY(central != nullptr);
        central->setPage(AppCentralWidget::Page::Banking);

        app.show();
        QVERIFY(QTest::qWaitForWindowExposed(&app));
    }

private Q_SLOTS:
    void initTestCase();

    void theDefaultLayoutPutsTheAccountsLeftOfTheTransactions();
    void theAccountsSideCarriesNoWayToCloseIt();
    void noMenuEntryTogglesTheAccountsSide();
    void theWindowForcesNoMinimumSize();
    void bothSidesShowAtTheSizeTheContentAsksFor();
    void bothAreasCarryANameOfTheirOwn();
};

void AppDockLayoutTest::initTestCase()
{
    // Keeps QSettings out of the real user configuration, see QStandardPaths docs.
    QStandardPaths::setTestModeEnabled(true);
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

    const auto names = QStringList{QStringLiteral("accountDock"),
                                   QStringLiteral("transactionDock")};

    for (const QString &name : names) {
        auto *const area = manager->findDockWidget(name);
        QVERIFY2(area != nullptr, qPrintable(name));

        QVERIFY2(!area->windowTitle().isEmpty(), qPrintable(name));
        QVERIFY2(area->windowTitle() != name, qPrintable(name));
    }
}

} // namespace olbaflinx::ui::tests

QTEST_MAIN(olbaflinx::ui::tests::AppDockLayoutTest)

#include "tst_appdocklayout.moc"
