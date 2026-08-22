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
#include "ui/Models/StandingOrderTableModel.h"
#include "ui/StandingOrderFetch.h"

#include "StandingOrderHelpers.h"
#include "TestHelpers.h"
#include "UiTestHelpers.h"

#include <QtTest/QtTest>

#include <QtCore/QTemporaryDir>

#include <QtGui/QAccessible>
#include <QtGui/QAccessibleInterface>
#include <QtGui/QAction>

#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QTabBar>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QTableView>

#include <chrono>
#include <functional>
#include <memory>

using namespace olbaflinx::core;
using namespace olbaflinx::core::banking;
using namespace olbaflinx::core::banking::account;
using namespace olbaflinx::core::logger;
using namespace olbaflinx::core::storage;
using namespace olbaflinx::ui;
using namespace olbaflinx::ui::models;

namespace olbaflinx::ui::tests {

using namespace olbaflinx::core::tests;

namespace {

constexpr quint32 testAccountId = 4711;

/** How many standing orders the fixture puts into the storage. */
constexpr int orderCount = 2;

QStringList announcements;

void collectAnnouncement(QAccessibleEvent *event)
{
    if (event->type() == QAccessible::Announcement) {
        announcements.append(static_cast<QAccessibleAnnouncementEvent *>(event)->message());
    }
}

/**
 * True while Tab actually stops at this widget.
 *
 * nextInFocusChain hands out the raw chain and knows nothing of visibility,
 * focus policy or a disabled parent, so the conditions Qt applies when it looks
 * for the next stop are repeated here.
 */
bool tabStopsHere(const QWidget *widget)
{
    return widget->isEnabled() && widget->isVisible()
           && (widget->focusPolicy() & Qt::TabFocus) == Qt::TabFocus
           && widget->focusProxy() == nullptr;
}

/**
 * The chain the user walks through with Tab, from the given root onwards. The
 * chain is circular and the root itself is left out.
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

int occurrencesOf(const QList<QWidget *> &chain, const QString &objectName)
{
    int count = 0;

    for (const QWidget *widget : chain) {
        if (widget->objectName() == objectName) {
            ++count;
        }
    }

    return count;
}

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

/**
 * Puts the modal question up, runs the check on it and answers it with the
 * escape key.
 *
 * A box that never comes would leave the run waiting, so the driver gives up
 * after a span of its own. A box that escape does not close is answered through
 * its default button, for the same reason.
 */
bool withTheAbortQuestion(QObject *context,
                          const std::function<void()> &trigger,
                          const std::function<void(QMessageBox *)> &check,
                          bool &escapeLedOut)
{
    bool checked = false;
    bool gaveUp = false;

    QTimer::singleShot(std::chrono::seconds(5), context, [&gaveUp] { gaveUp = true; });

    QTimer driver;
    driver.setInterval(0);

    QObject::connect(&driver, &QTimer::timeout, context, [&] {
        auto *const box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
        if (box == nullptr && !gaveUp) {
            return;
        }

        driver.stop();

        if (box == nullptr) {
            return;
        }

        if (!gaveUp) {
            check(box);
            checked = true;
        }

        QTest::keyClick(box, Qt::Key_Escape);
        escapeLedOut = !box->isVisible();

        if (!escapeLedOut) {
            box->defaultButton()->click();
        }
    });

    driver.start();
    trigger();

    return checked;
}

} // namespace

/**
 * What the standing order view says about itself to a tool that does not look
 * at the screen, and what the keyboard reaches of it.
 *
 * The three checks reach across the whole window rather than one widget: the
 * name of a tab comes from the bar that holds it, the focus chain is a property
 * of the page that stands, and the outcome of a fetch travels from the fetch to
 * the status bar before a tool hears of it.
 */
class StandingOrderAccessibilityTest final : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> workingDirectory;

    static QString password() { return TestHelpers::password(); }

    static constexpr auto workerTimeout = UiTestHelpers::workerTimeout;
    static constexpr int workerTimeoutMs = UiTestHelpers::workerTimeoutMs;

    static ApplicationInfo applicationInfo()
    {
        return TestHelpers::applicationInfo(
            QStringLiteral("OlbaFlinxStandingOrderAccessibilityTest"));
    }

    [[nodiscard]] QString storageFile() const
    {
        return workingDirectory->filePath(QStringLiteral("storage.obfx"));
    }

    [[nodiscard]] bool openStorage(Storage &storage) const
    {
        if (storage.setKey(password()).isError()) {
            return false;
        }

        storage.setStorageFile(storageFile());

        return !storage.initialize(true).isError();
    }

    /**
     * One account without online access, so that nothing here reaches for a bank.
     */
    static bool putAccount(Storage &storage)
    {
        auto map = TestHelpers::accountMapWith(testAccountId, 100.0);
        map[QStringLiteral("backend_name")] = QStringLiteral("aqnone");

        return !storage.storeItem(Account::fromMap(map).get()).isError();
    }

    static bool storeAndWait(Storage &storage,
                             const BankingItems &items,
                             const StandingOrderRun &run = {})
    {
        QSignalSpy finishedSpy(&storage, &Storage::writeFinished);

        if (storage.storeItems(items, run).isError()) {
            return false;
        }

        return !finishedSpy.isEmpty() || finishedSpy.wait(workerTimeout);
    }

    static QTabWidget *tabsOf(const App &app)
    {
        return app.findChild<QTabWidget *>(QStringLiteral("tabWidgetBanking"));
    }

    static QAction *actionOf(const App &app, const QString &name)
    {
        return app.findChild<QAction *>(name);
    }

    /**
     * Brings the window up on the second page with one account chosen and its
     * standing orders in the view.
     */
    [[nodiscard]] bool showTheOrders(App &app, Storage &storage) const
    {
        if (!UiTestHelpers::showTheWindow(app)) {
            return false;
        }

        if (!UiTestHelpers::readAccountsInto(app, storage)) {
            return false;
        }

        auto *const central = app.findChild<AppCentralWidget *>();
        auto *const treeModel = app.findChild<AccountTreeModel *>();

        if (central == nullptr || treeModel == nullptr || treeModel->rowCount() != 1) {
            return false;
        }

        central->accountWidget()->setCurrentIndex(UiTestHelpers::firstAccountOf(*treeModel));

        if (!storeAndWait(storage,
                          StandingOrderHelpers::orderRun(testAccountId, orderCount),
                          {testAccountId, true})) {
            return false;
        }

        auto *const fetch = app.findChild<StandingOrderFetch *>();
        auto *const model = app.findChild<StandingOrderTableModel *>();

        if (fetch == nullptr || model == nullptr) {
            return false;
        }

        // What the window does once a session has come back. The rows are in the
        // file at this point and the view has not been told.
        Q_EMIT fetch->ended(StandingOrderFetch::Outcome::Received, orderCount, QString());

        return QTest::qWaitFor([model] { return model->rowCount() == orderCount; }, workerTimeoutMs);
    }

private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void theViewCarriesANameAndARole();
    void theTabOfTheViewCarriesItsName();
    void everyPartOfTheNoticeCarriesANameAndARole_data();
    void everyPartOfTheNoticeCarriesANameAndARole();
    void theViewStandsInTheChainOfItsTabExactlyOnce();
    void theViewLeadsOnwards();
    void theTwoCommandsAreReachableWithoutAKeySequence();
    void theOutcomeOfAFetchIsAnnounced();
    void theQuestionOfAStoppedFetchAnswersToTheKeyboard();
};

void StandingOrderAccessibilityTest::initTestCase()
{
    // Keeps QSettings out of the real user configuration, see QStandardPaths docs.
    QStandardPaths::setTestModeEnabled(true);

    // Test mode alone puts the locations below ~/.qttest, which is a directory
    // of the user like any other and survives the run. HOME goes into a
    // temporary directory, so that nothing this binary writes outlives it.
    QVERIFY(TestHelpers::useTemporaryHome());
}

void StandingOrderAccessibilityTest::init()
{
    workingDirectory = std::make_unique<QTemporaryDir>();
    QVERIFY(workingDirectory->isValid());

    announcements.clear();
    QAccessible::installUpdateHandler(collectAnnouncement);

    // The arrangement of the areas outlives a test function and restoring one
    // moves the views in the focus chain.
    Storage settings(applicationInfo());
    settings.storeSetting(QStringLiteral("DockLayout"), QByteArray(), QStringLiteral("App"));
}

void StandingOrderAccessibilityTest::cleanup()
{
    QAccessible::installUpdateHandler(nullptr);
    announcements.clear();

    workingDirectory.reset();
}

/**
 * The view carries no visible label that a name could be taken from, so it needs
 * one of its own to be more than an unnamed table beside another.
 */
void StandingOrderAccessibilityTest::theViewCarriesANameAndARole()
{
    AppCentralWidget widget;

    auto *const view = widget.findChild<QTableView *>(QStringLiteral("tableViewStandingOrders"));
    QVERIFY(view != nullptr);

    QAccessibleInterface *accessible = QAccessible::queryAccessibleInterface(view);
    QVERIFY(accessible != nullptr);

    QVERIFY(!accessible->text(QAccessible::Name).isEmpty());
    QCOMPARE(accessible->role(), QAccessible::Table);

    // Told apart from the table beside it, which a name shared between the two
    // would not manage.
    auto *const transactions = widget.findChild<QTableView *>(
        QStringLiteral("tableViewTransactions"));
    QVERIFY(transactions != nullptr);

    QAccessibleInterface *beside = QAccessible::queryAccessibleInterface(transactions);
    QVERIFY(beside != nullptr);
    QVERIFY(accessible->text(QAccessible::Name) != beside->text(QAccessible::Name));
}

/**
 * The view sits behind a tab, and whoever cannot see the bar has to be told
 * which tab leads to it.
 */
void StandingOrderAccessibilityTest::theTabOfTheViewCarriesItsName()
{
    AppCentralWidget widget;

    auto *const tabs = widget.findChild<QTabWidget *>(QStringLiteral("tabWidgetBanking"));
    QVERIFY(tabs != nullptr);

    auto *const page = widget.findChild<QWidget *>(QStringLiteral("tabStandingOrders"));
    QVERIFY(page != nullptr);

    const int index = tabs->indexOf(page);
    QVERIFY(index >= 0);

    const QString title = tabs->tabText(index);
    QVERIFY(!title.isEmpty());

    QAccessibleInterface *bar = QAccessible::queryAccessibleInterface(tabs->tabBar());
    QVERIFY(bar != nullptr);

    QStringList names;
    for (int child = 0; child < bar->childCount(); ++child) {
        QAccessibleInterface *entry = bar->child(child);
        if (entry != nullptr) {
            names.append(entry->text(QAccessible::Name));
        }
    }

    QVERIFY2(names.contains(title),
             qPrintable(
                 QStringLiteral("%1 is not among %2").arg(title, names.join(QStringLiteral(", ")))));
}

/**
 * Where the view has nothing to show, two labels stand in its place. They carry
 * the whole answer for a tool, so neither of them may be nameless.
 */
void StandingOrderAccessibilityTest::everyPartOfTheNoticeCarriesANameAndARole_data()
{
    QTest::addColumn<QString>("objectName");

    QTest::newRow("headline") << QStringLiteral("labelStandingOrdersHeadline");
    QTest::newRow("notice") << QStringLiteral("labelStandingOrdersNotice");
}

void StandingOrderAccessibilityTest::everyPartOfTheNoticeCarriesANameAndARole()
{
    QFETCH(QString, objectName);

    AppCentralWidget widget;

    auto *const label = widget.findChild<QLabel *>(objectName);
    QVERIFY(label != nullptr);

    QAccessibleInterface *accessible = QAccessible::queryAccessibleInterface(label);
    QVERIFY(accessible != nullptr);

    QVERIFY(!accessible->text(QAccessible::Name).isEmpty());
    QCOMPARE(accessible->role(), QAccessible::StaticText);
}

/**
 * A view that Tab reaches twice would send the user through it a second time
 * before he leaves the page, and one it never reaches cannot be read at all.
 */
void StandingOrderAccessibilityTest::theViewStandsInTheChainOfItsTabExactlyOnce()
{
    Logger logger;
    Storage storage(applicationInfo());

    QVERIFY(openStorage(storage));
    QVERIFY(putAccount(storage));

    App app(&logger, &storage);
    QVERIFY(showTheOrders(app, storage));

    auto *const tabs = tabsOf(app);
    QVERIFY(tabs != nullptr);

    auto *const page = app.findChild<QWidget *>(QStringLiteral("tabStandingOrders"));
    QVERIFY(page != nullptr);

    tabs->setCurrentWidget(page);
    QTRY_VERIFY(page->isVisible());

    const QList<QWidget *> chain = tabChain(&app);

    QVERIFY2(occurrencesOf(chain, QStringLiteral("tableViewStandingOrders")) == 1,
             qPrintable(describe(chain)));

    // The table of the bookings stands on the tab beside it and is hidden while
    // this one is up, so the chain of the page holds the one view and not both.
    QVERIFY2(occurrencesOf(chain, QStringLiteral("tableViewTransactions")) == 0,
             qPrintable(describe(chain)));
}

/**
 * Tab leads out of the view again. A widget that kept the focus would need a key
 * of its own and the user would have to be told which.
 */
void StandingOrderAccessibilityTest::theViewLeadsOnwards()
{
    Logger logger;
    Storage storage(applicationInfo());

    QVERIFY(openStorage(storage));
    QVERIFY(putAccount(storage));

    App app(&logger, &storage);
    QVERIFY(showTheOrders(app, storage));

    auto *const tabs = tabsOf(app);
    QVERIFY(tabs != nullptr);

    auto *const page = app.findChild<QWidget *>(QStringLiteral("tabStandingOrders"));
    QVERIFY(page != nullptr);

    tabs->setCurrentWidget(page);
    QTRY_VERIFY(page->isVisible());

    auto *const view = app.findChild<QTableView *>(QStringLiteral("tableViewStandingOrders"));
    QVERIFY(view != nullptr);

    const QList<QWidget *> onwards = tabChain(view);

    QVERIFY(!onwards.isEmpty());
    QVERIFY(onwards.first() != view);
}

/**
 * The two commands carry no key sequence and are still reachable, over the
 * letter of their entry. What a sequence of their own would cost is a second
 * meaning for a key that already carries one.
 */
void StandingOrderAccessibilityTest::theTwoCommandsAreReachableWithoutAKeySequence()
{
    Logger logger;
    Storage storage(applicationInfo());

    App app(&logger, &storage);
    app.initialize();

    auto *const action = actionOf(app, QStringLiteral("appFetchStandingOrdersAction"));
    auto *const collective = actionOf(app, QStringLiteral("appFetchAllStandingOrdersAction"));

    QVERIFY(action != nullptr);
    QVERIFY(collective != nullptr);

    QVERIFY(action->shortcut().isEmpty());
    QVERIFY(collective->shortcut().isEmpty());

    QVERIFY(!mnemonicOf(action->text()).isNull());
    QVERIFY(!mnemonicOf(collective->text()).isNull());

    auto *const menu = app.findChild<QMenu *>(QStringLiteral("appAccountsMenu"));
    QVERIFY(menu != nullptr);

    QList<QChar> seen;
    for (const QAction *entry : menu->actions()) {
        if (entry->isSeparator()) {
            continue;
        }

        const QChar mnemonic = mnemonicOf(entry->text());
        QVERIFY2(!mnemonic.isNull(),
                 qPrintable(QStringLiteral("%1 carries no letter").arg(entry->objectName())));
        QVERIFY2(!seen.contains(mnemonic),
                 qPrintable(
                     QStringLiteral("%1 repeats the letter %2").arg(entry->objectName(), mnemonic)));

        seen.append(mnemonic);
    }
}

/**
 * The outcome of a fetch reaches the bar, and the bar swaps its text without a
 * sound. Whoever does not look at it would otherwise wait for an answer that has
 * long since arrived.
 */
void StandingOrderAccessibilityTest::theOutcomeOfAFetchIsAnnounced()
{
    Logger logger;
    Storage storage(applicationInfo());

    QVERIFY(openStorage(storage));

    App app(&logger, &storage);
    app.initialize();

    auto *const fetch = app.findChild<StandingOrderFetch *>();
    QVERIFY(fetch != nullptr);

    announcements.clear();

    Q_EMIT fetch->ended(StandingOrderFetch::Outcome::Failed, 0, QString());

    const QString message = app.statusBar()->currentMessage();
    QVERIFY(!message.isEmpty());

    QCOMPARE(announcements.count(), 1);
    QCOMPARE(announcements.first(), message);
}

/**
 * The question a stopped collective fetch puts up holds the records of the run
 * and nothing else does. Whoever cannot use a mouse has to be able to answer it,
 * and to leave it without answering.
 */
void StandingOrderAccessibilityTest::theQuestionOfAStoppedFetchAnswersToTheKeyboard()
{
    Logger logger;
    Storage storage(applicationInfo());

    QVERIFY(openStorage(storage));

    App app(&logger, &storage);
    app.initialize();

    auto *const fetch = app.findChild<StandingOrderFetch *>();
    QVERIFY(fetch != nullptr);

    bool escapeLedOut = false;

    const bool checked = withTheAbortQuestion(
        this,
        [fetch] { Q_EMIT fetch->abortNeedsAnswer(); },
        [](QMessageBox *box) {
            const QList<QAbstractButton *> buttons = box->buttons();
            QVERIFY(buttons.count() == 2);

            QList<QChar> seen;
            for (const QAbstractButton *button : buttons) {
                const QChar mnemonic = mnemonicOf(button->text());

                QVERIFY2(!mnemonic.isNull(),
                         qPrintable(
                             QStringLiteral("%1 carries no letter").arg(button->objectName())));
                QVERIFY(!seen.contains(mnemonic));

                seen.append(mnemonic);
            }
        },
        escapeLedOut);

    QVERIFY(checked);
    QVERIFY(escapeLedOut);
}

} // namespace olbaflinx::ui::tests

QTEST_MAIN(olbaflinx::ui::tests::StandingOrderAccessibilityTest)

#include "tst_standingorder_accessibility.moc"
