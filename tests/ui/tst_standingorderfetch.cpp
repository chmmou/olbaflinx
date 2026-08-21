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

#include "ui/StandingOrderFetch.h"

#include "core/ApplicationInfo.h"
#include "core/Banking/Account/Account.h"
#include "core/Logger/Logger.h"
#include "core/Storage/Storage.h"
#include "ui/AccountFetch.h"
#include "ui/App.h"
#include "ui/AppCentralWidget.h"
#include "ui/BankingSession.h"
#include "ui/Models/AccountTreeModel.h"
#include "ui/Models/StandingOrderTableModel.h"

#include "StandingOrderHelpers.h"
#include "TestHelpers.h"
#include "UiTestHelpers.h"

#include <gwenhywfar/db.h>
#include <gwenhywfar/gui.h>

#include <QtTest/QtTest>

#include <QtCore/QTemporaryDir>

#include <QtGui/QAccessible>
#include <QtGui/QAccessibleInterface>
#include <QtGui/QAction>

#include <QtWidgets/QApplication>
#include <QtWidgets/QMenu>
#include <QtWidgets/QStatusBar>
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

namespace olbaflinx::ui::tests {

using namespace olbaflinx::core::tests;

namespace {

constexpr quint32 testAccountId = 4711;

/** The name a run puts into the cache of the interface to watch it. */
constexpr auto cachedCredentialName = "probe-token";

} // namespace

/**
 * The way from the entry a user reaches to the standing orders he sees
 * afterwards.
 *
 * A session needs a bank, and these runs have none. What can be measured without
 * one is everything around it: that the entries are wired at all, what their
 * state says while nothing is chosen, and what the window does with an outcome
 * once it arrives. The outcome therefore travels through a signal of its own,
 * which a test raises the way a finished session would.
 */
class StandingOrderFetchTest final : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> workingDirectory;
    std::unique_ptr<QTemporaryDir> bankingHome;

    static QString password() { return TestHelpers::password(); }

    static constexpr int workerTimeoutMs = UiTestHelpers::workerTimeoutMs;
    static constexpr auto workerTimeout = std::chrono::seconds{30};

    /**
     * A session is answered by a bank that is not there, so it needs the whole
     * span a network call may take before it gives up.
     */
    static constexpr int sessionTimeoutMs = 60000;

    static ApplicationInfo applicationInfo()
    {
        auto info = TestHelpers::applicationInfo(QStringLiteral("OlbaFlinxStandingOrderFetchTest"));
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
     * sends a session to a bank, which no run here can reach.
     */
    [[nodiscard]] bool putAccount(Storage &storage, quint32 uniqueId = testAccountId) const
    {
        auto map = TestHelpers::accountMapWith(uniqueId, 100.0);
        map[QStringLiteral("backend_name")] = QStringLiteral("aqnone");

        return !storage.storeItem(Account::fromMap(map).get()).isError();
    }

    /** Runs one write and says whether it went through. */
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

    static QAction *actionOf(const App &app, const QString &name)
    {
        return app.findChild<QAction *>(name);
    }

    static QAction *standingOrderActionOf(const App &app)
    {
        return actionOf(app, QStringLiteral("appFetchStandingOrdersAction"));
    }

    static QAction *collectiveStandingOrderActionOf(const App &app)
    {
        return actionOf(app, QStringLiteral("appFetchAllStandingOrdersAction"));
    }

    static StandingOrderFetch *fetchOf(const App &app)
    {
        return app.findChild<StandingOrderFetch *>();
    }

    static StandingOrderTableModel *modelOf(const App &app)
    {
        return app.findChild<StandingOrderTableModel *>();
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

    static QAccessibleInterface *entryFor(QMenu *menu, QAction *action)
    {
        const int index = menu->actions().indexOf(action);
        if (index < 0) {
            return nullptr;
        }

        QAccessibleInterface *menuInterface = QAccessible::queryAccessibleInterface(menu);

        return menuInterface == nullptr ? nullptr : menuInterface->child(index);
    }

private Q_SLOTS:
    void init();
    void cleanup();

    void theEntriesCarryAnIdentifierANameAndARole();
    void theEntryForOneAccountIsOffWhileNothingIsChosen();
    void aRunningFetchOfTheBookingsSwitchesTheEntriesOff();
    void theStatusBarSaysThatAFetchIsRunning();
    void theViewShowsWhatAFetchStoredWithoutAskingTheUser();
    void anAccountWithoutOrdersReadsDifferentlyFromAFailedFetch();
    void anAbortReadsDifferentlyFromAFailure();
    void theWindowStaysOpenWhileAFetchRuns();
    void closingTheStorageEmptiesTheCachedCredential();
};

void StandingOrderFetchTest::init()
{
    workingDirectory = std::make_unique<QTemporaryDir>();
    bankingHome = std::make_unique<QTemporaryDir>();

    QVERIFY(workingDirectory->isValid());
    QVERIFY(bankingHome->isValid());

    // The banking layer writes its configuration into the home directory. A run
    // must not reach the one of whoever started it.
    qputenv("AQBANKING_HOME", bankingHome->path().toLocal8Bit());
    qputenv("GWEN_HOME", bankingHome->path().toLocal8Bit());
}

void StandingOrderFetchTest::cleanup()
{
    workingDirectory.reset();
    bankingHome.reset();
}

/**
 * The two commands stand beside those of the bookings and are told apart from
 * them by their identifier, which is what a run from outside reaches them by.
 */
void StandingOrderFetchTest::theEntriesCarryAnIdentifierANameAndARole()
{
    Logger logger;
    Storage storage(applicationInfo());

    QVERIFY(openStorage(storage));

    App app(&logger, &storage, applicationInfo());
    app.initialize();

    auto *const action = standingOrderActionOf(app);
    auto *const collective = collectiveStandingOrderActionOf(app);

    QVERIFY(action != nullptr);
    QVERIFY(collective != nullptr);

    QVERIFY(!action->objectName().isEmpty());
    QVERIFY(!collective->objectName().isEmpty());
    QVERIFY(!action->text().isEmpty());
    QVERIFY(!collective->text().isEmpty());

    // Told apart from the two of the bookings, which is what the user of a
    // screen reader has to go by as much as a run from outside.
    auto *const bookings = actionOf(app, QStringLiteral("appFetchTransactionsAction"));
    auto *const allBookings = actionOf(app, QStringLiteral("appFetchAllTransactionsAction"));

    QVERIFY(bookings != nullptr);
    QVERIFY(allBookings != nullptr);

    QVERIFY(action->objectName() != bookings->objectName());
    QVERIFY(collective->objectName() != allBookings->objectName());
    QVERIFY(action->text() != bookings->text());
    QVERIFY(collective->text() != allBookings->text());

    auto *const menu = app.findChild<QMenu *>(QStringLiteral("appAccountsMenu"));
    QVERIFY(menu != nullptr);
    QVERIFY(menu->actions().contains(action));
    QVERIFY(menu->actions().contains(collective));

    auto *const toolBar = app.findChild<QToolBar *>(QStringLiteral("appToolBar"));
    QVERIFY(toolBar != nullptr);
    QVERIFY(toolBar->actions().contains(action));
    QVERIFY(toolBar->actions().contains(collective));

    auto *const central = app.findChild<AppCentralWidget *>();
    QVERIFY(central != nullptr);
    QVERIFY(central->accountWidget()->actions().contains(action));

    QAccessibleInterface *entry = entryFor(menu, action);
    QVERIFY(entry != nullptr);
    QVERIFY(!entry->text(QAccessible::Name).isEmpty());
    QCOMPARE(entry->role(), QAccessible::MenuItem);

    // Switched off rather than hidden, so that a screen reader can say that the
    // command exists and does not apply right now.
    QVERIFY(action->isVisible());
}

/**
 * The command for one account hangs on a choice in the tree. The one over all of
 * them does not: whoever wants the whole holding has nothing to pick first.
 */
void StandingOrderFetchTest::theEntryForOneAccountIsOffWhileNothingIsChosen()
{
    Logger logger;
    Storage storage(applicationInfo());

    QVERIFY(openStorage(storage));
    QVERIFY(putAccount(storage));

    App app(&logger, &storage, applicationInfo());
    app.initialize();

    auto *const action = standingOrderActionOf(app);
    auto *const collective = collectiveStandingOrderActionOf(app);

    QVERIFY(action != nullptr);
    QVERIFY(collective != nullptr);

    // No storage is open yet, so neither of them applies.
    QVERIFY(!action->isEnabled());
    QVERIFY(!collective->isEnabled());

    QVERIFY(UiTestHelpers::readAccountsInto(app, storage));

    auto *const central = app.findChild<AppCentralWidget *>();
    auto *const treeModel = app.findChild<AccountTreeModel *>();

    QVERIFY(central != nullptr);
    QVERIFY(treeModel != nullptr);

    // The tree holds an account, and nothing is chosen in it.
    QVERIFY(!action->isEnabled());
    QVERIFY(collective->isEnabled());

    central->accountWidget()->setCurrentIndex(UiTestHelpers::firstAccountOf(*treeModel));

    QVERIFY(action->isEnabled());
    QVERIFY(collective->isEnabled());
}

/**
 * One fetch is out at a time, whichever kind it is: the banking instance belongs
 * to one thread while it sends, and the entries say so before a user can start a
 * second one.
 */
void StandingOrderFetchTest::aRunningFetchOfTheBookingsSwitchesTheEntriesOff()
{
    Logger logger;
    Storage storage(applicationInfo());

    QVERIFY(openStorage(storage));
    QVERIFY(putAccount(storage));

    App app(&logger, &storage, applicationInfo());
    app.initialize();

    QVERIFY(chooseTheAccount(app, storage));

    auto *const action = standingOrderActionOf(app);
    auto *const collective = collectiveStandingOrderActionOf(app);
    auto *const bookings = app.findChild<AccountFetch *>();

    QVERIFY(action != nullptr);
    QVERIFY(collective != nullptr);
    QVERIFY(bookings != nullptr);

    QVERIFY(action->isEnabled());
    QVERIFY(collective->isEnabled());

    Q_EMIT bookings->started();

    QVERIFY(!action->isEnabled());
    QVERIFY(!collective->isEnabled());

    Q_EMIT bookings->ended(AccountFetch::Outcome::Received, 0, QString());

    QVERIFY(action->isEnabled());
    QVERIFY(collective->isEnabled());

    // And the other way round: a running standing order fetch keeps the fetch of
    // the bookings from starting.
    auto *const bookingAction = actionOf(app, QStringLiteral("appFetchTransactionsAction"));
    QVERIFY(bookingAction != nullptr);

    auto *const fetch = fetchOf(app);
    QVERIFY(fetch != nullptr);

    Q_EMIT fetch->started();

    QVERIFY(!bookingAction->isEnabled());
    QVERIFY(!action->isEnabled());

    Q_EMIT fetch->ended(StandingOrderFetch::Outcome::Received, 0, QString());

    QVERIFY(bookingAction->isEnabled());
    QVERIFY(action->isEnabled());
}

/**
 * The command is acknowledged before anything goes out, and the outcome replaces
 * that word once the run is over. Without the first one the window would stand
 * silent while the starting point is read.
 */
void StandingOrderFetchTest::theStatusBarSaysThatAFetchIsRunning()
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

    QSignalSpy endedSpy(fetch, &StandingOrderFetch::ended);

    standingOrderActionOf(app)->trigger();

    QVERIFY(!app.statusBar()->currentMessage().isEmpty());

    QVERIFY(endedSpy.wait(sessionTimeoutMs));

    // The account has no online access, so it was passed over before anything
    // was sent, and the message says that rather than reporting a failure.
    QVERIFY(!app.statusBar()->currentMessage().isEmpty());
    QCOMPARE(endedSpy.constFirst().at(0).value<StandingOrderFetch::Outcome>(),
             StandingOrderFetch::Outcome::Skipped);
}

/**
 * The rows a fetch stored are in the file and the view knows nothing of them. It
 * is asked again, and the user has nothing to do for it.
 */
void StandingOrderFetchTest::theViewShowsWhatAFetchStoredWithoutAskingTheUser()
{
    Logger logger;
    Storage storage(applicationInfo());

    QVERIFY(openStorage(storage));
    QVERIFY(putAccount(storage));

    App app(&logger, &storage, applicationInfo());
    app.initialize();

    QVERIFY(chooseTheAccount(app, storage));

    auto *const model = modelOf(app);
    QVERIFY(model != nullptr);

    QTRY_VERIFY_WITH_TIMEOUT(!model->isReading(), workerTimeoutMs);
    QCOMPARE(model->rowCount(), 0);

    // What a fetch would have written by the time it reports its outcome.
    QVERIFY(storeAndWait(storage,
                         StandingOrderHelpers::orderRun(testAccountId, 2),
                         {testAccountId, true}));

    Q_EMIT fetchOf(app)->ended(StandingOrderFetch::Outcome::Received, 2, QString());

    QTRY_COMPARE_WITH_TIMEOUT(model->rowCount(), 2, workerTimeoutMs);
}

/**
 * Both lead into the same empty view, and the message is what tells them apart.
 * Without it the user of a failed fetch would read his account as one without
 * standing orders.
 */
void StandingOrderFetchTest::anAccountWithoutOrdersReadsDifferentlyFromAFailedFetch()
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

    Q_EMIT fetch->started();
    Q_EMIT fetch->ended(StandingOrderFetch::Outcome::Received, 0, QString());

    const QString withoutOrders = app.statusBar()->currentMessage();
    QVERIFY(!withoutOrders.isEmpty());

    Q_EMIT fetch->started();
    Q_EMIT fetch->ended(StandingOrderFetch::Outcome::Failed, 0, QString());

    const QString afterFailure = app.statusBar()->currentMessage();
    QVERIFY(!afterFailure.isEmpty());

    QVERIFY(withoutOrders != afterFailure);

    // The third case: the bank carries no request of this kind for the account,
    // which is neither an account without orders nor a failure.
    Q_EMIT fetch->started();
    Q_EMIT fetch->ended(StandingOrderFetch::Outcome::NotOffered, 0, QString());

    const QString notOffered = app.statusBar()->currentMessage();
    QVERIFY(!notOffered.isEmpty());

    QVERIFY(notOffered != withoutOrders);
    QVERIFY(notOffered != afterFailure);
}

/**
 * A fetch the user stopped is not a fetch that went wrong, and the window has to
 * say which of the two happened.
 */
void StandingOrderFetchTest::anAbortReadsDifferentlyFromAFailure()
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

    Q_EMIT fetch->started();
    Q_EMIT fetch->ended(StandingOrderFetch::Outcome::Aborted, 0, QString());

    const QString afterAbort = app.statusBar()->currentMessage();
    QVERIFY(!afterAbort.isEmpty());

    Q_EMIT fetch->started();
    Q_EMIT fetch->ended(StandingOrderFetch::Outcome::Failed, 0, QString());

    QVERIFY(afterAbort != app.statusBar()->currentMessage());

    // The same over several accounts, where the summary carries the difference.
    auto stopped = StandingOrderFetch::Summary{};
    stopped.outcome = StandingOrderFetch::Outcome::Aborted;

    Q_EMIT fetch->started();
    Q_EMIT fetch->allEnded(stopped);

    const QString afterCollectiveAbort = app.statusBar()->currentMessage();
    QVERIFY(!afterCollectiveAbort.isEmpty());

    auto failed = StandingOrderFetch::Summary{};
    failed.outcome = StandingOrderFetch::Outcome::Failed;

    Q_EMIT fetch->started();
    Q_EMIT fetch->allEnded(failed);

    QVERIFY(afterCollectiveAbort != app.statusBar()->currentMessage());
}

/**
 * Ending the process under a running session would reach into objects that are
 * already being taken down. The window says so instead of closing.
 */
void StandingOrderFetchTest::theWindowStaysOpenWhileAFetchRuns()
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

    Q_EMIT fetch->started();

    QVERIFY(!app.close());
    QVERIFY(!app.statusBar()->currentMessage().isEmpty());

    Q_EMIT fetch->ended(StandingOrderFetch::Outcome::Received, 0, QString());

    QVERIFY(app.close());
}

/**
 * The interface belongs to the window and outlives the storage. A PIN entered
 * for one storage must not still be cached while the next one is open, and it
 * makes no difference which kind of fetch put it there.
 */
void StandingOrderFetchTest::closingTheStorageEmptiesTheCachedCredential()
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

    // The interface comes up with the first fetch, so the cache is only there
    // once one has run.
    QSignalSpy endedSpy(fetch, &StandingOrderFetch::ended);

    standingOrderActionOf(app)->trigger();

    QVERIFY(endedSpy.wait(sessionTimeoutMs));

    GWEN_DB_NODE *const cache = GWEN_Gui_GetPasswordDb(GWEN_Gui_GetGui());
    QVERIFY(cache != nullptr);

    GWEN_DB_SetCharValue(cache, GWEN_DB_FLAGS_OVERWRITE_VARS, cachedCredentialName, "1234");
    QVERIFY(GWEN_DB_GetCharValue(cache, cachedCredentialName, 0, nullptr) != nullptr);

    auto *const closeAction = actionOf(app, QStringLiteral("appCloseStorageAction"));
    QVERIFY(closeAction != nullptr);
    QVERIFY(closeAction->isEnabled());

    closeAction->trigger();

    QVERIFY(GWEN_DB_GetCharValue(cache, cachedCredentialName, 0, nullptr) == nullptr);
}

} // namespace olbaflinx::ui::tests

QTEST_MAIN(olbaflinx::ui::tests::StandingOrderFetchTest)

#include "tst_standingorderfetch.moc"
