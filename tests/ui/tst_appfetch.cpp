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
#include "core/Banking/Banking.h"
#include "core/Logger/Logger.h"
#include "core/Storage/Storage.h"
#include "ui/AccountFetch.h"
#include "ui/App.h"
#include "ui/AppCentralWidget.h"
#include "ui/BankingGui.h"
#include "ui/ErrorMessage.h"
#include "ui/Models/AccountTreeModel.h"
#include "ui/Models/TransactionTableModel.h"

#include "BankingHelpers.h"
#include "TestHelpers.h"
#include "TransactionHelpers.h"
#include "UiTestHelpers.h"

#include <QtTest/QtTest>

#include <QtCore/QTemporaryDir>

#include <QtGui/QAccessible>
#include <QtGui/QAccessibleInterface>
#include <QtGui/QAction>

#include <QtCore/QTimer>

#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QTreeView>

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

/** The figure the account carries when it is set up, before any fetch. */
constexpr double storedBalance = 12.5;

/** What a fetch brings back for it. */
constexpr double fetchedBalance = 99.0;

} // namespace

/**
 * The way from the entry a user reaches to the rows he sees afterwards.
 *
 * A session needs a bank, and these runs have none. What can be measured without
 * one is everything around it: that the entry is wired at all, what its state
 * says while nothing is chosen, and what the window does with an outcome once it
 * arrives. The outcome therefore travels through a signal of its own, which a
 * test raises the way a finished session would.
 */
class AppFetchTest final : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> workingDirectory;
    std::unique_ptr<QTemporaryDir> bankingHome;

    static QString password() { return TestHelpers::password(); }

    static constexpr int workerTimeoutMs = UiTestHelpers::workerTimeoutMs;

    /**
     * A session is answered by a bank that is not there, so it needs the whole
     * span a network call may take before it gives up.
     */
    static constexpr int sessionTimeoutMs = 60000;

    static ApplicationInfo applicationInfo()
    {
        auto info = TestHelpers::applicationInfo(QStringLiteral("OlbaFlinxAppFetchTest"));
        info.registrationKey = QStringLiteral("probe-key");

        return info;
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
     * One account, under the identifier every run here works with.
     *
     * Without online access, and that is not a detail: an account that has it
     * sends a session to a bank, which no run here can reach. What the banking
     * layer does with such an account is measurable all the same: it passes it
     * over by its identifier, before anything is sent.
     */
    [[nodiscard]] bool putAccount(Storage &storage, quint32 uniqueId = testAccountId) const
    {
        auto map = TestHelpers::accountMapWith(uniqueId, storedBalance);
        map[QStringLiteral("backend_name")] = QStringLiteral("aqnone");

        const auto account = Account::fromMap(map);

        return !storage.storeItem(account.get()).isError();
    }

    static QAction *fetchActionOf(const App &app)
    {
        return app.findChild<QAction *>(QStringLiteral("appFetchTransactionsAction"));
    }

    static QAction *collectiveActionOf(const App &app)
    {
        return app.findChild<QAction *>(QStringLiteral("appFetchAllTransactionsAction"));
    }

    /** Three accounts under the same bank, none of them with online access. */
    [[nodiscard]] bool putThreeAccounts(Storage &storage) const
    {
        for (int index = 0; index < 3; ++index) {
            if (!putAccount(storage, testAccountId + quint32(index))) {
                return false;
            }
        }

        return true;
    }

    static AccountFetch *fetchOf(const App &app) { return app.findChild<AccountFetch *>(); }

    static QAccessibleInterface *entryFor(QMenu *menu, QAction *action)
    {
        const int index = menu->actions().indexOf(action);
        if (index < 0) {
            return nullptr;
        }

        QAccessibleInterface *menuInterface = QAccessible::queryAccessibleInterface(menu);

        return menuInterface == nullptr ? nullptr : menuInterface->child(index);
    }

    /**
     * Brings the window up on the second page with one account in the tree, and
     * leaves that account chosen.
     */
    [[nodiscard]] static bool chooseTheAccount(App &app, Storage &storage)
    {
        if (!UiTestHelpers::readAccountsInto(app, storage)) {
            return false;
        }

        auto *const central = app.findChild<AppCentralWidget *>();
        auto *const treeModel = app.findChild<AccountTreeModel *>();

        if (central == nullptr || treeModel == nullptr || treeModel->rowCount() != 1) {
            return false;
        }

        central->accountWidget()->setCurrentIndex(UiTestHelpers::firstAccountOf(*treeModel));

        return true;
    }

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void theActionStartsAFetchForTheChosenAccount();
    void withoutASelectionTheActionIsVisibleAndCannotBeInvoked();
    void theTransactionViewShowsTheNewRowsWithoutChoosingTheAccountAgain();
    void theCounterOfTheFilterBarCarriesTheNewNumber();
    void aFailureDuringAFetchIsNoFailureOfTheAccountView();
    void theEntriesOfTheFetchCarryAnIdentifierANameAndARole();

    void theCloseEntryCannotBeInvokedWhileAFetchRuns();
    void theWindowIsNotClosedWhileAFetchRuns();
    void theQuitEntryGoesThroughTheSameRefusalAsClosing();
    void theAccountTreeTakesUpWhatWasWrittenPastTheWindow();
    void aRefreshThatFindsNoAccountDoesNotEmptyTheTreeLater();
    void theAccountTreeShowsMoreAccountsThanOneDefaultWindowHolds();
    void theRegistrationKeyIsNotEmptyWhenTheBankingLayerComesUp();
    void theWindowComesUpBesideAnInstanceOfTheWizard();
    void theSpanOfTheCachedCredentialRunsAfterAFetchAndNotDuringIt();
    void theAccountViewShowsANewBalanceWithoutARestart();
    void aFetchWithoutNewBookingsSaysSo();

    void theCollectiveEntryRunsForEveryAccountAndHangsOnNoSelection();
    void theCollectiveEntryCarriesAnIdentifierANameAndARole();
    void theOutcomeOfACollectiveFetchNamesFourFigures();
    void theClosingMessageComesOnlyAfterTheLastAccount();
    void aStopBringsTheQuestionWithKeepingPreselected();
    void keepingAndDiscardingSayDifferentThingsAndDiscardingWritesNothing();
};

/**
 * AqBanking keeps its configuration below AQBANKING_HOME. Without pointing that
 * at a directory of our own, every run would write into the configuration of
 * whoever started it.
 */
void AppFetchTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);

    // Test mode alone puts the locations below ~/.qttest, which is a directory
    // of the user like any other and survives the run. HOME goes into a
    // temporary directory, so that nothing this binary writes outlives it.
    QVERIFY(TestHelpers::useTemporaryHome());

    bankingHome = std::make_unique<QTemporaryDir>();
    QVERIFY(bankingHome->isValid());

    QVERIFY(qputenv("AQBANKING_HOME", bankingHome->path().toUtf8()));
}

void AppFetchTest::cleanupTestCase()
{
    qunsetenv("AQBANKING_HOME");
    bankingHome.reset();
}

void AppFetchTest::init()
{
    workingDirectory = std::make_unique<QTemporaryDir>();
    QVERIFY(workingDirectory->isValid());
}

void AppFetchTest::cleanup()
{
    workingDirectory.reset();
}

/**
 * The entry was built in the first epic and left switched off. What is measured
 * here is that it reaches the banking layer at all, and that the account it
 * reaches it with is the one the tree has chosen.
 *
 * The banking layer answers that this account has no online access, which it can
 * only say about the account it was handed. That a session with a bank cannot be
 * run here is what leaves this as the way to measure it.
 */
void AppFetchTest::theActionStartsAFetchForTheChosenAccount()
{
    Logger logger;
    Storage storage(applicationInfo());

    QVERIFY(openStorage(storage));
    QVERIFY(putAccount(storage));

    App app(&logger, &storage, applicationInfo());
    app.initialize();

    QVERIFY(chooseTheAccount(app, storage));

    auto *const action = fetchActionOf(app);
    QVERIFY(action != nullptr);
    QVERIFY(action->isEnabled());

    auto *const fetch = fetchOf(app);
    QVERIFY(fetch != nullptr);

    QSignalSpy startedSpy(fetch, &AccountFetch::started);
    QSignalSpy endedSpy(fetch, &AccountFetch::ended);

    action->trigger();

    // Said the moment the command is given, before the bank is reached.
    QCOMPARE(startedSpy.count(), 1);
    QVERIFY(!app.statusBar()->currentMessage().isEmpty());

    QVERIFY(endedSpy.wait(sessionTimeoutMs));

    const auto outcome = endedSpy.first().first().value<AccountFetch::Outcome>();
    QCOMPARE(outcome, AccountFetch::Outcome::Skipped);

    // This account carries no backend, which is the one case the outcome stands
    // for. An account the bank holds no order for has online access and ends in
    // NothingOffered; the two used to share this value, and the window told the
    // user of both that the account had no online access.
    QVERIFY(app.statusBar()->currentMessage().contains(QStringLiteral("no online access")));
}

/**
 * An entry that cannot be invoked stays where it is and turns grey. A tool then
 * says that the command exists and that it does not grip right now; an entry
 * that is gone says nothing at all.
 */
void AppFetchTest::withoutASelectionTheActionIsVisibleAndCannotBeInvoked()
{
    Logger logger;
    Storage storage(applicationInfo());

    QVERIFY(openStorage(storage));
    QVERIFY(putAccount(storage));

    App app(&logger, &storage, applicationInfo());
    app.initialize();

    auto *const action = fetchActionOf(app);
    QVERIFY(action != nullptr);

    // On the overview, where no storage is open yet.
    QVERIFY(action->isVisible());
    QVERIFY(!action->isEnabled());

    QVERIFY(UiTestHelpers::readAccountsInto(app, storage));

    // A storage is open now, and still nothing is chosen.
    QVERIFY(action->isVisible());
    QVERIFY(!action->isEnabled());

    auto *const menu = app.findChild<QMenu *>(QStringLiteral("appAccountsMenu"));
    QVERIFY(menu != nullptr);

    QAccessibleInterface *entry = entryFor(menu, action);
    QVERIFY(entry != nullptr);
    QVERIFY(entry->state().disabled);

    QVERIFY(chooseTheAccount(app, storage));

    QVERIFY(action->isEnabled());
    QVERIFY(!entryFor(menu, action)->state().disabled);
}

/**
 * The rows a fetch brought are read back over the very path the view reads its
 * rows on, and the account stays the one that was chosen.
 */
void AppFetchTest::theTransactionViewShowsTheNewRowsWithoutChoosingTheAccountAgain()
{
    Logger logger;
    Storage storage(applicationInfo());

    QVERIFY(openStorage(storage));
    QVERIFY(putAccount(storage));
    QVERIFY(TransactionHelpers::putTransactions(storageFile(),
                                                password(),
                                                testAccountId,
                                                3,
                                                QStringLiteral("Buchung")));

    App app(&logger, &storage, applicationInfo());
    app.initialize();

    QVERIFY(chooseTheAccount(app, storage));

    auto *const transactionModel = app.findChild<TransactionTableModel *>();
    QVERIFY(transactionModel != nullptr);

    QTRY_COMPARE_WITH_TIMEOUT(transactionModel->rowCount(), 3, workerTimeoutMs);

    // What a session would have stored, put in the same place by hand.
    QVERIFY(TransactionHelpers::putTransactions(storageFile(),
                                                password(),
                                                testAccountId,
                                                2,
                                                QStringLiteral("Nachtrag")));

    auto *const fetch = fetchOf(app);
    QVERIFY(fetch != nullptr);

    Q_EMIT fetch->ended(AccountFetch::Outcome::Received, 2, QString());

    QTRY_COMPARE_WITH_TIMEOUT(transactionModel->rowCount(), 5, workerTimeoutMs);
    QCOMPARE(transactionModel->accountId(), testAccountId);
}

/**
 * The counter hangs on the number the storage reports with every read. A refresh
 * that went past that path would leave it standing on the number of before.
 */
void AppFetchTest::theCounterOfTheFilterBarCarriesTheNewNumber()
{
    Logger logger;
    Storage storage(applicationInfo());

    QVERIFY(openStorage(storage));
    QVERIFY(putAccount(storage));
    QVERIFY(TransactionHelpers::putTransactions(storageFile(),
                                                password(),
                                                testAccountId,
                                                3,
                                                QStringLiteral("Buchung")));

    App app(&logger, &storage, applicationInfo());
    app.initialize();

    QVERIFY(chooseTheAccount(app, storage));

    auto *const transactionModel = app.findChild<TransactionTableModel *>();
    QVERIFY(transactionModel != nullptr);

    QTRY_COMPARE_WITH_TIMEOUT(transactionModel->totalRows(), 3, workerTimeoutMs);

    QVERIFY(TransactionHelpers::putTransactions(storageFile(),
                                                password(),
                                                testAccountId,
                                                2,
                                                QStringLiteral("Nachtrag")));

    auto *const fetch = fetchOf(app);
    QVERIFY(fetch != nullptr);

    Q_EMIT fetch->ended(AccountFetch::Outcome::Received, 2, QString());

    QTRY_COMPARE_WITH_TIMEOUT(transactionModel->totalRows(), 5, workerTimeoutMs);

    auto *const counter = app.findChild<QLabel *>(QStringLiteral("labelTransactionCount"));
    QVERIFY(counter != nullptr);
    QVERIFY(counter->text().contains(QStringLiteral("5")));
}

/**
 * A fetch does not read, so the distribution of an error used to hand its
 * failure to the account view: the notice would have covered a tree that is in
 * order with a message about accounts that could not be read.
 *
 * Measured against an empty tree, where that notice is the one thing on screen.
 * With accounts in it the tree stands whatever the notice says, and the run
 * would pass without the distinction ever being made.
 */
void AppFetchTest::aFailureDuringAFetchIsNoFailureOfTheAccountView()
{
    Logger logger;
    Storage storage(applicationInfo());

    QVERIFY(openStorage(storage));

    App app(&logger, &storage, applicationInfo());
    app.initialize();

    QVERIFY(UiTestHelpers::readAccountsInto(app, storage));

    auto *const notice = app.findChild<QLabel *>(QStringLiteral("labelAccountsNotice"));
    QVERIFY(notice != nullptr);

    const QString noticeBefore = notice->text();
    QVERIFY(!noticeBefore.isEmpty());

    auto *const fetch = fetchOf(app);
    QVERIFY(fetch != nullptr);

    Q_EMIT fetch->started();

    Q_EMIT storage.writeFailed(ErrorCode::DatabaseFailure,
                               QStringLiteral("INSERT INTO transactions failed"));

    // The status bar is where it belongs, and the accounts side is untouched.
    QCOMPARE(app.statusBar()->currentMessage(), userMessage(ErrorCode::DatabaseFailure));
    QCOMPARE(notice->text(), noticeBefore);

    Q_EMIT fetch->ended(AccountFetch::Outcome::Failed, 0, QStringLiteral("no bank"));

    // The same failure outside a fetch still reaches the view it belongs to.
    Q_EMIT storage.writeFailed(ErrorCode::DatabaseFailure,
                               QStringLiteral("INSERT INTO transactions failed"));

    QCOMPARE(notice->text(), userMessage(ErrorCode::DatabaseFailure));
}

/**
 * The entry is reachable in three places and carries the same name in each of
 * them. The tree needs a policy of its own for that: without it a plain
 * addAction puts the entry nowhere a user could reach.
 */
void AppFetchTest::theEntriesOfTheFetchCarryAnIdentifierANameAndARole()
{
    Logger logger;
    Storage storage(applicationInfo());

    QVERIFY(openStorage(storage));

    App app(&logger, &storage, applicationInfo());
    app.initialize();

    auto *const action = fetchActionOf(app);
    QVERIFY(action != nullptr);

    QVERIFY(!action->objectName().isEmpty());
    QVERIFY(!action->text().isEmpty());

    // The key the platform offers for fetching anew, rather than one made up
    // here. No other entry of the window carries it.
    QCOMPARE(action->shortcut(), QKeySequence(QKeySequence::Refresh));

    const auto actions = app.findChildren<QAction *>();
    for (const QAction *other : actions) {
        if (other != action && !other->shortcut().isEmpty()) {
            QVERIFY(other->shortcut() != action->shortcut());
        }
    }

    auto *const menu = app.findChild<QMenu *>(QStringLiteral("appAccountsMenu"));
    QVERIFY(menu != nullptr);
    QVERIFY(menu->actions().contains(action));

    auto *const toolBar = app.findChild<QToolBar *>(QStringLiteral("appToolBar"));
    QVERIFY(toolBar != nullptr);
    QVERIFY(toolBar->actions().contains(action));

    auto *const central = app.findChild<AppCentralWidget *>();
    QVERIFY(central != nullptr);

    auto *const tree = central->accountWidget();
    QVERIFY(tree->actions().contains(action));
    QCOMPARE(tree->contextMenuPolicy(), Qt::ActionsContextMenu);

    QAccessibleInterface *entry = entryFor(menu, action);
    QVERIFY(entry != nullptr);
    QVERIFY(!entry->text(QAccessible::Name).isEmpty());
    QCOMPARE(entry->role(), QAccessible::MenuItem);
}

/**
 * A storage that is closed while a run writes into it would leave exactly the
 * half stored account the storing path goes to such lengths to prevent.
 */
void AppFetchTest::theCloseEntryCannotBeInvokedWhileAFetchRuns()
{
    Logger logger;
    Storage storage(applicationInfo());

    QVERIFY(openStorage(storage));
    QVERIFY(putAccount(storage));

    App app(&logger, &storage, applicationInfo());
    app.initialize();

    QVERIFY(chooseTheAccount(app, storage));

    auto *const closeAction = app.findChild<QAction *>(QStringLiteral("appCloseStorageAction"));
    auto *const fetchAction = fetchActionOf(app);
    auto *const wizardAction = app.findChild<QAction *>(QStringLiteral("appSetupAssistantAction"));

    QVERIFY(closeAction != nullptr);
    QVERIFY(fetchAction != nullptr);
    QVERIFY(wizardAction != nullptr);

    QVERIFY(closeAction->isEnabled());

    auto *const fetch = fetchOf(app);
    QVERIFY(fetch != nullptr);

    Q_EMIT fetch->started();

    QVERIFY(closeAction->isVisible());
    QVERIFY(!closeAction->isEnabled());
    QVERIFY(!fetchAction->isEnabled());
    QVERIFY(!wizardAction->isEnabled());

    Q_EMIT fetch->ended(AccountFetch::Outcome::Received, 0, QString());

    QVERIFY(closeAction->isEnabled());
    QVERIFY(fetchAction->isEnabled());
    QVERIFY(wizardAction->isEnabled());
}

/**
 * The application has no way of its own to end a session; the abort runs over
 * the button of the library. Ending the process under a running session would
 * reach into objects that are already gone.
 */
void AppFetchTest::theWindowIsNotClosedWhileAFetchRuns()
{
    Logger logger;
    Storage storage(applicationInfo());

    QVERIFY(openStorage(storage));
    QVERIFY(putAccount(storage));

    App app(&logger, &storage, applicationInfo());
    QVERIFY(UiTestHelpers::showTheWindow(app));

    auto *const fetch = fetchOf(app);
    QVERIFY(fetch != nullptr);

    Q_EMIT fetch->started();

    QVERIFY(!app.close());
    QVERIFY(app.isVisible());

    const QString message = app.statusBar()->currentMessage();
    QVERIFY(!message.isEmpty());

    Q_EMIT fetch->ended(AccountFetch::Outcome::Aborted, 0, QString());

    QVERIFY(app.close());
}

/**
 * The entry in the menu takes the same way as the button of the window manager.
 * Ending the application past it would leave the shutdown of the banking layer
 * waiting for a session that is waiting for this thread.
 */
void AppFetchTest::theQuitEntryGoesThroughTheSameRefusalAsClosing()
{
    Logger logger;
    Storage storage(applicationInfo());

    QVERIFY(openStorage(storage));
    QVERIFY(putAccount(storage));

    App app(&logger, &storage, applicationInfo());
    QVERIFY(UiTestHelpers::showTheWindow(app));

    auto *const fetch = fetchOf(app);
    QVERIFY(fetch != nullptr);

    auto *const quitAction = app.findChild<QAction *>(QStringLiteral("appQuitAction"));
    QVERIFY(quitAction != nullptr);

    Q_EMIT fetch->started();

    quitAction->trigger();
    QVERIFY(app.isVisible());

    Q_EMIT fetch->ended(AccountFetch::Outcome::Aborted, 0, QString());

    quitAction->trigger();
    QVERIFY(!app.isVisible());
}

/**
 * The wizard writes its accounts into the storage from outside the window, and
 * the window has no way of knowing it happened. Without a read of its own the
 * tree stays as it was until the storage is closed and opened again.
 */
void AppFetchTest::theAccountTreeTakesUpWhatWasWrittenPastTheWindow()
{
    Logger logger;
    Storage storage(applicationInfo());

    QVERIFY(openStorage(storage));
    QVERIFY(putAccount(storage));

    App app(&logger, &storage, applicationInfo());
    QVERIFY(UiTestHelpers::showTheWindow(app));
    QVERIFY(UiTestHelpers::readAccountsInto(app, storage));

    auto *const treeModel = app.findChild<AccountTreeModel *>();
    QVERIFY(treeModel != nullptr);
    QCOMPARE(treeModel->rowCount(), 1);

    const QModelIndex bank = treeModel->index(0, 0);
    QCOMPARE(treeModel->rowCount(bank), 1);

    // The second account arrives the way the wizard puts one there: straight
    // into the storage, with nothing telling the window about it.
    QVERIFY(putAccount(storage, testAccountId + 1));
    QCOMPARE(treeModel->rowCount(treeModel->index(0, 0)), 1);

    app.refreshAccounts();

    QTRY_COMPARE_WITH_TIMEOUT(treeModel->rowCount(treeModel->index(0, 0)), 2, 5000);
}

/**
 * A single shot connection only parts once its signal has fired. Every way out
 * of a read that brings no record leaves it standing, and the empty account
 * table is a case the application itself treats as an everyday one.
 *
 * The connection then took the result of the next read. That one carries
 * transactions; the tree cannot build an account from one, so it emptied itself
 * and dropped the choice of the user with it.
 */
void AppFetchTest::aRefreshThatFindsNoAccountDoesNotEmptyTheTreeLater()
{
    Logger logger;
    Storage storage(applicationInfo());

    QVERIFY(openStorage(storage));
    QVERIFY(putAccount(storage));

    App app(&logger, &storage, applicationInfo());
    QVERIFY(UiTestHelpers::showTheWindow(app));
    QVERIFY(UiTestHelpers::readAccountsInto(app, storage));

    auto *const treeModel = app.findChild<AccountTreeModel *>();
    auto *const transactionModel = app.findChild<TransactionTableModel *>();
    QVERIFY(treeModel != nullptr);
    QVERIFY(transactionModel != nullptr);
    QCOMPARE(treeModel->rowCount(), 1);

    // The account leaves the table, so the refresh below finds nothing and
    // reports no records at all.
    QVERIFY(TestHelpers::runStatement(storageFile(),
                                      password(),
                                      QStringLiteral("DELETE FROM accounts;")));

    QSignalSpy readFinishedSpy(&storage, &Storage::readFinished);

    app.refreshAccounts();
    QTRY_COMPARE_WITH_TIMEOUT(readFinishedSpy.count(), 1, workerTimeoutMs);

    // The tree kept what it had: nothing was handed to it.
    QCOMPARE(treeModel->rowCount(), 1);

    // Bookings for the account that is chosen, and a read of them. Their records
    // travel through the same signal the refresh above was listening for.
    QVERIFY(putAccount(storage));
    QVERIFY(TransactionHelpers::putTransactions(storageFile(),
                                                password(),
                                                testAccountId,
                                                3,
                                                QStringLiteral("Miete")));

    transactionModel->setAccountId(testAccountId);
    QTRY_COMPARE_WITH_TIMEOUT(transactionModel->rowCount(), 3, workerTimeoutMs);

    // The tree still shows its bank and its account. A connection left standing
    // would have handed it three bookings, which it answers with an empty tree.
    QCOMPARE(treeModel->rowCount(), 1);
    QCOMPARE(treeModel->rowCount(treeModel->index(0, 0)), 1);
}

/**
 * The tree replaces its whole content and pages through nothing, so the read
 * behind it has to bring the whole holding. It used to leave the window of a
 * read at its default of fifty: whoever keeps more accounts than that saw the
 * first fifty after every fetch and the rest nowhere, and the account he had
 * chosen was gone from the tree along with them.
 */
void AppFetchTest::theAccountTreeShowsMoreAccountsThanOneDefaultWindowHolds()
{
    constexpr int accountCount = 60;

    Logger logger;
    Storage storage(applicationInfo());

    QVERIFY(openStorage(storage));

    for (int i = 0; i < accountCount; ++i) {
        QVERIFY(putAccount(storage, testAccountId + quint32(i)));
    }

    App app(&logger, &storage, applicationInfo());
    QVERIFY(UiTestHelpers::showTheWindow(app));

    auto *const treeModel = app.findChild<AccountTreeModel *>();
    QVERIFY(treeModel != nullptr);

    QSignalSpy readFinishedSpy(&storage, &Storage::readFinished);

    app.refreshAccounts();
    QTRY_COMPARE_WITH_TIMEOUT(readFinishedSpy.count(), 1, workerTimeoutMs);

    // One bank node, because every account of the helper carries the same bank.
    QCOMPARE(treeModel->rowCount(), 1);
    QCOMPARE(treeModel->rowCount(treeModel->index(0, 0)), accountCount);
}

/**
 * The key names the application to the bank servers. A fourth field of an
 * aggregate stays empty without a word from any compiler, and the application
 * would sign on without one.
 */
void AppFetchTest::theRegistrationKeyIsNotEmptyWhenTheBankingLayerComesUp()
{
    Logger logger;
    Storage storage(applicationInfo());

    QVERIFY(openStorage(storage));

    auto withoutAKey = applicationInfo();
    withoutAKey.registrationKey.clear();

    AccountFetch fetch(withoutAKey, &storage);
    QVERIFY(fetch.initialize().isError());

    AccountFetch withAKey(applicationInfo(), &storage);
    QVERIFY2(!withAKey.initialize().isError(), "the banking layer refused a key that is there");

    // The value the application signs on with is the one place it stands.
    QVERIFY(!QString(FinTsRegistrationKey).isEmpty());
}

/**
 * Two banking instances must not extend the same user interface of the banking
 * layer: the second extension aborts the process. The window therefore keeps one
 * of its own, apart from the one the wizard holds.
 */
void AppFetchTest::theWindowComesUpBesideAnInstanceOfTheWizard()
{
    Logger logger;
    Storage storage(applicationInfo());

    QVERIFY(openStorage(storage));

    // What the wizard holds, built the way its page builds it.
    auto wizardGui = std::make_unique<BankingGui>();
    auto wizardBanking = std::make_unique<Banking>(applicationInfo());

    QVERIFY(!wizardBanking
                 ->initialize(applicationInfo().name,
                              applicationInfo().version,
                              applicationInfo().registrationKey,
                              wizardGui->getCInterface())
                 .isError());

    AccountFetch fetch(applicationInfo(), &storage);
    QVERIFY(!fetch.initialize().isError());

    // The wizard is still usable afterwards, and both go down in the order the
    // shutdown asks for.
    QSignalSpy finishedSpy(wizardBanking.get(), &Banking::finished);
    wizardBanking->accounts();
    QCOMPARE(finishedSpy.count(), 1);

    wizardBanking.reset();
    wizardGui.reset();
}

/**
 * The span the cached PIN outlives a fetch by only starts when a fetch has
 * ended. Without a caller it would never start, and the cache would stand until
 * the interface goes down.
 */
void AppFetchTest::theSpanOfTheCachedCredentialRunsAfterAFetchAndNotDuringIt()
{
    Logger logger;
    Storage storage(applicationInfo());

    QVERIFY(openStorage(storage));
    QVERIFY(putAccount(storage));

    App app(&logger, &storage, applicationInfo());
    app.initialize();

    QVERIFY(chooseTheAccount(app, storage));

    auto *const fetch = fetchOf(app);
    QVERIFY(fetch != nullptr);

    QSignalSpy endedSpy(fetch, &AccountFetch::ended);

    fetchActionOf(app)->trigger();

    QVERIFY(!fetch->isPasswordCacheExpiring());

    QVERIFY(endedSpy.wait(sessionTimeoutMs));

    // Started at the end of a fetch, whichever way it ended.
    QVERIFY(fetch->isPasswordCacheExpiring());
}

/**
 * The fetched balance reaches the tree over the read of the accounts. Without
 * that read it would sit in the storage and stay invisible until the next start.
 */
void AppFetchTest::theAccountViewShowsANewBalanceWithoutARestart()
{
    Logger logger;
    Storage storage(applicationInfo());

    QVERIFY(openStorage(storage));
    QVERIFY(putAccount(storage));

    App app(&logger, &storage, applicationInfo());
    app.initialize();

    QVERIFY(chooseTheAccount(app, storage));

    auto *const treeModel = app.findChild<AccountTreeModel *>();
    QVERIFY(treeModel != nullptr);

    const QModelIndex account = UiTestHelpers::firstAccountOf(*treeModel);
    QCOMPARE(treeModel->data(account, AccountTreeModel::BalanceRole).toDouble(), storedBalance);

    // What a session would have stored for the account.
    const auto balance = BankingHelpers::balanceFromBackend(testAccountId,
                                                            {.value = fetchedBalance});
    QVERIFY(!storage.storeItem(balance.get()).isError());

    // The figure is in the file. What follows measures the way from there onto
    // the screen and nothing else.
    QCOMPARE(TestHelpers::storageScalar(storageFile(),
                                        password(),
                                        QStringLiteral("SELECT `value` FROM balances;"))
                 .toDouble(),
             fetchedBalance);

    auto *const fetch = fetchOf(app);
    QVERIFY(fetch != nullptr);

    QSignalSpy accountsRead(&storage, &Storage::itemsReceived);

    Q_EMIT fetch->ended(AccountFetch::Outcome::Received, 0, QString());

    // Told apart on purpose: whether the accounts were read again at all, and
    // whether the tree then shows what they carry.
    QVERIFY2(accountsRead.wait(UiTestHelpers::workerTimeout),
             "the accounts were never read again after the fetch");

    QTRY_COMPARE_WITH_TIMEOUT(treeModel
                                  ->data(UiTestHelpers::firstAccountOf(*treeModel),
                                         AccountTreeModel::BalanceRole)
                                  .toDouble(),
                              fetchedBalance,
                              workerTimeoutMs);

    // The choice survives the read that brought the new figure.
    auto *const transactionModel = app.findChild<TransactionTableModel *>();
    QVERIFY(transactionModel != nullptr);
    QCOMPARE(transactionModel->accountId(), testAccountId);
}

/**
 * A fetch that brings nothing changes no view. The message is the only thing
 * that tells it apart from a fetch that never happened.
 */
void AppFetchTest::aFetchWithoutNewBookingsSaysSo()
{
    Logger logger;
    Storage storage(applicationInfo());

    QVERIFY(openStorage(storage));
    QVERIFY(putAccount(storage));

    App app(&logger, &storage, applicationInfo());
    app.initialize();

    QVERIFY(chooseTheAccount(app, storage));

    auto *const fetch = fetchOf(app);
    QVERIFY(fetch != nullptr);

    Q_EMIT fetch->ended(AccountFetch::Outcome::Received, 0, QString());

    const QString withoutAny = app.statusBar()->currentMessage();
    QVERIFY(!withoutAny.isEmpty());

    Q_EMIT fetch->ended(AccountFetch::Outcome::Received, 7, QString());

    const QString withSeven = app.statusBar()->currentMessage();
    QVERIFY(withSeven.contains(QStringLiteral("7")));
    QVERIFY(withSeven != withoutAny);

    // An account the bank holds no order for the bookings of. It brought a
    // balance and no booking, and saying "no new transactions" would send the
    // user looking for a fetch that went wrong.
    Q_EMIT fetch->ended(AccountFetch::Outcome::BalanceOnly, 0, QString());

    const QString balanceOnly = app.statusBar()->currentMessage();
    QVERIFY(!balanceOnly.isEmpty());
    QVERIFY(balanceOnly != withoutAny);

    // An account with online access whose bank holds no order for it at all.
    // Told apart from an account without online access, which is a matter of
    // the setup rather than of what the bank offers.
    Q_EMIT fetch->ended(AccountFetch::Outcome::NothingOffered, 0, QString());

    const QString nothingOffered = app.statusBar()->currentMessage();
    QVERIFY(!nothingOffered.isEmpty());
    QVERIFY(nothingOffered != withoutAny);
    QVERIFY(nothingOffered != balanceOnly);

    Q_EMIT fetch->ended(AccountFetch::Outcome::Skipped, 0, QString());

    QVERIFY(app.statusBar()->currentMessage() != nothingOffered);

    // A run that reached the storage and could not write there. Nothing of it
    // stayed behind, so the user is told to try again rather than sent looking
    // for bookings that are not in the file.
    Q_EMIT fetch->ended(AccountFetch::Outcome::StoreFailed, 0, QString());

    const QString storeFailed = app.statusBar()->currentMessage();
    QVERIFY(!storeFailed.isEmpty());
    QVERIFY(storeFailed != withoutAny);

    // The same outcome after the bookings went in and only the balance failed.
    // They are committed in a run of their own by then, so this must not read
    // like the line above: whoever fetches again on the strength of that one
    // finds those records already there and adds none of them.
    Q_EMIT fetch->ended(AccountFetch::Outcome::StoreFailed, 7, QString());

    const QString storeFailedAfterBookings = app.statusBar()->currentMessage();
    QVERIFY(!storeFailedAfterBookings.isEmpty());
    QVERIFY(storeFailedAfterBookings != storeFailed);
    QVERIFY(storeFailedAfterBookings.contains(QStringLiteral("7")));

    // Neither of them names an account or an amount.
    for (const QString &message :
         {withoutAny, withSeven, balanceOnly, nothingOffered, storeFailed, storeFailedAfterBookings}) {
        QVERIFY(!message.contains(QStringLiteral("DE02")));
        QVERIFY(!message.contains(QStringLiteral("0137075030")));
        QVERIFY(!message.contains(QStringLiteral("12,5")));
        QVERIFY(!message.contains(QStringLiteral("12.5")));
    }
}

namespace {

/**
 * Runs the given check on the modal window the application puts up, then lets it
 * go through its default button.
 *
 * A box that never comes would leave the run waiting, so the driver gives up
 * after a span of its own and the test fails instead of hanging.
 */
bool withTheModalQuestion(QObject *context,
                          const std::function<void()> &trigger,
                          const std::function<void(QMessageBox *)> &check)
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

        box->defaultButton()->click();
    });

    driver.start();
    trigger();

    return checked;
}

} // namespace

/**
 * The second entry is the one for the whole holding. It hangs on no selection:
 * a user who wants everything has nothing to choose first.
 *
 * Every account here is one without online access, which is what makes the run
 * measurable without a bank. The banking layer passes such an account over by
 * its identifier before anything is sent, and it does so for each of the three.
 */
void AppFetchTest::theCollectiveEntryRunsForEveryAccountAndHangsOnNoSelection()
{
    Logger logger;
    Storage storage(applicationInfo());

    QVERIFY(openStorage(storage));
    QVERIFY(putThreeAccounts(storage));

    App app(&logger, &storage, applicationInfo());
    app.initialize();

    auto *const action = collectiveActionOf(app);
    QVERIFY(action != nullptr);

    // On the overview, where no storage is open yet.
    QVERIFY(action->isVisible());
    QVERIFY(!action->isEnabled());

    QVERIFY(UiTestHelpers::readAccountsInto(app, storage));

    // Nothing is chosen, and that is beside the point for this one.
    QVERIFY(action->isEnabled());

    auto *const fetch = fetchOf(app);
    QVERIFY(fetch != nullptr);

    QSignalSpy startedSpy(fetch, &AccountFetch::started);
    QSignalSpy allEndedSpy(fetch, &AccountFetch::allEnded);
    QSignalSpy endedSpy(fetch, &AccountFetch::ended);

    action->trigger();

    QCOMPARE(startedSpy.count(), 1);
    QVERIFY(!app.statusBar()->currentMessage().isEmpty());

    QVERIFY(allEndedSpy.wait(sessionTimeoutMs));

    // The single fetch has an end of its own and must not be reported here as
    // well: the window would take a run of three accounts for a run of one.
    QCOMPARE(endedSpy.count(), 0);
    QCOMPARE(allEndedSpy.count(), 1);

    const auto summary = allEndedSpy.first().first().value<AccountFetch::Summary>();

    QCOMPARE(summary.skipped, 3);
    QCOMPARE(summary.fetched, 0);
    QCOMPARE(summary.failed, 0);
    QCOMPARE(summary.storedCount, 0);
}

/**
 * The entry is reachable in the menu and in the tool bar, and it carries what an
 * assistive tool needs to name it.
 */
void AppFetchTest::theCollectiveEntryCarriesAnIdentifierANameAndARole()
{
    Logger logger;
    Storage storage(applicationInfo());

    QVERIFY(openStorage(storage));

    App app(&logger, &storage, applicationInfo());
    app.initialize();

    auto *const action = collectiveActionOf(app);
    QVERIFY(action != nullptr);

    QVERIFY(!action->objectName().isEmpty());
    QVERIFY(!action->text().isEmpty());

    auto *const menu = app.findChild<QMenu *>(QStringLiteral("appAccountsMenu"));
    QVERIFY(menu != nullptr);
    QVERIFY(menu->actions().contains(action));

    auto *const toolBar = app.findChild<QToolBar *>(QStringLiteral("appToolBar"));
    QVERIFY(toolBar != nullptr);
    QVERIFY(toolBar->actions().contains(action));

    // Its own shortcut or none, but never the one of the entry beside it.
    auto *const single = fetchActionOf(app);
    QVERIFY(single != nullptr);
    if (!action->shortcut().isEmpty()) {
        QVERIFY(action->shortcut() != single->shortcut());
    }

    QAccessibleInterface *entry = entryFor(menu, action);
    QVERIFY(entry != nullptr);
    QVERIFY(!entry->text(QAccessible::Name).isEmpty());
    QCOMPARE(entry->role(), QAccessible::MenuItem);

    // Nothing is open, so it says that it does not grip rather than disappearing.
    QVERIFY(action->isVisible());
    QVERIFY(entry->state().disabled);
}

/**
 * Four figures, because three would leave a sum that does not add up: an account
 * without online access is neither fetched nor failed.
 */
void AppFetchTest::theOutcomeOfACollectiveFetchNamesFourFigures()
{
    Logger logger;
    Storage storage(applicationInfo());

    QVERIFY(openStorage(storage));
    QVERIFY(putThreeAccounts(storage));

    App app(&logger, &storage, applicationInfo());
    app.initialize();

    QVERIFY(UiTestHelpers::readAccountsInto(app, storage));

    auto *const fetch = fetchOf(app);
    QVERIFY(fetch != nullptr);

    AccountFetch::Summary summary;
    summary.fetched = 2;
    summary.skipped = 3;
    summary.failed = 4;
    summary.storedCount = 17;

    Q_EMIT fetch->allEnded(summary);

    const QString message = app.statusBar()->currentMessage();

    QVERIFY(!message.isEmpty());

    // Told apart by their values, so that a message which carries one figure
    // four times cannot pass for one that carries four.
    QVERIFY(message.contains(QStringLiteral("2")));
    QVERIFY(message.contains(QStringLiteral("3")));
    QVERIFY(message.contains(QStringLiteral("4")));
    QVERIFY(message.contains(QStringLiteral("17")));

    // The bar is read out to assistive tools, and what is spoken in a room is
    // not the place for an account or an amount.
    QVERIFY(!message.contains(QStringLiteral("DE02")));
    QVERIFY(!message.contains(QStringLiteral("0000202051")));
    QVERIFY(!message.contains(QStringLiteral("12,5")));
    QVERIFY(!message.contains(QStringLiteral("12.5")));
}

/**
 * The progress window of the library comes and goes once per institution, so it
 * is no sign of the end. The closing message is, and it must not stand there
 * while the run is still going.
 */
void AppFetchTest::theClosingMessageComesOnlyAfterTheLastAccount()
{
    Logger logger;
    Storage storage(applicationInfo());

    QVERIFY(openStorage(storage));
    QVERIFY(putThreeAccounts(storage));

    App app(&logger, &storage, applicationInfo());
    app.initialize();

    QVERIFY(UiTestHelpers::readAccountsInto(app, storage));

    auto *const fetch = fetchOf(app);
    QVERIFY(fetch != nullptr);

    AccountFetch::Summary summary;
    summary.fetched = 3;
    summary.storedCount = 17;

    const QString closing = [&] {
        Q_EMIT fetch->allEnded(summary);
        return app.statusBar()->currentMessage();
    }();

    QVERIFY(!closing.isEmpty());

    app.statusBar()->clearMessage();

    // A run that has begun and not ended. Nothing of the closing message stands
    // there, and the entries are switched off for as long.
    Q_EMIT fetch->started();

    QVERIFY(app.statusBar()->currentMessage() != closing);
    QVERIFY(!collectiveActionOf(app)->isEnabled());
    QVERIFY(!fetchActionOf(app)->isEnabled());

    Q_EMIT fetch->allEnded(summary);

    QCOMPARE(app.statusBar()->currentMessage(), closing);
    QVERIFY(collectiveActionOf(app)->isEnabled());
}

/**
 * The question belongs to the application and not to the library, so it falls
 * under the promise about entries: reachable without a mouse, named, and with
 * keeping as the button that is already chosen.
 */
void AppFetchTest::aStopBringsTheQuestionWithKeepingPreselected()
{
    Logger logger;
    Storage storage(applicationInfo());

    QVERIFY(openStorage(storage));
    QVERIFY(putThreeAccounts(storage));

    App app(&logger, &storage, applicationInfo());
    QVERIFY(UiTestHelpers::showTheWindow(app));
    QVERIFY(UiTestHelpers::readAccountsInto(app, storage));

    auto *const fetch = fetchOf(app);
    QVERIFY(fetch != nullptr);

    QPushButton *keep = nullptr;
    QPushButton *discard = nullptr;

    const bool asked = withTheModalQuestion(
        &app,
        [fetch] { Q_EMIT fetch->abortNeedsAnswer(); },
        [&](QMessageBox *box) {
            QVERIFY(!box->objectName().isEmpty());
            QVERIFY(!box->text().isEmpty());

            keep = box->findChild<QPushButton *>(QStringLiteral("appFetchKeepButton"));
            discard = box->findChild<QPushButton *>(QStringLiteral("appFetchDiscardButton"));

            QVERIFY(keep != nullptr);
            QVERIFY(discard != nullptr);

            // Keeping is the answer a stray press of the return key gives. What was
            // fetched cost minutes on the line, and it is gone for good either way.
            QCOMPARE(box->defaultButton(), keep);

            QAccessibleInterface *interface = QAccessible::queryAccessibleInterface(keep);
            QVERIFY(interface != nullptr);
            QVERIFY(!interface->text(QAccessible::Name).isEmpty());
            QCOMPARE(interface->role(), QAccessible::Button);
        });

    QVERIFY2(asked, "the window put up no question after the fetch was stopped");

    QVERIFY(keep != nullptr);
    QVERIFY(discard != nullptr);
}

/**
 * The two answers say different things, and the one that discards leaves the
 * holding exactly as it was.
 *
 * What can be measured here is the window: no session can be run in this
 * environment, so nothing arrives that could be written. That the run writes
 * nothing at all after a discard is what the count and the write signals hold.
 */
void AppFetchTest::keepingAndDiscardingSayDifferentThingsAndDiscardingWritesNothing()
{
    Logger logger;
    Storage storage(applicationInfo());

    QVERIFY(openStorage(storage));
    QVERIFY(putThreeAccounts(storage));

    App app(&logger, &storage, applicationInfo());
    app.initialize();

    QVERIFY(UiTestHelpers::readAccountsInto(app, storage));

    auto *const fetch = fetchOf(app);
    QVERIFY(fetch != nullptr);

    QSignalSpy writeFinishedSpy(&storage, &Storage::writeFinished);

    AccountFetch::Summary kept;
    kept.outcome = AccountFetch::Outcome::Aborted;
    kept.keptAfterAbort = true;
    kept.fetched = 1;
    kept.storedCount = 17;

    Q_EMIT fetch->allEnded(kept);

    const QString afterKeeping = app.statusBar()->currentMessage();

    QVERIFY(!afterKeeping.isEmpty());
    QVERIFY(afterKeeping.contains(QStringLiteral("17")));

    AccountFetch::Summary discarded;
    discarded.outcome = AccountFetch::Outcome::Aborted;
    discarded.keptAfterAbort = false;

    Q_EMIT fetch->allEnded(discarded);

    const QString afterDiscarding = app.statusBar()->currentMessage();

    QVERIFY(!afterDiscarding.isEmpty());
    QVERIFY2(afterDiscarding != afterKeeping, "keeping and discarding told the user the same thing");

    // Neither answer wrote a row of its own. A discard that had to delete
    // afterwards is what this ordering exists to avoid.
    QCOMPARE(writeFinishedSpy.count(), 0);
}

} // namespace olbaflinx::ui::tests

QTEST_MAIN(olbaflinx::ui::tests::AppFetchTest)

#include "tst_appfetch.moc"
