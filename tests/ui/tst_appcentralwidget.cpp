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

#include "ui/AppCentralWidget.h"

#include "core/ApplicationInfo.h"
#include "core/Banking/Account/Account.h"
#include "core/Logger/Logger.h"
#include "core/Storage/Storage.h"
#include "ui/App.h"
#include "ui/Models/AccountTreeModel.h"
#include "ui/Models/StandingOrderTableModel.h"
#include "ui/Models/TransactionTableModel.h"
#include "ui/Storage/StorageDialog.h"

#include "TestHelpers.h"
#include "TransactionHelpers.h"
#include "UiTestHelpers.h"

#include <QtTest/QtTest>

#include <QtCore/QTemporaryDir>

#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QTableView>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QTreeView>

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

/**
 * The central area used to be a single page with two widgets on fixed
 * rectangles. It carries two pages now, and the window switches between them
 * when a storage is opened or closed.
 */
class AppCentralWidgetTest final : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> workingDirectory;

    static QStackedWidget *pagesOf(const AppCentralWidget &widget)
    {
        return widget.findChild<QStackedWidget *>(QStringLiteral("stackedWidgetPages"));
    }

    static QStackedWidget *accountPagesOf(const AppCentralWidget &widget)
    {
        return widget.findChild<QStackedWidget *>(QStringLiteral("stackedWidgetAccounts"));
    }

    static QLabel *accountNoticeOf(const AppCentralWidget &widget)
    {
        return widget.findChild<QLabel *>(QStringLiteral("labelAccountsNotice"));
    }

    static QStackedWidget *transactionPagesOf(const AppCentralWidget &widget)
    {
        return widget.findChild<QStackedWidget *>(QStringLiteral("stackedWidgetTransactions"));
    }

    static QLabel *transactionHeadlineOf(const AppCentralWidget &widget)
    {
        return widget.findChild<QLabel *>(QStringLiteral("labelTransactionsHeadline"));
    }

    static QLabel *transactionNoticeOf(const AppCentralWidget &widget)
    {
        return widget.findChild<QLabel *>(QStringLiteral("labelTransactionsNotice"));
    }

    static QStackedWidget *standingOrderPagesOf(const AppCentralWidget &widget)
    {
        return widget.findChild<QStackedWidget *>(QStringLiteral("stackedWidgetStandingOrders"));
    }

    static QLabel *standingOrderHeadlineOf(const AppCentralWidget &widget)
    {
        return widget.findChild<QLabel *>(QStringLiteral("labelStandingOrdersHeadline"));
    }

    static QLabel *standingOrderNoticeOf(const AppCentralWidget &widget)
    {
        return widget.findChild<QLabel *>(QStringLiteral("labelStandingOrdersNotice"));
    }

    static QTableView *standingOrderViewOf(const AppCentralWidget &widget)
    {
        return widget.findChild<QTableView *>(QStringLiteral("tableViewStandingOrders"));
    }

    static QWidget *filterBarOf(const AppCentralWidget &widget)
    {
        return widget.findChild<QWidget *>(QStringLiteral("widgetTransactionFilter"));
    }

    static QLineEdit *searchFieldOf(const AppCentralWidget &widget)
    {
        return widget.findChild<QLineEdit *>(QStringLiteral("lineEditTransactionSearch"));
    }

    static QLabel *counterOf(const AppCentralWidget &widget)
    {
        return widget.findChild<QLabel *>(QStringLiteral("labelTransactionCount"));
    }

    static QPushButton *noticeResetOf(const AppCentralWidget &widget)
    {
        return widget.findChild<QPushButton *>(QStringLiteral("pushButtonTransactionsNoticeReset"));
    }

    static QString password() { return TestHelpers::password(); }

    static constexpr auto workerTimeout = UiTestHelpers::workerTimeout;
    static constexpr int workerTimeoutMs = UiTestHelpers::workerTimeoutMs;

    [[nodiscard]] bool openStorage(Storage &storage) const
    {
        if (storage.setKey(password()).isError()) {
            return false;
        }

        storage.setStorageFile(storageFile());

        return !storage.initialize(true).isError();
    }

    [[nodiscard]] QString storageFile() const
    {
        return workingDirectory->filePath(QStringLiteral("storage.obfx"));
    }

    static ApplicationInfo applicationInfo()
    {
        // Through the factory the other tests use. Written out here, this one
        // silently left the field that was added to the struct empty.
        return TestHelpers::applicationInfo(QStringLiteral("OlbaFlinxAppCentralWidgetTest"));
    }

    static QAction *actionOf(const App &app, const QString &name)
    {
        return app.findChild<QAction *>(name);
    }

private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void theEmptyTransactionViewNamesItsReason();
    void theEmptyStandingOrderViewNamesItsReason();
    void theStandingOrderViewSitsInTheTabThatWasKeptForIt();
    void choosingAnAccountShowsItsTransactionsAndABankClearsThem();
    void anAccountThatBecomesInactiveTakesTheSelectionWithIt();
    void openingAStorageLeavesNoAccountSelected();
    void aStorageWithoutAccountsSaysSoWithoutAMessage();
    void anAccountWithoutTransactionsSaysSoWithoutAMessage();
    void theFilterBarStandsAndWaitsWhileNoAccountIsChosen();
    void aFilterWithoutAMatchGetsAnEmptyStateOfItsOwnWithAButton();
    void theCounterNamesWhatTheFilterLeaves();
    void closingTheStorageTakesTheFilterWithIt();
    void closingTheStorageTakesTheSortIndicatorBackWithIt();

    void theAccountViewIsThereAndCarriesTheModel();
    void anEmptyModelPutsTheNoticeInPlaceOfTheTree();
    void anEntryCarriesTheAccountNameTheIbanAndTheBalance();
    void aFailedReadDoesNotLookLikeAnEmptyStorage();
    void aReadThatOutlivesItsStorageReachesNoView();

    void startsOnTheStorageOverview();
    void switchingKeepsBothPagesAlive();
    void switchingToThePageAlreadyShownChangesNothing();
    void closingAStorageReturnsToTheOverviewAndDropsTheAccounts();
    void closingAStorageWithoutOneOpenIsHarmless();
    void theCommandsThatNeedAStorageWaitForOne();
    void theEntriesWithoutTheirStoryStayDisabled();
    void aMessageFromTheOverviewReachesTheStatusBar();
    void leavingAPageTakesItsMessageWithIt();
    void theOverviewDoesNotForceASizeOnTheWindow();
};

void AppCentralWidgetTest::initTestCase()
{
    // Keeps QSettings out of the real user configuration, see QStandardPaths docs.
    QStandardPaths::setTestModeEnabled(true);

    // Test mode alone puts the locations below ~/.qttest, which is a directory
    // of the user like any other and survives the run. HOME goes into a
    // temporary directory, so that nothing this binary writes outlives it.
    QVERIFY(TestHelpers::useTemporaryHome());
}

void AppCentralWidgetTest::init()
{
    workingDirectory = std::make_unique<QTemporaryDir>();
    QVERIFY(workingDirectory->isValid());
}

void AppCentralWidgetTest::cleanup()
{
    workingDirectory.reset();
}

/**
 * An empty transaction view says why it is empty. Three states, and a reader has
 * to be able to tell them apart without knowing the code.
 *
 * Choosing a bank shares its headline with choosing nothing at all, because it
 * is no choice of an account either. What it carries of its own is the
 * explanation.
 */
void AppCentralWidgetTest::theEmptyTransactionViewNamesItsReason()
{
    AppCentralWidget widget;
    TransactionTableModel model;

    widget.setTransactionModel(&model);

    auto *pages = transactionPagesOf(widget);
    auto *headline = transactionHeadlineOf(widget);
    auto *notice = transactionNoticeOf(widget);

    QVERIFY(pages != nullptr);
    QVERIFY(headline != nullptr);
    QVERIFY(notice != nullptr);

    widget.setTransactionNotice(AppCentralWidget::TransactionNotice::NoAccountSelected);

    QCOMPARE(pages->currentWidget(), notice->parentWidget());
    QVERIFY(!headline->text().isEmpty());
    QVERIFY(!notice->text().isEmpty());

    const QString headlineWithoutAnAccount = headline->text();
    const QString noticeWithoutAnAccount = notice->text();

    widget.setTransactionNotice(AppCentralWidget::TransactionNotice::BankSelected);

    QCOMPARE(headline->text(), headlineWithoutAnAccount);
    QVERIFY(notice->text() != noticeWithoutAnAccount);
    QVERIFY(!notice->text().isEmpty());

    widget.setTransactionNotice(AppCentralWidget::TransactionNotice::AccountWithoutTransactions);

    QVERIFY(headline->text() != headlineWithoutAnAccount);
    QVERIFY(notice->text() != noticeWithoutAnAccount);
}

/**
 * One click on an account shows its transactions. A click on the bank above it
 * is no choice of an account, so the list of the account before does not stay
 * standing under it.
 */
void AppCentralWidgetTest::theEmptyStandingOrderViewNamesItsReason()
{
    AppCentralWidget widget;
    StandingOrderTableModel model;

    widget.setStandingOrderModel(&model);

    auto *pages = standingOrderPagesOf(widget);
    auto *headline = standingOrderHeadlineOf(widget);
    auto *notice = standingOrderNoticeOf(widget);

    QVERIFY(pages != nullptr);
    QVERIFY(headline != nullptr);
    QVERIFY(notice != nullptr);

    widget.setStandingOrderNotice(AppCentralWidget::StandingOrderNotice::NoAccountSelected);

    QCOMPARE(pages->currentWidget(), notice->parentWidget());
    QVERIFY(!headline->text().isEmpty());
    QVERIFY(!notice->text().isEmpty());

    const QString headlineWithoutAnAccount = headline->text();
    const QString noticeWithoutAnAccount = notice->text();

    widget.setStandingOrderNotice(AppCentralWidget::StandingOrderNotice::BankSelected);

    QCOMPARE(headline->text(), headlineWithoutAnAccount);
    QVERIFY(notice->text() != noticeWithoutAnAccount);
    QVERIFY(!notice->text().isEmpty());

    widget.setStandingOrderNotice(
        AppCentralWidget::StandingOrderNotice::AccountWithoutStandingOrders);

    QVERIFY(headline->text() != headlineWithoutAnAccount);
    QVERIFY(notice->text() != noticeWithoutAnAccount);
}

void AppCentralWidgetTest::theStandingOrderViewSitsInTheTabThatWasKeptForIt()
{
    AppCentralWidget widget;

    auto *const tabs = widget.findChild<QTabWidget *>(QStringLiteral("tabWidgetBanking"));
    auto *const tab = widget.findChild<QWidget *>(QStringLiteral("tabStandingOrders"));
    auto *const view = standingOrderViewOf(widget);

    QVERIFY(tabs != nullptr);
    QVERIFY(tab != nullptr);
    QVERIFY(view != nullptr);

    // The view is inside that tab and the tab is one of the tab widget's, so
    // nothing of it lives in an area of its own.
    QVERIFY(tabs->indexOf(tab) >= 0);
    QVERIFY(tab->isAncestorOf(view));
}

void AppCentralWidgetTest::choosingAnAccountShowsItsTransactionsAndABankClearsThem()
{
    Logger logger;
    Storage storage(applicationInfo());

    QVERIFY(openStorage(storage));

    const auto account = Account::fromMap(TestHelpers::namedAccountMap());
    QVERIFY(!storage.storeItem(account.get()).isError());
    QVERIFY(TransactionHelpers::putTransactions(storageFile(),
                                                password(),
                                                4711,
                                                3,
                                                QStringLiteral("Buchung")));

    App app(&logger, &storage);
    app.initialize();

    auto *central = app.findChild<AppCentralWidget *>();
    auto *treeModel = app.findChild<AccountTreeModel *>();
    auto *transactionModel = app.findChild<TransactionTableModel *>();

    QVERIFY(central != nullptr);
    QVERIFY(treeModel != nullptr);
    QVERIFY(transactionModel != nullptr);

    QVERIFY(UiTestHelpers::readAccountsInto(app, storage));
    QCOMPARE(treeModel->rowCount(), 1);

    auto *pages = transactionPagesOf(*central);
    auto *headline = transactionHeadlineOf(*central);
    QVERIFY(pages != nullptr);
    QVERIFY(headline != nullptr);

    const QString headlineWithoutAnAccount = headline->text();

    auto *view = central->accountWidget();
    view->setCurrentIndex(UiTestHelpers::firstAccountOf(*treeModel));

    QCOMPARE(transactionModel->accountId(), 4711u);
    QTRY_COMPARE_WITH_TIMEOUT(transactionModel->rowCount(), 3, workerTimeoutMs);
    QCOMPARE(pages->currentIndex(), 0);

    // The bank node. It groups, it is not an account.
    view->setCurrentIndex(treeModel->index(0, 0));

    QCOMPARE(transactionModel->accountId(), 0u);
    QCOMPARE(transactionModel->rowCount(), 0);
    QCOMPARE(pages->currentIndex(), 1);
    QCOMPARE(headline->text(), headlineWithoutAnAccount);
}

/**
 * An account the user deselects in the wizard leaves the tree. The selection
 * cannot stay on an entry that is gone, so it falls away and the transaction
 * view returns to the state where nothing is chosen.
 */
void AppCentralWidgetTest::anAccountThatBecomesInactiveTakesTheSelectionWithIt()
{
    Logger logger;
    Storage storage(applicationInfo());

    QVERIFY(openStorage(storage));

    const auto account = Account::fromMap(TestHelpers::namedAccountMap());
    QVERIFY(!storage.storeItem(account.get()).isError());
    QVERIFY(TransactionHelpers::putTransactions(storageFile(),
                                                password(),
                                                4711,
                                                3,
                                                QStringLiteral("Buchung")));

    App app(&logger, &storage);
    app.initialize();

    auto *central = app.findChild<AppCentralWidget *>();
    auto *treeModel = app.findChild<AccountTreeModel *>();
    auto *transactionModel = app.findChild<TransactionTableModel *>();

    QVERIFY(central != nullptr);
    QVERIFY(treeModel != nullptr);
    QVERIFY(transactionModel != nullptr);

    QVERIFY(UiTestHelpers::readAccountsInto(app, storage));

    central->accountWidget()->setCurrentIndex(UiTestHelpers::firstAccountOf(*treeModel));
    QTRY_COMPARE_WITH_TIMEOUT(transactionModel->rowCount(), 3, workerTimeoutMs);

    // The wizard turns the account down. What reaches the window is the run of
    // accounts without it.
    app.setAccounts({});

    QCOMPARE(treeModel->rowCount(), 0);
    QCOMPARE(transactionModel->accountId(), 0u);
    QTRY_COMPARE_WITH_TIMEOUT(transactionModel->rowCount(), 0, workerTimeoutMs);

    auto *headline = transactionHeadlineOf(*central);
    QVERIFY(headline != nullptr);
    QVERIFY(!headline->text().isEmpty());
}

/**
 * A choice of account does not outlive its storage. After one is opened nothing
 * is chosen, whatever was chosen before it was closed.
 */
void AppCentralWidgetTest::openingAStorageLeavesNoAccountSelected()
{
    Logger logger;
    Storage storage(applicationInfo());

    QVERIFY(openStorage(storage));

    const auto account = Account::fromMap(TestHelpers::namedAccountMap());
    QVERIFY(!storage.storeItem(account.get()).isError());
    QVERIFY(TransactionHelpers::putTransactions(storageFile(),
                                                password(),
                                                4711,
                                                3,
                                                QStringLiteral("Buchung")));

    App app(&logger, &storage);
    app.initialize();

    auto *central = app.findChild<AppCentralWidget *>();
    auto *treeModel = app.findChild<AccountTreeModel *>();
    auto *transactionModel = app.findChild<TransactionTableModel *>();

    QVERIFY(central != nullptr);
    QVERIFY(treeModel != nullptr);
    QVERIFY(transactionModel != nullptr);

    QVERIFY(UiTestHelpers::readAccountsInto(app, storage));

    central->accountWidget()->setCurrentIndex(UiTestHelpers::firstAccountOf(*treeModel));
    QTRY_COMPARE_WITH_TIMEOUT(transactionModel->rowCount(), 3, workerTimeoutMs);

    app.closeStorage();

    QCOMPARE(transactionModel->accountId(), 0u);
    QCOMPARE(transactionModel->rowCount(), 0);
    QCOMPARE(transactionPagesOf(*central)->currentIndex(), 1);
}

/**
 * A read that finds nothing is not a failure. A storage without accounts reaches
 * the notice that says none is set up, and the status bar stays clear; taken as
 * an error it would say instead that the holding could not be read.
 */
void AppCentralWidgetTest::aStorageWithoutAccountsSaysSoWithoutAMessage()
{
    Logger logger;
    Storage storage(applicationInfo());

    QVERIFY(openStorage(storage));

    App app(&logger, &storage);
    app.initialize();

    auto *central = app.findChild<AppCentralWidget *>();
    QVERIFY(central != nullptr);

    auto *notice = accountNoticeOf(*central);
    QVERIFY(notice != nullptr);

    const QString noticeOfAnEmptyStorage = notice->text();

    QVERIFY(UiTestHelpers::readAccountsInto(app, storage));

    QCOMPARE(app.statusBar()->currentMessage(), QString());
    QCOMPARE(notice->text(), noticeOfAnEmptyStorage);
    QCOMPARE(accountPagesOf(*central)->currentWidget(), notice->parentWidget());
}

/**
 * The same for an account whose transactions the bank has not brought yet: its
 * own notice, and no message.
 */
void AppCentralWidgetTest::anAccountWithoutTransactionsSaysSoWithoutAMessage()
{
    Logger logger;
    Storage storage(applicationInfo());

    QVERIFY(openStorage(storage));

    const auto account = Account::fromMap(TestHelpers::namedAccountMap());
    QVERIFY(!storage.storeItem(account.get()).isError());

    App app(&logger, &storage);
    app.initialize();

    auto *central = app.findChild<AppCentralWidget *>();
    auto *treeModel = app.findChild<AccountTreeModel *>();
    auto *transactionModel = app.findChild<TransactionTableModel *>();

    QVERIFY(central != nullptr);
    QVERIFY(treeModel != nullptr);
    QVERIFY(transactionModel != nullptr);

    QVERIFY(UiTestHelpers::readAccountsInto(app, storage));

    auto *headline = transactionHeadlineOf(*central);
    QVERIFY(headline != nullptr);

    const QString headlineWithoutAnAccount = headline->text();

    QSignalSpy finishedSpy(&storage, &Storage::readFinished);
    central->accountWidget()->setCurrentIndex(UiTestHelpers::firstAccountOf(*treeModel));

    QVERIFY(finishedSpy.wait(workerTimeout));

    QCOMPARE(transactionModel->rowCount(), 0);
    QCOMPARE(transactionPagesOf(*central)->currentIndex(), 1);
    QVERIFY(headline->text() != headlineWithoutAnAccount);
    QCOMPARE(app.statusBar()->currentMessage(), QString());
}

/**
 * Without an account the bar stands there and does nothing. It does not vanish:
 * a bar that comes and goes moves what stands below it, and an assistive tool
 * can only say that a control exists while it is there.
 */
void AppCentralWidgetTest::theFilterBarStandsAndWaitsWhileNoAccountIsChosen()
{
    AppCentralWidget widget;
    TransactionTableModel model;

    widget.setTransactionModel(&model);
    widget.setTransactionNotice(AppCentralWidget::TransactionNotice::NoAccountSelected);

    auto *bar = filterBarOf(widget);
    QVERIFY(bar != nullptr);

    QVERIFY(!bar->isHidden());
    QVERIFY(!bar->isEnabled());

    widget.setTransactionNotice(AppCentralWidget::TransactionNotice::BankSelected);
    QVERIFY(!bar->isHidden());
    QVERIFY(!bar->isEnabled());

    widget.setTransactionNotice(AppCentralWidget::TransactionNotice::AccountWithoutTransactions);
    QVERIFY(bar->isEnabled());
}

/**
 * An account that holds transactions of which none meets the filter is the
 * fourth empty state, and the only one whose cause the user can take back where
 * he stands. It is therefore the only one with a button.
 */
void AppCentralWidgetTest::aFilterWithoutAMatchGetsAnEmptyStateOfItsOwnWithAButton()
{
    Logger logger;
    Storage storage(applicationInfo());

    QVERIFY(openStorage(storage));

    const auto account = Account::fromMap(TestHelpers::namedAccountMap());
    QVERIFY(!storage.storeItem(account.get()).isError());
    QVERIFY(TransactionHelpers::putTransactions(storageFile(),
                                                password(),
                                                4711,
                                                3,
                                                QStringLiteral("Miete")));

    App app(&logger, &storage);
    app.initialize();

    auto *central = app.findChild<AppCentralWidget *>();
    auto *treeModel = app.findChild<AccountTreeModel *>();
    auto *transactionModel = app.findChild<TransactionTableModel *>();

    QVERIFY(central != nullptr);
    QVERIFY(treeModel != nullptr);
    QVERIFY(transactionModel != nullptr);

    QVERIFY(UiTestHelpers::readAccountsInto(app, storage));

    central->accountWidget()->setCurrentIndex(UiTestHelpers::firstAccountOf(*treeModel));
    QTRY_COMPARE_WITH_TIMEOUT(transactionModel->rowCount(), 3, workerTimeoutMs);

    auto *headline = transactionHeadlineOf(*central);
    auto *notice = transactionNoticeOf(*central);
    auto *button = noticeResetOf(*central);
    auto *search = searchFieldOf(*central);

    QVERIFY(headline != nullptr);
    QVERIFY(notice != nullptr);
    QVERIFY(button != nullptr);
    QVERIFY(search != nullptr);

    const QString headlineWithTransactions = headline->text();

    search->setText(QStringLiteral("Versicherung"));

    QTRY_COMPARE_WITH_TIMEOUT(transactionModel->rowCount(), 0, workerTimeoutMs);
    QTRY_VERIFY_WITH_TIMEOUT(!button->isHidden(), workerTimeoutMs);

    QVERIFY(headline->text() != headlineWithTransactions);
    QVERIFY(!notice->text().isEmpty());
    QCOMPARE(transactionPagesOf(*central)->currentIndex(), 1);

    // The button takes the filter back and the transactions return.
    button->click();

    QVERIFY(search->text().isEmpty());
    QTRY_COMPARE_WITH_TIMEOUT(transactionModel->rowCount(), 3, workerTimeoutMs);
    QVERIFY(button->isHidden());
}

/**
 * The counter names what the filter leaves of the whole holding, not what the
 * page that was read carries. A holding larger than one window is what tells the
 * two apart.
 */
void AppCentralWidgetTest::theCounterNamesWhatTheFilterLeaves()
{
    Logger logger;
    Storage storage(applicationInfo());

    QVERIFY(openStorage(storage));

    const auto account = Account::fromMap(TestHelpers::namedAccountMap());
    QVERIFY(!storage.storeItem(account.get()).isError());
    QVERIFY(TransactionHelpers::putTransactions(storageFile(),
                                                password(),
                                                4711,
                                                120,
                                                QStringLiteral("Miete")));
    QVERIFY(TransactionHelpers::putTransactions(storageFile(),
                                                password(),
                                                4711,
                                                80,
                                                QStringLiteral("Gehalt")));

    App app(&logger, &storage);
    app.initialize();

    auto *central = app.findChild<AppCentralWidget *>();
    auto *treeModel = app.findChild<AccountTreeModel *>();
    auto *transactionModel = app.findChild<TransactionTableModel *>();

    QVERIFY(central != nullptr);
    QVERIFY(treeModel != nullptr);
    QVERIFY(transactionModel != nullptr);

    QVERIFY(UiTestHelpers::readAccountsInto(app, storage));

    auto *counter = counterOf(*central);
    auto *search = searchFieldOf(*central);
    QVERIFY(counter != nullptr);
    QVERIFY(search != nullptr);

    central->accountWidget()->setCurrentIndex(UiTestHelpers::firstAccountOf(*treeModel));

    QTRY_COMPARE_WITH_TIMEOUT(transactionModel->totalRows(), 200, workerTimeoutMs);
    QVERIFY(counter->text().contains(QStringLiteral("200")));

    // One page holds a hundred of the two hundred, and while rows are missing
    // the counter names both numbers: what is loaded and what the filter leaves.
    QCOMPARE(transactionModel->rowCount(), 100);
    QVERIFY(!transactionModel->atEnd());
    QVERIFY(counter->text().contains(QStringLiteral("100")));

    // Eighty fit on one page, so the holding is through and the counter drops
    // back to the one number. Checked on the text and on no pixel.
    search->setText(QStringLiteral("Gehalt"));

    QTRY_COMPARE_WITH_TIMEOUT(transactionModel->totalRows(), 80, workerTimeoutMs);
    QTRY_VERIFY_WITH_TIMEOUT(transactionModel->atEnd(), workerTimeoutMs);
    QVERIFY(counter->text().contains(QStringLiteral("80")));
    QVERIFY(!counter->text().contains(QStringLiteral("100")));
    QVERIFY(!counter->text().contains(QStringLiteral("200")));
}

/**
 * The filter outlives a change of account, so that whoever is looking for
 * something keeps looking for it. It does not outlive the storage it was set in.
 */
void AppCentralWidgetTest::closingTheStorageTakesTheFilterWithIt()
{
    Logger logger;
    Storage storage(applicationInfo());

    QVERIFY(openStorage(storage));

    const auto account = Account::fromMap(TestHelpers::namedAccountMap());
    QVERIFY(!storage.storeItem(account.get()).isError());
    QVERIFY(TransactionHelpers::putTransactions(storageFile(),
                                                password(),
                                                4711,
                                                3,
                                                QStringLiteral("Miete")));

    App app(&logger, &storage);
    app.initialize();

    auto *central = app.findChild<AppCentralWidget *>();
    auto *treeModel = app.findChild<AccountTreeModel *>();
    auto *transactionModel = app.findChild<TransactionTableModel *>();

    QVERIFY(central != nullptr);
    QVERIFY(treeModel != nullptr);
    QVERIFY(transactionModel != nullptr);

    QVERIFY(UiTestHelpers::readAccountsInto(app, storage));

    central->accountWidget()->setCurrentIndex(UiTestHelpers::firstAccountOf(*treeModel));
    QTRY_COMPARE_WITH_TIMEOUT(transactionModel->rowCount(), 3, workerTimeoutMs);

    auto *search = searchFieldOf(*central);
    QVERIFY(search != nullptr);

    search->setText(QStringLiteral("Miete"));
    QTRY_VERIFY_WITH_TIMEOUT(transactionModel->filter().isSet(), workerTimeoutMs);

    app.closeStorage();

    QVERIFY(search->text().isEmpty());
    QVERIFY(!transactionModel->filter().isSet());
}

/**
 * The order belongs to the storage it was chosen in, and giving up the account
 * puts the model back to the one it opens with. The indicator was set once at
 * setup and heard nothing of that: the header went on pointing at a column the
 * rows no longer stood under, and the next click on it turned around an order
 * that was never in force. A single click could then no longer produce that
 * column ascending at all.
 */
void AppCentralWidgetTest::closingTheStorageTakesTheSortIndicatorBackWithIt()
{
    Logger logger;
    Storage storage(applicationInfo());

    QVERIFY(openStorage(storage));

    const auto account = Account::fromMap(TestHelpers::namedAccountMap());
    QVERIFY(!storage.storeItem(account.get()).isError());
    QVERIFY(TransactionHelpers::putTransactions(storageFile(),
                                                password(),
                                                4711,
                                                3,
                                                QStringLiteral("Miete")));

    App app(&logger, &storage);
    app.initialize();

    auto *central = app.findChild<AppCentralWidget *>();
    auto *treeModel = app.findChild<AccountTreeModel *>();
    auto *transactionModel = app.findChild<TransactionTableModel *>();
    auto *view = app.findChild<QTableView *>(QStringLiteral("tableViewTransactions"));

    QVERIFY(central != nullptr);
    QVERIFY(treeModel != nullptr);
    QVERIFY(transactionModel != nullptr);
    QVERIFY(view != nullptr);

    QVERIFY(UiTestHelpers::readAccountsInto(app, storage));

    central->accountWidget()->setCurrentIndex(UiTestHelpers::firstAccountOf(*treeModel));
    QTRY_COMPARE_WITH_TIMEOUT(transactionModel->rowCount(), 3, workerTimeoutMs);

    auto *const header = view->horizontalHeader();

    transactionModel->sort(TransactionTableModel::ValueColumn, Qt::AscendingOrder);

    QCOMPARE(header->sortIndicatorSection(), int(TransactionTableModel::ValueColumn));
    QCOMPARE(header->sortIndicatorOrder(), Qt::AscendingOrder);

    app.closeStorage();

    // The model went back to its default, and the header went with it.
    QCOMPARE(int(transactionModel->sortColumn()), int(TransactionTableModel::DateColumn));
    QCOMPARE(header->sortIndicatorSection(), int(TransactionTableModel::DateColumn));
    QCOMPARE(header->sortIndicatorOrder(), Qt::DescendingOrder);
}

/**
 * The view used to be a widget that kept its entries in itself, and the getter
 * answered with nothing at all. Whoever asked for it got a null pointer and
 * would have dereferenced it.
 */
void AppCentralWidgetTest::theAccountViewIsThereAndCarriesTheModel()
{
    AppCentralWidget widget;
    AccountTreeModel model;

    auto *view = widget.accountWidget();
    QVERIFY(view != nullptr);

    widget.setAccountModel(&model);

    QCOMPARE(view->model(), &model);
}

/**
 * An empty tree and a tree that was never filled look the same on screen. The
 * user reads that no account has been set up instead of facing a blank area that
 * could just as well be a failure.
 */
void AppCentralWidgetTest::anEmptyModelPutsTheNoticeInPlaceOfTheTree()
{
    AppCentralWidget widget;
    AccountTreeModel model;

    widget.setAccountModel(&model);

    auto *pages = accountPagesOf(widget);
    auto *notice = accountNoticeOf(widget);
    QVERIFY(pages != nullptr);
    QVERIFY(notice != nullptr);

    QCOMPARE(pages->currentWidget(), notice->parentWidget());
    QVERIFY(!notice->text().isEmpty());

    BankingItems items;
    items << Account::fromMap(TestHelpers::namedAccountMap());
    model.setItems(items);

    QCOMPARE(pages->currentWidget(), widget.accountWidget()->parentWidget());
}

/**
 * The entry carries the account name, the IBAN and the balance with its
 * currency, and nothing else. An account whose bank reports no IBAN leaves that
 * place empty and stays in the tree rather than dropping out of it.
 */
void AppCentralWidgetTest::anEntryCarriesTheAccountNameTheIbanAndTheBalance()
{
    AccountTreeModel model;

    auto withoutIban = TestHelpers::namedAccountMap();
    withoutIban[QStringLiteral("account_name")] = QStringLiteral("Tagesgeld");
    withoutIban[QStringLiteral("iban")] = QString();

    BankingItems items;
    items << Account::fromMap(TestHelpers::namedAccountMap()) << Account::fromMap(withoutIban);
    model.setItems(items);

    const QModelIndex bank = model.index(0, 0);
    QCOMPARE(model.rowCount(bank), 2);

    const QString separator = QStringLiteral(" - ");

    const QStringList girokonto
        = model.data(model.index(0, 0, bank), Qt::DisplayRole).toString().split(separator);

    // Three places and no fourth. The owner and the account number stay roles of
    // the model and are not part of what the entry shows.
    QCOMPARE(girokonto.size(), 3);
    QCOMPARE(girokonto.at(0), QStringLiteral("Girokonto"));
    QCOMPARE(girokonto.at(1), QStringLiteral("DE02500105170137075030"));
    QVERIFY(girokonto.at(2).contains(QStringLiteral("EUR")));

    const QStringList tagesgeld
        = model.data(model.index(1, 0, bank), Qt::DisplayRole).toString().split(separator);

    QCOMPARE(tagesgeld.size(), 3);
    QCOMPARE(tagesgeld.at(0), QStringLiteral("Tagesgeld"));
    QVERIFY(tagesgeld.at(1).isEmpty());
    QVERIFY(tagesgeld.at(2).contains(QStringLiteral("EUR")));
}

/**
 * A read that failed is not a storage without accounts. What the view already
 * shows stays where it is, and a view with nothing to show says that the holding
 * could not be read instead of that there is none.
 */
void AppCentralWidgetTest::aFailedReadDoesNotLookLikeAnEmptyStorage()
{
    AppCentralWidget widget;
    AccountTreeModel model;

    widget.setAccountModel(&model);

    auto *pages = accountPagesOf(widget);
    auto *notice = accountNoticeOf(widget);
    QVERIFY(pages != nullptr);
    QVERIFY(notice != nullptr);

    const QString emptyText = notice->text();
    const QString failure = QStringLiteral("The storage could not be used.");

    widget.showAccountsUnreadable(failure);

    QCOMPARE(pages->currentWidget(), notice->parentWidget());
    QCOMPARE(notice->text(), failure);
    QVERIFY(notice->text() != emptyText);

    // With a holding on screen the failure takes nothing off it.
    BankingItems items;
    items << Account::fromMap(TestHelpers::namedAccountMap());
    model.setItems(items);

    widget.showAccountsUnreadable(failure);

    QCOMPARE(pages->currentWidget(), widget.accountWidget()->parentWidget());
}

/**
 * Closing a storage does not stop the read that is going on it. The result
 * belongs to the file that was closed, and shown under the next one it would put
 * a foreign holding in front of the user as his own.
 */
void AppCentralWidgetTest::aReadThatOutlivesItsStorageReachesNoView()
{
    Logger logger;
    Storage storage(applicationInfo());

    QVERIFY(!storage.setKey(password()).isError());
    storage.setStorageFile(storageFile());
    QVERIFY(!storage.initialize(true).isError());

    for (int i = 0; i < 5; ++i) {
        const auto account = TestHelpers::createFakeAccount();
        QVERIFY(!storage.storeItem(account.get()).isError());
    }

    App app(&logger, &storage);
    app.initialize();

    auto *model = app.findChild<AccountTreeModel *>();
    QVERIFY(model != nullptr);

    auto *overview = app.findChild<StorageDialog *>();
    QVERIFY(overview != nullptr);

    connect(&storage, &Storage::itemsReceived, &app, &App::setAccounts);
    Q_EMIT overview->storageOpened();

    QSignalSpy finishedSpy(&storage, &Storage::readFinished);
    QVERIFY(!storage.receiveItems({.type = Storage::StorageAccount}).isError());

    app.closeStorage();

    QVERIFY(finishedSpy.wait(workerTimeout));
    QCOMPARE(model->rowCount(), 0);
}

void AppCentralWidgetTest::startsOnTheStorageOverview()
{
    const AppCentralWidget widget;

    QCOMPARE(widget.page(), AppCentralWidget::Page::Storages);

    const auto *pages = pagesOf(widget);
    QVERIFY(pages != nullptr);
    QCOMPARE(pages->count(), 2);
    QCOMPARE(pages->currentIndex(), 0);
}

/**
 * The page that is left has to survive being left. Closing a storage drops the
 * records out of the models, not the widgets out of the page.
 */
void AppCentralWidgetTest::switchingKeepsBothPagesAlive()
{
    AppCentralWidget widget;

    const auto *pages = pagesOf(widget);
    QVERIFY(pages != nullptr);

    const QWidget *storagesPage = pages->widget(0);
    const QWidget *bankingPage = pages->widget(1);
    QVERIFY(storagesPage != nullptr);
    QVERIFY(bankingPage != nullptr);

    widget.setPage(AppCentralWidget::Page::Banking);

    QCOMPARE(widget.page(), AppCentralWidget::Page::Banking);
    QCOMPARE(pages->currentIndex(), 1);
    QCOMPARE(pages->count(), 2);
    QCOMPARE(pages->widget(0), storagesPage);
    QCOMPARE(pages->widget(1), bankingPage);

    widget.setPage(AppCentralWidget::Page::Storages);

    QCOMPARE(widget.page(), AppCentralWidget::Page::Storages);
    QCOMPARE(pages->currentIndex(), 0);
    QCOMPARE(pages->count(), 2);
    QCOMPARE(pages->widget(0), storagesPage);
    QCOMPARE(pages->widget(1), bankingPage);
}

void AppCentralWidgetTest::switchingToThePageAlreadyShownChangesNothing()
{
    AppCentralWidget widget;

    widget.setPage(AppCentralWidget::Page::Storages);

    QCOMPARE(widget.page(), AppCentralWidget::Page::Storages);

    widget.setPage(AppCentralWidget::Page::Banking);
    widget.setPage(AppCentralWidget::Page::Banking);

    QCOMPARE(widget.page(), AppCentralWidget::Page::Banking);
    QCOMPARE(pagesOf(widget)->currentIndex(), 1);
}

/**
 * Closing a storage has to leave nothing of it behind. The page is the visible
 * half of that, the model the half a user cannot see: a window that shows the
 * overview again while the accounts of the closed storage still sit in the model
 * would hand them to the next storage that is opened.
 */
void AppCentralWidgetTest::closingAStorageReturnsToTheOverviewAndDropsTheAccounts()
{
    Logger logger;
    Storage storage(applicationInfo());

    App app(&logger, &storage);
    app.initialize();

    auto *central = app.findChild<AppCentralWidget *>();
    QVERIFY(central != nullptr);

    auto *model = app.findChild<AccountTreeModel *>();
    QVERIFY(model != nullptr);

    auto *overview = app.findChild<StorageDialog *>();
    QVERIFY(overview != nullptr);

    BankingItems items;
    items << Account::fromMap(TestHelpers::namedAccountMap());
    app.setAccounts(items);

    // The way the overview reports a storage it got open. Going through the
    // signal rather than setting the page keeps the test on the path the
    // application takes.
    Q_EMIT overview->storageOpened();

    QCOMPARE(model->rowCount(), 1);
    QCOMPARE(central->page(), AppCentralWidget::Page::Banking);

    app.closeStorage();

    QCOMPARE(central->page(), AppCentralWidget::Page::Storages);
    QCOMPARE(model->rowCount(), 0);
}

/**
 * Every command in the tool bar needs an open storage, so the bar waits for one.
 * The menu entries stay in place and turn grey instead; a menu that changes its
 * shape has to be learned twice.
 */
void AppCentralWidgetTest::theCommandsThatNeedAStorageWaitForOne()
{
    Logger logger;
    Storage storage(applicationInfo());

    App app(&logger, &storage);
    app.initialize();

    auto *toolBar = app.findChild<QToolBar *>(QStringLiteral("appToolBar"));
    QVERIFY(toolBar != nullptr);

    auto *closeAction = actionOf(app, QStringLiteral("appCloseStorageAction"));
    auto *assistantAction = actionOf(app, QStringLiteral("appSetupAssistantAction"));
    auto *newAction = actionOf(app, QStringLiteral("appNewStorageAction"));

    // The areas it puts back only stand on the second page.
    auto *resetAction = actionOf(app, QStringLiteral("appResetLayoutAction"));

    QVERIFY(closeAction != nullptr);
    QVERIFY(assistantAction != nullptr);
    QVERIFY(newAction != nullptr);
    QVERIFY(resetAction != nullptr);

    QVERIFY(!toolBar->isVisibleTo(&app));
    QVERIFY(!closeAction->isEnabled());
    QVERIFY(!assistantAction->isEnabled());
    QVERIFY(!resetAction->isEnabled());

    // Creating a storage is the one command the first page is there for.
    QVERIFY(newAction->isEnabled());

    auto *overview = app.findChild<StorageDialog *>();
    QVERIFY(overview != nullptr);

    Q_EMIT overview->storageOpened();

    QVERIFY(toolBar->isVisibleTo(&app));
    QVERIFY(closeAction->isEnabled());
    QVERIFY(assistantAction->isEnabled());
    QVERIFY(newAction->isEnabled());
    QVERIFY(resetAction->isEnabled());

    app.closeStorage();

    QVERIFY(!toolBar->isVisibleTo(&app));
    QVERIFY(!closeAction->isEnabled());
    QVERIFY(!assistantAction->isEnabled());
    QVERIFY(!resetAction->isEnabled());
}

/**
 * The command behind this entry is not built yet. The entry exists so that the
 * menu keeps its shape once it is switched on, and it stays disabled until then
 * rather than doing nothing when pressed.
 */
void AppCentralWidgetTest::theEntriesWithoutTheirStoryStayDisabled()
{
    Logger logger;
    Storage storage(applicationInfo());

    App app(&logger, &storage);
    app.initialize();

    const auto names = QStringList{QStringLiteral("appFetchTransactionsAction")};

    for (const auto &name : names) {
        auto *action = actionOf(app, name);

        QVERIFY2(action != nullptr, qPrintable(name));
        QVERIFY2(!action->isEnabled(), qPrintable(name));
    }

    auto *overview = app.findChild<StorageDialog *>();
    QVERIFY(overview != nullptr);

    Q_EMIT overview->storageOpened();

    // An open storage does not bring them to life either. Only the command
    // behind them does, once it is built.
    for (const auto &name : names) {
        QVERIFY2(!actionOf(app, name)->isEnabled(), qPrintable(name));
    }
}

/**
 * The overview says what happened, the window puts it where the user reads it.
 * The overview is a page and has no status bar of its own.
 */
void AppCentralWidgetTest::aMessageFromTheOverviewReachesTheStatusBar()
{
    Logger logger;
    Storage storage(applicationInfo());

    App app(&logger, &storage);
    app.initialize();

    auto *overview = app.findChild<StorageDialog *>();
    QVERIFY(overview != nullptr);

    QCOMPARE(app.statusBar()->currentMessage(), QString());

    const QString text = QStringLiteral("\"Privat\" could not be opened.");
    Q_EMIT overview->message(text);

    QCOMPARE(app.statusBar()->currentMessage(), text);
}

/**
 * A message belongs to the page it was raised on. "Nothing was found, import
 * your accounts" is true of an open storage and says nothing on the overview,
 * where there is no storage to import into. It used to stay there after the
 * storage was closed.
 */
void AppCentralWidgetTest::leavingAPageTakesItsMessageWithIt()
{
    Logger logger;
    Storage storage(applicationInfo());

    App app(&logger, &storage);
    app.initialize();

    auto *overview = app.findChild<StorageDialog *>();
    QVERIFY(overview != nullptr);

    Q_EMIT overview->storageOpened();

    const QString text = QStringLiteral("Nothing was found.");
    app.showMessage(text);

    QCOMPARE(app.statusBar()->currentMessage(), text);

    app.closeStorage();

    QCOMPARE(app.statusBar()->currentMessage(), QString());
}

/**
 * The overview used to be a window of its own and carried the minimum size of
 * one. As a page inside the window it hands that size on to the window, and
 * where the window is smaller its contents run out of it. The scroll area it
 * already carries is what handles a window too small for the entries.
 */
void AppCentralWidgetTest::theOverviewDoesNotForceASizeOnTheWindow()
{
    Storage storage(applicationInfo());

    StorageDialog overview(&storage);
    overview.initialize(nullptr);

    QCOMPARE(overview.minimumWidth(), 0);
    QCOMPARE(overview.minimumHeight(), 0);
}

/**
 * The menu entry is reachable through its shortcut before a storage was ever
 * opened. It has to do nothing rather than something.
 */
void AppCentralWidgetTest::closingAStorageWithoutOneOpenIsHarmless()
{
    Logger logger;
    Storage storage(applicationInfo());

    App app(&logger, &storage);
    app.initialize();

    auto *central = app.findChild<AppCentralWidget *>();
    QVERIFY(central != nullptr);

    app.closeStorage();

    QCOMPARE(central->page(), AppCentralWidget::Page::Storages);
}

} // namespace olbaflinx::ui::tests

QTEST_MAIN(olbaflinx::ui::tests::AppCentralWidgetTest)

#include "tst_appcentralwidget.moc"
