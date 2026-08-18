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
#include "core/Logger/Logger.h"
#include "core/Storage/Storage.h"
#include "ui/App.h"
#include "ui/AppCentralWidget.h"
#include "ui/Models/AccountTreeModel.h"
#include "ui/Models/TransactionTableModel.h"
#include "ui/Storage/NewStorageItem.h"
#include "ui/Storage/StorageDialog.h"

#include "TestHelpers.h"
#include "TransactionHelpers.h"
#include "UiTestHelpers.h"

#include <QtTest/QtTest>

#include <QtCore/QSet>
#include <QtCore/QTemporaryDir>

#include <QtGui/QAction>
#include <QtGui/QShortcut>

#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTableView>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QTreeView>

#include <algorithm>
#include <chrono>
#include <memory>

using namespace olbaflinx::core;
using namespace olbaflinx::core::banking;
using namespace olbaflinx::core::banking::account;
using namespace olbaflinx::core::logger;
using namespace olbaflinx::core::storage;
using namespace olbaflinx::ui;
using namespace olbaflinx::ui::models;
using namespace olbaflinx::ui::storage;

namespace olbaflinx::ui::tests {

using namespace olbaflinx::core::tests;

namespace {

/**
 * True while Tab actually stops at this widget.
 *
 * nextInFocusChain hands out the raw chain and knows nothing of visibility,
 * focus policy or a disabled parent. A test over the raw chain would also
 * measure what Tab never reaches and would pass a window that orders wrongly
 * for the user, so the conditions Qt itself applies when it looks for the next
 * stop are repeated here.
 */
bool tabStopsHere(const QWidget *widget)
{
    return widget->isEnabled() && widget->isVisible()
           && (widget->focusPolicy() & Qt::TabFocus) == Qt::TabFocus
           && widget->focusProxy() == nullptr;
}

/**
 * The chain the user walks through with Tab, from the given root onwards.
 *
 * The chain is circular, so the walk ends where it began. The root itself is
 * left out: it is the window or the widget under test and not a stop of its own.
 */
QList<QWidget *> tabChain(QWidget *root)
{
    QList<QWidget *> chain;

    for (QWidget *widget = root->nextInFocusChain(); widget != root;
         widget = widget->nextInFocusChain()) {
        if (tabStopsHere(widget)) {
            chain.append(widget);
        }
    }

    return chain;
}

int positionOf(const QList<QWidget *> &chain, const QString &objectName)
{
    for (int index = 0; index < chain.size(); ++index) {
        if (chain.at(index)->objectName() == objectName) {
            return index;
        }
    }

    return -1;
}

/**
 * The chain in one line, for the message of a failing comparison.
 */
QString describe(const QList<QWidget *> &chain)
{
    QStringList names;
    names.reserve(chain.size());

    for (const QWidget *widget : chain) {
        names.append(widget->objectName().isEmpty()
                         ? QString::fromLatin1(widget->metaObject()->className())
                         : widget->objectName());
    }

    return names.join(QStringLiteral(" -> "));
}

/**
 * The letter the user reaches the entry by, or a null character where the text
 * carries none. A doubled ampersand is the escaped character itself and no
 * mnemonic.
 */
QChar mnemonicOf(const QString &text)
{
    for (int index = 0; index < text.size() - 1; ++index) {
        if (text.at(index) != QLatin1Char('&')) {
            continue;
        }

        if (text.at(index + 1) == QLatin1Char('&')) {
            ++index;
            continue;
        }

        return text.at(index + 1).toUpper();
    }

    return {};
}

} // namespace

/**
 * What Tab reaches in this window, and what the keyboard reaches without it.
 * The order comes from the arrangement of the forms; the two lists that used to
 * write one of their own down are gone, and nothing in the sources sets a focus
 * policy.
 */
class AppKeyboardTest final : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> workingDirectory;

    static ApplicationInfo applicationInfo()
    {
        return TestHelpers::applicationInfo(QStringLiteral("OlbaFlinxAppKeyboardTest"));
    }

    static QString password() { return TestHelpers::password(); }

    static constexpr auto workerTimeout = UiTestHelpers::workerTimeout;
    static constexpr int workerTimeoutMs = UiTestHelpers::workerTimeoutMs;

    /**
     * How many bookings the fixture puts into the storage.
     */
    static constexpr int bookingCount = 3;

    [[nodiscard]] QString storageFile() const
    {
        return workingDirectory->filePath(QStringLiteral("storage.obfx"));
    }

    /**
     * An open storage with one account and a handful of bookings in it.
     */
    [[nodiscard]] bool fillStorage(Storage &storage) const
    {
        if (storage.setKey(password()).isError()) {
            return false;
        }

        storage.setStorageFile(storageFile());

        if (storage.initialize(true).isError()) {
            return false;
        }

        const auto account = Account::fromMap(TestHelpers::namedAccountMap());
        if (storage.storeItem(account.get()).isError()) {
            return false;
        }

        return TransactionHelpers::putTransactions(storageFile(),
                                                   password(),
                                                   4711,
                                                   bookingCount,
                                                   QStringLiteral("Buchung"));
    }

    /**
     * The state the chain is measured in: the second page, with an account
     * chosen and its bookings in the table. The window is already up.
     *
     * Nothing less will do. As long as no account is chosen the filter bar is
     * switched off and the table stands behind its notice, and a chain measured
     * there holds two stops and says nothing about the arrangement.
     */
    static bool chooseTheAccount(App &app, Storage &storage)
    {
        auto *const central = app.findChild<AppCentralWidget *>();
        auto *const treeModel = app.findChild<AccountTreeModel *>();
        auto *const transactionModel = app.findChild<TransactionTableModel *>();

        if (central == nullptr || treeModel == nullptr || transactionModel == nullptr) {
            return false;
        }

        if (!UiTestHelpers::readAccountsInto(app, storage)) {
            return false;
        }

        central->accountWidget()->setCurrentIndex(UiTestHelpers::firstAccountOf(*treeModel));

        return QTest::qWaitFor(
            [transactionModel] { return transactionModel->rowCount() == bookingCount; },
            workerTimeoutMs);
    }

private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void theChainOfAnOverviewEntryFollowsTheArrangement();
    void theChainOfTheBankingPageFollowsTheArrangement();
    void eachPageCarriesItsOwnChain();
    void theEmptyStateOfTheFilterBringsItsButtonIntoTheChain();
    void noElementHoldsTheFocus();
    void everyCommandIsReachableWithoutTheMouse();
    void theToolBarStandsOutsideTheChain();
    void noKeySequenceIsGivenTwice();
    void noMnemonicIsGivenTwiceInTheSameMenu();
    void theAccountsComeBeforeTheTransactions();
    void theAccountsStayBeforeTheTransactionsAfterARestore();
    void theKeyboardChoosesAnAccountLikeAClick();
    void theSortingIsReachableWithoutTheMouse();
    void theSortingByKeyOrdersLikeAClickOnTheHeader();
    void takingTheFocusChangesNothing();
};

void AppKeyboardTest::initTestCase()
{
    // Keeps QSettings out of the real user configuration, see QStandardPaths docs.
    QStandardPaths::setTestModeEnabled(true);

    // Test mode alone puts the locations below ~/.qttest, which is a directory
    // of the user like any other and survives the run. HOME goes into a
    // temporary directory, so that nothing this binary writes outlives it.
    QVERIFY(TestHelpers::useTemporaryHome());
}

void AppKeyboardTest::init()
{
    workingDirectory = std::make_unique<QTemporaryDir>();
    QVERIFY(workingDirectory->isValid());

    // The arrangement of the areas outlives a test function, and restoring one
    // moves the accounts in the focus chain. Every function that does not put a
    // saved arrangement there itself starts without one.
    Storage settings(applicationInfo());
    settings.storeSetting(QStringLiteral("DockLayout"), QByteArray(), QStringLiteral("App"));
}

void AppKeyboardTest::cleanup()
{
    workingDirectory.reset();
}

/**
 * The entry drew an order of its own in the form: the password field first, the
 * menu button second, and the button that opens the storage in neither place,
 * which put it behind the whole ordered chain. The arrangement reads top down
 * and left to right, and that is the order that stands now.
 */
void AppKeyboardTest::theChainOfAnOverviewEntryFollowsTheArrangement()
{
    Storage storage(applicationInfo());

    NewStorageItem item(&storage);
    item.show();
    QVERIFY(QTest::qWaitForWindowExposed(&item));

    const QList<QWidget *> chain = tabChain(&item);

    const int menu = positionOf(chain, QStringLiteral("btnStorageMenu"));
    const int field = positionOf(chain, QStringLiteral("leStoragePassword"));
    const int open = positionOf(chain, QStringLiteral("btnOpenStorage"));

    QVERIFY2(menu >= 0 && field >= 0 && open >= 0, qPrintable(describe(chain)));
    QVERIFY2(menu < field && field < open, qPrintable(describe(chain)));
}

/**
 * The bar above the transactions reads from left to right and the table stands
 * below it. Nothing writes that order down; it is the one the form draws.
 */
void AppKeyboardTest::theChainOfTheBankingPageFollowsTheArrangement()
{
    Logger logger;
    Storage storage(applicationInfo());

    QVERIFY(fillStorage(storage));

    App app(&logger, &storage);
    QVERIFY(UiTestHelpers::showTheWindow(app));
    QVERIFY(chooseTheAccount(app, storage));

    // From where the page puts the keyboard, not from the window. The chain is
    // a ring; walking it from the window says nothing about the order the user
    // meets, because the window is not where he starts.
    QWidget *const start = app.focusWidget();
    QVERIFY(start != nullptr);

    QList<QWidget *> chain = tabChain(start);
    chain.prepend(start);

    const QStringList expected{QStringLiteral("treeViewBankingAccounts"),
                               QStringLiteral("lineEditTransactionSearch"),
                               QStringLiteral("comboBoxTransactionPeriod"),
                               QStringLiteral("comboBoxTransactionDirection"),
                               QStringLiteral("pushButtonTransactionFilterReset"),
                               QStringLiteral("tableViewTransactions")};

    int previous = -1;
    for (const QString &name : expected) {
        const int position = positionOf(chain, name);

        QVERIFY2(position >= 0,
                 qPrintable(
                     QStringLiteral("%1 is not in the chain: %2").arg(name, describe(chain))));
        QVERIFY2(position > previous,
                 qPrintable(QStringLiteral("%1 comes too early in: %2").arg(name, describe(chain))));

        previous = position;
    }

    // A widget appears once in a circular chain by construction. What could put
    // one in twice is a second window whose chain has been spliced into this one.
    const QSet<QWidget *> seen(chain.cbegin(), chain.cend());
    QCOMPARE(seen.size(), chain.size());
}

/**
 * What is not on the page that stands is not in the chain either, and no
 * mechanism is needed for that. The order is held per state the window takes,
 * not over their sum.
 */
void AppKeyboardTest::eachPageCarriesItsOwnChain()
{
    Logger logger;
    Storage storage(applicationInfo());

    QVERIFY(fillStorage(storage));

    App app(&logger, &storage);
    QVERIFY(UiTestHelpers::showTheWindow(app));

    const QString overviewButton = QStringLiteral("btnNewStorageItem");
    const QString searchField = QStringLiteral("lineEditTransactionSearch");

    const QList<QWidget *> onTheOverview = tabChain(&app);
    QVERIFY2(positionOf(onTheOverview, overviewButton) >= 0, qPrintable(describe(onTheOverview)));
    QCOMPARE(positionOf(onTheOverview, searchField), -1);

    QVERIFY(chooseTheAccount(app, storage));

    const QList<QWidget *> onTheBankingPage = tabChain(&app);
    QVERIFY2(positionOf(onTheBankingPage, searchField) >= 0, qPrintable(describe(onTheBankingPage)));
    QCOMPARE(positionOf(onTheBankingPage, overviewButton), -1);
}

/**
 * A filter that matches nothing puts a button in place of the table. It is the
 * only way out of that state without the mouse, so it has to be in the chain
 * while it stands and gone once the table is back.
 */
void AppKeyboardTest::theEmptyStateOfTheFilterBringsItsButtonIntoTheChain()
{
    Logger logger;
    Storage storage(applicationInfo());

    QVERIFY(fillStorage(storage));

    App app(&logger, &storage);
    QVERIFY(UiTestHelpers::showTheWindow(app));
    QVERIFY(chooseTheAccount(app, storage));

    const QString reset = QStringLiteral("pushButtonTransactionsNoticeReset");
    QCOMPARE(positionOf(tabChain(&app), reset), -1);

    auto *const search = app.findChild<QLineEdit *>(QStringLiteral("lineEditTransactionSearch"));
    QVERIFY(search != nullptr);
    search->setText(QStringLiteral("nothing matches this"));

    auto *const transactionModel = app.findChild<TransactionTableModel *>();
    QVERIFY(transactionModel != nullptr);
    QTRY_COMPARE(transactionModel->rowCount(), 0);

    QVERIFY2(positionOf(tabChain(&app), reset) >= 0, qPrintable(describe(tabChain(&app))));
}

/**
 * Tab leads out of every stop. A widget that kept it would need a key of its
 * own and the user would have to be told which; this window carries no such
 * element, and the test is here for the day one is added.
 */
void AppKeyboardTest::noElementHoldsTheFocus()
{
    Logger logger;
    Storage storage(applicationInfo());

    QVERIFY(fillStorage(storage));

    App app(&logger, &storage);
    QVERIFY(UiTestHelpers::showTheWindow(app));
    QVERIFY(chooseTheAccount(app, storage));

    const QList<QWidget *> chain = tabChain(&app);
    QVERIFY(chain.size() > 1);

    for (QWidget *widget : chain) {
        const QList<QWidget *> onwards = tabChain(widget);

        QVERIFY2(!onwards.isEmpty(),
                 qPrintable(QStringLiteral("%1 leads nowhere").arg(widget->objectName())));
        QVERIFY2(onwards.first() != widget,
                 qPrintable(QStringLiteral("%1 leads to itself").arg(widget->objectName())));
    }
}

/**
 * Every command of the menu carries a mnemonic or a shortcut, and the tool bar
 * holds no command of its own. What is measured is the function, so one way is
 * enough.
 */
void AppKeyboardTest::everyCommandIsReachableWithoutTheMouse()
{
    Logger logger;
    Storage storage(applicationInfo());

    App app(&logger, &storage);
    app.initialize();

    const QList<QMenu *> menus = app.menuBar()->findChildren<QMenu *>();
    QVERIFY(!menus.isEmpty());

    QList<QAction *> inTheMenus;

    for (const QMenu *menu : menus) {
        QVERIFY2(!mnemonicOf(menu->title()).isNull(),
                 qPrintable(QStringLiteral("the menu %1 has no mnemonic").arg(menu->objectName())));

        const QList<QAction *> actions = menu->actions();
        QVERIFY(!actions.isEmpty());

        for (QAction *action : actions) {
            if (action->isSeparator()) {
                continue;
            }

            QVERIFY2(!mnemonicOf(action->text()).isNull() || !action->shortcut().isEmpty(),
                     qPrintable(QStringLiteral("%1 is reachable with the mouse alone")
                                    .arg(action->objectName())));

            inTheMenus.append(action);
        }
    }

    auto *const toolBar = app.findChild<QToolBar *>(QStringLiteral("appToolBar"));
    QVERIFY(toolBar != nullptr);

    const QList<QAction *> onTheBar = toolBar->actions();
    QVERIFY(!onTheBar.isEmpty());

    for (QAction *action : onTheBar) {
        if (action->isSeparator()) {
            continue;
        }

        // The same object, not a second one carrying the same text. A command
        // that stood on the bar alone would have no way without the mouse.
        QVERIFY2(inTheMenus.contains(action),
                 qPrintable(QStringLiteral("%1 stands on the bar alone").arg(action->objectName())));
    }
}

/**
 * The bar comes with the second page, and Tab does not reach it either way:
 * QToolBar gives the button it builds for an action Qt::NoFocus. That is what
 * makes the check above the one that matters, and it is why the bar needs no
 * place of its own in the order.
 */
void AppKeyboardTest::theToolBarStandsOutsideTheChain()
{
    Logger logger;
    Storage storage(applicationInfo());

    QVERIFY(fillStorage(storage));

    App app(&logger, &storage);
    QVERIFY(UiTestHelpers::showTheWindow(app));
    QVERIFY(chooseTheAccount(app, storage));

    auto *const toolBar = app.findChild<QToolBar *>(QStringLiteral("appToolBar"));
    QVERIFY(toolBar != nullptr);
    QVERIFY(toolBar->isVisible());

    const QList<QToolButton *> buttons = toolBar->findChildren<QToolButton *>();
    QVERIFY(!buttons.isEmpty());

    for (const QToolButton *button : buttons) {
        QCOMPARE(button->focusPolicy(), Qt::NoFocus);
    }

    const QList<QWidget *> chain = tabChain(&app);
    const bool reachesTheBar = std::any_of(chain.cbegin(),
                                           chain.cend(),
                                           [toolBar](const QWidget *widget) {
                                               return toolBar->isAncestorOf(widget);
                                           });

    QVERIFY2(!reachesTheBar, qPrintable(describe(chain)));
}

/**
 * Two widgets on one sequence make it ambiguous, and then neither of them
 * fires. It happened once: the shortcut for a new storage lay on the button of
 * the overview and on the menu entry at the same time.
 */
void AppKeyboardTest::noKeySequenceIsGivenTwice()
{
    Logger logger;
    Storage storage(applicationInfo());

    QVERIFY(fillStorage(storage));

    App app(&logger, &storage);
    QVERIFY(UiTestHelpers::showTheWindow(app));
    QVERIFY(chooseTheAccount(app, storage));

    QList<QKeySequence> seen;

    const QList<QAction *> actions = app.findChildren<QAction *>();
    for (const QAction *action : actions) {
        const QList<QKeySequence> shortcuts = action->shortcuts();

        for (const QKeySequence &shortcut : shortcuts) {
            QVERIFY2(!seen.contains(shortcut),
                     qPrintable(QStringLiteral("%1 is given twice, the second time to %2")
                                    .arg(shortcut.toString(), action->objectName())));
            seen.append(shortcut);
        }
    }

    const QList<QShortcut *> shortcuts = app.findChildren<QShortcut *>();
    for (const QShortcut *shortcut : shortcuts) {
        QVERIFY2(!seen.contains(shortcut->key()),
                 qPrintable(QStringLiteral("%1 is given twice, the second time to a shortcut")
                                .arg(shortcut->key().toString())));
        seen.append(shortcut->key());
    }
}

/**
 * A mnemonic reaches only as far as the menu it stands in, so it is the menu
 * that has to be free of a second one.
 */
void AppKeyboardTest::noMnemonicIsGivenTwiceInTheSameMenu()
{
    Logger logger;
    Storage storage(applicationInfo());

    App app(&logger, &storage);
    app.initialize();

    const QList<QMenu *> menus = app.menuBar()->findChildren<QMenu *>();
    QVERIFY(!menus.isEmpty());

    QList<QChar> inTheBar;
    for (const QMenu *menu : menus) {
        const QChar letter = mnemonicOf(menu->title());

        QVERIFY2(!inTheBar.contains(letter),
                 qPrintable(QStringLiteral("the menu bar carries %1 twice").arg(letter)));
        inTheBar.append(letter);

        QList<QChar> inTheMenu;
        const QList<QAction *> actions = menu->actions();

        for (const QAction *action : actions) {
            const QChar entry = mnemonicOf(action->text());
            if (entry.isNull()) {
                continue;
            }

            QVERIFY2(!inTheMenu.contains(entry),
                     qPrintable(QStringLiteral("%1 carries %2 twice")
                                    .arg(menu->objectName(), QString(entry))));
            inTheMenu.append(entry);
        }
    }
}

/**
 * The two areas are built in the source and have no form an arrangement could
 * be read from. In the default arrangement the accounts stand left of the
 * transactions, so that is where the keyboard has to begin. An arrangement the
 * user has moved is not followed.
 *
 * Which of the two the chain holds first cannot be read off the ring: the ring
 * has no beginning. What decides it is the widget the page hands the keyboard
 * to, and the page says so rather than leaving it to the dock manager, whose
 * first area has to be the central one.
 */
void AppKeyboardTest::theAccountsComeBeforeTheTransactions()
{
    Logger logger;
    Storage storage(applicationInfo());

    QVERIFY(fillStorage(storage));

    App app(&logger, &storage);
    QVERIFY(UiTestHelpers::showTheWindow(app));
    QVERIFY(chooseTheAccount(app, storage));

    auto *const central = app.findChild<AppCentralWidget *>();
    QVERIFY(central != nullptr);

    QCOMPARE(app.focusWidget(), central->accountWidget());

    const QList<QWidget *> onwards = tabChain(app.focusWidget());
    QVERIFY2(positionOf(onwards, QStringLiteral("lineEditTransactionSearch")) >= 0,
             qPrintable(describe(onwards)));
}

/**
 * The same for the second start and every one after it.
 *
 * A restore hands the areas to the manager again and rebuilds the ring. The
 * arrangement that comes back here is the default one, and the keyboard has to
 * begin at the accounts there too.
 */
void AppKeyboardTest::theAccountsStayBeforeTheTransactionsAfterARestore()
{
    Logger logger;
    Storage storage(applicationInfo());

    QVERIFY(fillStorage(storage));

    // The arrangement of a first run, written the way the window writes it.
    {
        App first(&logger, &storage);
        QVERIFY(UiTestHelpers::showTheWindow(first));
        QVERIFY(chooseTheAccount(first, storage));

        first.close();
    }

    QVERIFY(!storage.setting(QStringLiteral("DockLayout"), QStringLiteral("App"), QByteArray())
                 .toByteArray()
                 .isEmpty());

    App second(&logger, &storage);
    QVERIFY(UiTestHelpers::showTheWindow(second));
    QVERIFY(chooseTheAccount(second, storage));

    auto *const central = second.findChild<AppCentralWidget *>();
    QVERIFY(central != nullptr);

    QCOMPARE(second.focusWidget(), central->accountWidget());

    const QList<QWidget *> onwards = tabChain(second.focusWidget());
    QVERIFY2(positionOf(onwards, QStringLiteral("lineEditTransactionSearch")) >= 0,
             qPrintable(describe(onwards)));
}

/**
 * The selection hangs on the current index, so an arrow key carries it as a
 * click does. Nothing here is a change of context: the view beside it fills,
 * the window stays where it is and nothing is sent off.
 */
void AppKeyboardTest::theKeyboardChoosesAnAccountLikeAClick()
{
    Logger logger;
    Storage storage(applicationInfo());

    QVERIFY(fillStorage(storage));

    App app(&logger, &storage);
    app.initialize();

    auto *const treeModel = app.findChild<AccountTreeModel *>();
    auto *const transactionModel = app.findChild<TransactionTableModel *>();
    auto *const central = app.findChild<AppCentralWidget *>();
    QVERIFY(treeModel != nullptr);
    QVERIFY(transactionModel != nullptr);
    QVERIFY(central != nullptr);

    QVERIFY(UiTestHelpers::readAccountsInto(app, storage));

    app.show();
    QVERIFY(QTest::qWaitForWindowExposed(&app));

    auto *const view = central->accountWidget();
    view->expandAll();
    view->setCurrentIndex(treeModel->index(0, 0));
    view->setFocus();

    // The bank groups its accounts and is no choice of one.
    QCOMPARE(transactionModel->accountId(), 0u);

    // Down from the bank onto the account below it, the step a click takes.
    QTest::keyClick(view, Qt::Key_Down);

    QCOMPARE(view->currentIndex(), UiTestHelpers::firstAccountOf(*treeModel));
    QCOMPARE(transactionModel->accountId(), 4711u);
    QTRY_COMPARE_WITH_TIMEOUT(transactionModel->rowCount(), bookingCount, workerTimeoutMs);
}

/**
 * The header is the only place the window used to offer the ordering at, and a
 * QHeaderView takes no keyboard focus of its own. The second way sits on the
 * table as an action, which puts it on a key sequence and into the menu the
 * context key opens.
 */
void AppKeyboardTest::theSortingIsReachableWithoutTheMouse()
{
    Logger logger;
    Storage storage(applicationInfo());

    App app(&logger, &storage);
    app.initialize();

    auto *const view = app.findChild<QTableView *>(QStringLiteral("tableViewTransactions"));
    QVERIFY(view != nullptr);

    QCOMPARE(view->horizontalHeader()->focusPolicy(), Qt::NoFocus);

    auto *const action = view->findChild<QAction *>(QStringLiteral("actionSortTransactions"));
    QVERIFY(action != nullptr);
    QVERIFY(!action->shortcut().isEmpty());

    // The context key reaches the widget that has the focus, and the view lists
    // its own actions there.
    QCOMPARE(view->contextMenuPolicy(), Qt::ActionsContextMenu);
    QVERIFY(view->actions().contains(action));
}

/**
 * Both ways set the same indicator, and the ordering follows from it. The key
 * turns the order around on a column that already carries the indicator, which
 * is what a second click on the header does.
 */
void AppKeyboardTest::theSortingByKeyOrdersLikeAClickOnTheHeader()
{
    Logger logger;
    Storage storage(applicationInfo());

    QVERIFY(fillStorage(storage));

    App app(&logger, &storage);
    QVERIFY(UiTestHelpers::showTheWindow(app));
    QVERIFY(chooseTheAccount(app, storage));

    auto *const transactionModel = app.findChild<TransactionTableModel *>();
    auto *const view = app.findChild<QTableView *>(QStringLiteral("tableViewTransactions"));
    QVERIFY(transactionModel != nullptr);
    QVERIFY(view != nullptr);

    auto *const action = view->findChild<QAction *>(QStringLiteral("actionSortTransactions"));
    QVERIFY(action != nullptr);

    auto *const header = view->horizontalHeader();
    const int column = header->sortIndicatorSection() == 0 ? 1 : 0;

    view->setCurrentIndex(transactionModel->index(0, column));
    action->trigger();

    QCOMPARE(header->sortIndicatorSection(), column);
    QCOMPARE(header->sortIndicatorOrder(), Qt::AscendingOrder);
    QCOMPARE(int(transactionModel->sortColumn()), column);
    QCOMPARE(transactionModel->sortOrder(), Qt::AscendingOrder);

    // Again on the same column, the way a second click turns it around.
    action->trigger();

    QCOMPARE(header->sortIndicatorSection(), column);
    QCOMPARE(header->sortIndicatorOrder(), Qt::DescendingOrder);
    QCOMPARE(transactionModel->sortOrder(), Qt::DescendingOrder);
}

/**
 * Arriving with Tab is no input. The page stays the one that stands, no window
 * opens and the account whose bookings are shown does not change.
 *
 * The loading of the transactions that a chosen account brings on is none of
 * those three: the view beside it fills, and that is what the arrow key is
 * meant to do.
 */
void AppKeyboardTest::takingTheFocusChangesNothing()
{
    Logger logger;
    Storage storage(applicationInfo());

    QVERIFY(fillStorage(storage));

    App app(&logger, &storage);
    QVERIFY(UiTestHelpers::showTheWindow(app));
    QVERIFY(chooseTheAccount(app, storage));

    auto *const central = app.findChild<AppCentralWidget *>();
    auto *const transactionModel = app.findChild<TransactionTableModel *>();
    QVERIFY(central != nullptr);
    QVERIFY(transactionModel != nullptr);

    const AppCentralWidget::Page page = central->page();
    const qsizetype windows = QApplication::topLevelWidgets().size();
    const quint32 account = transactionModel->accountId();

    const QList<QWidget *> chain = tabChain(&app);
    QVERIFY(!chain.isEmpty());

    for (QWidget *widget : chain) {
        widget->setFocus(Qt::TabFocusReason);
        QCoreApplication::processEvents();

        QVERIFY2(central->page() == page,
                 qPrintable(QStringLiteral("%1 turned the page").arg(widget->objectName())));
        QVERIFY2(QApplication::topLevelWidgets().size() == windows,
                 qPrintable(QStringLiteral("%1 opened a window").arg(widget->objectName())));
        QVERIFY2(transactionModel->accountId() == account,
                 qPrintable(QStringLiteral("%1 changed the account").arg(widget->objectName())));
    }
}

} // namespace olbaflinx::ui::tests

QTEST_MAIN(olbaflinx::ui::tests::AppKeyboardTest)

#include "tst_appkeyboard.moc"
