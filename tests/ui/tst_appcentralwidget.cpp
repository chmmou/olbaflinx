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
#include "ui/Storage/StorageDialog.h"

#include "TestHelpers.h"

#include <QtTest/QtTest>

#include <QtCore/QTemporaryDir>

#include <QtWidgets/QLabel>
#include <QtWidgets/QStackedWidget>
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
using namespace olbaflinx::ui::storage;

namespace olbaflinx::ui::tests {

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

    static QString password() { return QStringLiteral("M'yF13\"stP\\$44W0$3d/"); }

    /**
     * How long a spy waits for a signal a worker thread has to produce first.
     * The call returns the moment the signal arrives, so no test sleeps for it.
     */
    static constexpr auto workerTimeout = std::chrono::seconds{30};

    [[nodiscard]] QString storageFile() const
    {
        return workingDirectory->filePath(QStringLiteral("storage.obfx"));
    }

    static ApplicationInfo applicationInfo()
    {
        return {QStringLiteral("de.chm-projects.olbaflinx.test"),
                QStringLiteral("OlbaFlinxAppCentralWidgetTest"),
                QStringLiteral("1.0.0")};
    }

    static QAction *actionOf(const App &app, const QString &name)
    {
        return app.findChild<QAction *>(name);
    }

    static QMap<QString, QVariant> accountMap()
    {
        return {{QStringLiteral("type"), 1},
                {QStringLiteral("unique_id"), 4711},
                {QStringLiteral("backend_name"), QStringLiteral("aqhbci")},
                {QStringLiteral("owner_name"), QStringLiteral("Max Mustermann")},
                {QStringLiteral("account_name"), QStringLiteral("Girokonto")},
                {QStringLiteral("currency"), QStringLiteral("EUR")},
                {QStringLiteral("iban"), QStringLiteral("DE02500105170137075030")},
                {QStringLiteral("bic"), QStringLiteral("INGDDEFF")},
                {QStringLiteral("bank_code"), QStringLiteral("50010517")},
                {QStringLiteral("bank_name"), QStringLiteral("ING-DiBa")},
                {QStringLiteral("account_number"), QStringLiteral("0137075030")},
                {QStringLiteral("balance"), 12.5}};
    }

private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

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
    items << Account::fromMap(accountMap());
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

    auto withoutIban = accountMap();
    withoutIban[QStringLiteral("account_name")] = QStringLiteral("Tagesgeld");
    withoutIban[QStringLiteral("iban")] = QString();

    BankingItems items;
    items << Account::fromMap(accountMap()) << Account::fromMap(withoutIban);
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
    items << Account::fromMap(accountMap());
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
        const auto account = olbaflinx::core::tests::TestHelpers::createFakeAccount();
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

    QSignalSpy finishedSpy(&storage, &Storage::finished);
    storage.receiveItems(Storage::StorageAccount);

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
    items << Account::fromMap(accountMap());
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

    QVERIFY(closeAction != nullptr);
    QVERIFY(assistantAction != nullptr);
    QVERIFY(newAction != nullptr);

    QVERIFY(!toolBar->isVisibleTo(&app));
    QVERIFY(!closeAction->isEnabled());
    QVERIFY(!assistantAction->isEnabled());

    // Creating a storage is the one command the first page is there for.
    QVERIFY(newAction->isEnabled());

    auto *overview = app.findChild<StorageDialog *>();
    QVERIFY(overview != nullptr);

    Q_EMIT overview->storageOpened();

    QVERIFY(toolBar->isVisibleTo(&app));
    QVERIFY(closeAction->isEnabled());
    QVERIFY(assistantAction->isEnabled());
    QVERIFY(newAction->isEnabled());

    app.closeStorage();

    QVERIFY(!toolBar->isVisibleTo(&app));
    QVERIFY(!closeAction->isEnabled());
    QVERIFY(!assistantAction->isEnabled());
}

/**
 * Three entries belong to stories that are not built yet. They exist so that the
 * menu keeps its shape once they are switched on, and they stay disabled until
 * then rather than doing nothing when pressed.
 */
void AppCentralWidgetTest::theEntriesWithoutTheirStoryStayDisabled()
{
    Logger logger;
    Storage storage(applicationInfo());

    App app(&logger, &storage);
    app.initialize();

    const auto names = QStringList{QStringLiteral("appFetchTransactionsAction"),
                                   QStringLiteral("appAccountsViewAction"),
                                   QStringLiteral("appResetLayoutAction")};

    for (const auto &name : names) {
        auto *action = actionOf(app, name);

        QVERIFY2(action != nullptr, qPrintable(name));
        QVERIFY2(!action->isEnabled(), qPrintable(name));
    }

    auto *overview = app.findChild<StorageDialog *>();
    QVERIFY(overview != nullptr);

    Q_EMIT overview->storageOpened();

    // An open storage does not bring them to life either. Their story does.
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
