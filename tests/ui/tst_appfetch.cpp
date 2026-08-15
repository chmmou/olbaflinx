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

#include <QtWidgets/QLabel>
#include <QtWidgets/QMenu>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QTreeView>

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
     * sends a session, and the session puts up the progress dialog the banking
     * library brings. Under the offscreen platform that dialog takes the process
     * down while it paints, so no run here may bring one about. What the banking
     * layer does with such an account is measurable all the same: it passes it
     * over by its identifier, before anything is sent.
     */
    [[nodiscard]] bool putAccount(Storage &storage) const
    {
        auto map = TestHelpers::accountMapWith(testAccountId, storedBalance);
        map[QStringLiteral("backend_name")] = QStringLiteral("aqnone");

        const auto account = Account::fromMap(map);

        return !storage.storeItem(account.get()).isError();
    }

    static QAction *fetchActionOf(const App &app)
    {
        return app.findChild<QAction *>(QStringLiteral("appFetchTransactionsAction"));
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
    void theRegistrationKeyIsNotEmptyWhenTheBankingLayerComesUp();
    void theWindowComesUpBesideAnInstanceOfTheWizard();
    void theSpanOfTheCachedCredentialRunsAfterAFetchAndNotDuringIt();
    void theAccountViewShowsANewBalanceWithoutARestart();
    void aFetchWithoutNewBookingsSaysSo();
};

/**
 * AqBanking keeps its configuration below AQBANKING_HOME. Without pointing that
 * at a directory of our own, every run would write into the configuration of
 * whoever started it.
 */
void AppFetchTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);

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

    Q_EMIT storage.errorOccurred(ErrorCode::DatabaseFailure,
                                 QStringLiteral("INSERT INTO transactions failed"));

    // The status bar is where it belongs, and the accounts side is untouched.
    QCOMPARE(app.statusBar()->currentMessage(), userMessage(ErrorCode::DatabaseFailure));
    QCOMPARE(notice->text(), noticeBefore);

    Q_EMIT fetch->ended(AccountFetch::Outcome::Failed, 0, QStringLiteral("no bank"));

    // The same failure outside a fetch still reaches the view it belongs to.
    Q_EMIT storage.errorOccurred(ErrorCode::DatabaseFailure,
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

    // Neither of them names an account or an amount.
    for (const QString &message : {withoutAny, withSeven}) {
        QVERIFY(!message.contains(QStringLiteral("DE02")));
        QVERIFY(!message.contains(QStringLiteral("0137075030")));
        QVERIFY(!message.contains(QStringLiteral("12,5")));
        QVERIFY(!message.contains(QStringLiteral("12.5")));
    }
}

} // namespace olbaflinx::ui::tests

QTEST_MAIN(olbaflinx::ui::tests::AppFetchTest)

#include "tst_appfetch.moc"
