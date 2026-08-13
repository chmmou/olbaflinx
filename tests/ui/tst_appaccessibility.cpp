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

#include "TestHelpers.h"

#include <QtTest/QtTest>

#include <QtGui/QAccessible>
#include <QtGui/QAccessibleInterface>
#include <QtGui/QAction>

#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QTableView>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QTreeView>

using namespace olbaflinx::core;
using namespace olbaflinx::core::logger;
using namespace olbaflinx::core::storage;
using namespace olbaflinx::ui;

namespace olbaflinx::ui::tests {

using namespace olbaflinx::core::tests;

namespace {

QStringList announcements;

void collectAnnouncement(QAccessibleEvent *event)
{
    if (event->type() == QAccessible::Announcement) {
        announcements.append(static_cast<QAccessibleAnnouncementEvent *>(event)->message());
    }
}

} // namespace

/**
 * A view says nothing about itself to an assistive tool unless it is told to.
 * Neither of the two carries a visible label that a name could be taken from, so
 * both need one of their own.
 */
class AppAccessibilityTest final : public QObject
{
    Q_OBJECT

private:
    static QAccessibleInterface *interfaceOf(QObject *object)
    {
        return QAccessible::queryAccessibleInterface(object);
    }

    static ApplicationInfo applicationInfo()
    {
        return TestHelpers::applicationInfo(QStringLiteral("OlbaFlinxAppAccessibilityTest"));
    }

    /**
     * @return The interface a tool reaches the entry through, or null when the
     *  action does not sit in the menu at all.
     *
     * An action carries no interface of its own; the menu holds one child per
     * action, in the order the actions were added.
     */
    static QAccessibleInterface *entryFor(QMenu *menu, QAction *action)
    {
        const int index = menu->actions().indexOf(action);
        if (index < 0) {
            return nullptr;
        }

        QAccessibleInterface *menuInterface = interfaceOf(menu);
        return menuInterface == nullptr ? nullptr : menuInterface->child(index);
    }

private Q_SLOTS:
    void init();
    void cleanup();

    void theAccountViewCarriesANameAndARole();
    void theTransactionViewCarriesANameAndARole();
    void everyControlOfTheFilterBarCarriesANameAndARole_data();
    void everyControlOfTheFilterBarCarriesANameAndARole();
    void everyMenuEntryCarriesANameAndARole();
    void everyToolBarEntryCarriesAName();
    void anEntryThatCannotBeInvokedSaysSo();
    void aStatusMessageIsAnnounced();
    void anEmptyStatusMessageIsNotAnnounced();
};

void AppAccessibilityTest::init()
{
    announcements.clear();
    QAccessible::installUpdateHandler(collectAnnouncement);
}

void AppAccessibilityTest::cleanup()
{
    QAccessible::installUpdateHandler(nullptr);
    announcements.clear();
}

void AppAccessibilityTest::theAccountViewCarriesANameAndARole()
{
    AppCentralWidget widget;

    auto *view = widget.accountWidget();
    QVERIFY(view != nullptr);

    auto *accessible = interfaceOf(view);
    QVERIFY(accessible != nullptr);

    QVERIFY(!accessible->text(QAccessible::Name).isEmpty());
    QCOMPARE(accessible->role(), QAccessible::Tree);
}

void AppAccessibilityTest::theTransactionViewCarriesANameAndARole()
{
    AppCentralWidget widget;

    auto *view = widget.findChild<QTableView *>(QStringLiteral("tableViewTransactions"));
    QVERIFY(view != nullptr);

    auto *accessible = interfaceOf(view);
    QVERIFY(accessible != nullptr);

    QVERIFY(!accessible->text(QAccessible::Name).isEmpty());
    QCOMPARE(accessible->role(), QAccessible::Table);
}

/**
 * The bar above the transactions carries four controls and one readout, and
 * none of them stands next to a visible label. Each says what it is and what
 * kind of thing it is.
 */
void AppAccessibilityTest::everyControlOfTheFilterBarCarriesANameAndARole_data()
{
    QTest::addColumn<QString>("objectName");
    QTest::addColumn<QAccessible::Role>("role");

    QTest::newRow("search") << QStringLiteral("lineEditTransactionSearch")
                            << QAccessible::EditableText;
    QTest::newRow("period") << QStringLiteral("comboBoxTransactionPeriod") << QAccessible::ComboBox;
    QTest::newRow("direction") << QStringLiteral("comboBoxTransactionDirection")
                               << QAccessible::ComboBox;
    QTest::newRow("reset") << QStringLiteral("pushButtonTransactionFilterReset")
                           << QAccessible::Button;
    QTest::newRow("counter") << QStringLiteral("labelTransactionCount") << QAccessible::StaticText;
}

void AppAccessibilityTest::everyControlOfTheFilterBarCarriesANameAndARole()
{
    QFETCH(QString, objectName);
    QFETCH(QAccessible::Role, role);

    AppCentralWidget widget;

    auto *control = widget.findChild<QWidget *>(objectName);
    QVERIFY(control != nullptr);

    auto *accessible = interfaceOf(control);
    QVERIFY(accessible != nullptr);

    QVERIFY(!accessible->text(QAccessible::Name).isEmpty());
    QCOMPARE(accessible->role(), role);
}

/**
 * An action carries no accessible interface of its own, so a tool never reaches
 * one directly. It reaches the entry the menu holds for it, and that entry is
 * what has to say what the command does.
 */
void AppAccessibilityTest::everyMenuEntryCarriesANameAndARole()
{
    Logger logger;
    Storage storage(applicationInfo());

    App app(&logger, &storage);
    app.initialize();

    const QList<QMenu *> menus = app.menuBar()->findChildren<QMenu *>();
    QVERIFY(!menus.isEmpty());

    for (QMenu *menu : menus) {
        QAccessibleInterface *menuInterface = interfaceOf(menu);
        QVERIFY(menuInterface != nullptr);

        QCOMPARE(menuInterface->childCount(), int(menu->actions().count()));

        for (int index = 0; index < menuInterface->childCount(); ++index) {
            QAccessibleInterface *entry = menuInterface->child(index);
            QVERIFY(entry != nullptr);

            // A separator is a child like any other and carries no name.
            if (entry->role() == QAccessible::Separator) {
                continue;
            }

            QVERIFY2(!entry->text(QAccessible::Name).isEmpty(),
                     qPrintable(QStringLiteral("entry %1 of %2 has no name")
                                    .arg(index)
                                    .arg(menu->objectName())));
            QCOMPARE(entry->role(), QAccessible::MenuItem);
        }
    }
}

/**
 * The bar holds the same commands as the menu. Each is reached through the
 * button the bar builds for it, and that button carries the text of the action.
 */
void AppAccessibilityTest::everyToolBarEntryCarriesAName()
{
    Logger logger;
    Storage storage(applicationInfo());

    App app(&logger, &storage);
    app.initialize();

    auto *toolBar = app.findChild<QToolBar *>(QStringLiteral("appToolBar"));
    QVERIFY(toolBar != nullptr);

    const QList<QAction *> actions = toolBar->actions();
    QVERIFY(!actions.isEmpty());

    for (QAction *action : actions) {
        if (action->isSeparator()) {
            continue;
        }

        QWidget *button = toolBar->widgetForAction(action);
        QVERIFY(button != nullptr);

        QAccessibleInterface *entry = interfaceOf(button);
        QVERIFY(entry != nullptr);

        QVERIFY2(!entry->text(QAccessible::Name).isEmpty(),
                 qPrintable(QStringLiteral("no name for %1").arg(action->objectName())));
    }
}

/**
 * On the overview there is no storage to close and none to set up. Both entries
 * stay in place and turn grey, and a tool has to be able to tell that the
 * command exists but does not currently apply.
 */
void AppAccessibilityTest::anEntryThatCannotBeInvokedSaysSo()
{
    Logger logger;
    Storage storage(applicationInfo());

    App app(&logger, &storage);
    app.initialize();

    auto *fileMenu = app.findChild<QMenu *>(QStringLiteral("appFileMenu"));
    QVERIFY(fileMenu != nullptr);

    auto *closeAction = app.findChild<QAction *>(QStringLiteral("appCloseStorageAction"));
    QVERIFY(closeAction != nullptr);
    QVERIFY(!closeAction->isEnabled());

    QAccessibleInterface *closeEntry = entryFor(fileMenu, closeAction);
    QVERIFY(closeEntry != nullptr);
    QVERIFY(closeEntry->state().disabled);

    // The entry next to it can be invoked, so the state says something.
    auto *quitAction = app.findChild<QAction *>(QStringLiteral("appQuitAction"));
    QVERIFY(quitAction != nullptr);

    QAccessibleInterface *quitEntry = entryFor(fileMenu, quitAction);
    QVERIFY(quitEntry != nullptr);
    QVERIFY(!quitEntry->state().disabled);
}

/**
 * The status bar swaps its text without a sound. Whoever does not look at it
 * learns nothing, and every message it carries is one the user is meant to read.
 */
void AppAccessibilityTest::aStatusMessageIsAnnounced()
{
    Logger logger;
    Storage storage(applicationInfo());

    App app(&logger, &storage);
    app.initialize();

    announcements.clear();

    const QString message = QStringLiteral("Seventeen accounts reached the storage.");
    app.showMessage(message);

    QCOMPARE(app.statusBar()->currentMessage(), message);
    QCOMPARE(announcements.count(), 1);
    QCOMPARE(announcements.first(), message);
}

/**
 * Clearing the bar travels through the same signal with an empty text. Sending
 * that on would ask the tool to announce nothing, once per page change.
 */
void AppAccessibilityTest::anEmptyStatusMessageIsNotAnnounced()
{
    Logger logger;
    Storage storage(applicationInfo());

    App app(&logger, &storage);
    app.initialize();

    app.showMessage(QStringLiteral("Something to say"));
    announcements.clear();

    app.statusBar()->clearMessage();

    QCOMPARE(app.statusBar()->currentMessage(), QString());
    QCOMPARE(announcements.count(), 0);
}

} // namespace olbaflinx::ui::tests

QTEST_MAIN(olbaflinx::ui::tests::AppAccessibilityTest)

#include "tst_appaccessibility.moc"
