/**
 * Copyright (C) 2022-2026, Alexander Saal <developer@olbaflinx.chm-projects.de>
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the
 * GNU General Public License as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
 * even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "ui/App.h"

#include "core/Banking/Banking.h"
#include "core/Logger/Logger.h"
#include "core/Storage/Storage.h"
#include "ui/AccountFetch.h"
#include "ui/AppCentralWidget.h"
#include "ui/Assistant/SetupAssistant.h"
#include "ui/ErrorMessage.h"
#include "ui/Logging.h"
#include "ui/Models/AccountTreeModel.h"
#include "ui/Models/TransactionTableModel.h"
#include "ui/Storage/StorageDialog.h"

#include "ui_App.h"

#include <QtCore/QDir>

#include <QtGui/QAccessible>
#include <QtGui/QAccessibleAnnouncementEvent>
#include <QtGui/QCloseEvent>

#include <QtWidgets/QApplication>
#include <QtWidgets/QLayout>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QStatusBar>

#include <qtadvanceddocking-qt6/DockAreaWidget.h>
#include <qtadvanceddocking-qt6/DockManager.h>
#include <qtadvanceddocking-qt6/DockWidget.h>

namespace {

/**
 * The names under which a saved layout finds its areas again.
 *
 * The dock manager takes the object name of an area as the key of the saved
 * state, and it takes the title for it unless one is set. A title is
 * translatable, so a layout saved in one language would no longer be found in
 * another. These names are set apart from the titles and never change.
 */
const QString TransactionDockName = QStringLiteral("transactionDock");
const QString AccountDockName = QStringLiteral("accountDock");

/**
 * Where the window keeps what it remembers between two runs.
 *
 * The arrangement of the areas sits beside the position and the size of the
 * window, in plain settings and not in the encrypted storage: a layout is no
 * secret, and it has to be readable before any storage is opened.
 */
const QString WindowGroup = QStringLiteral("App");
const QString PositionKey = QStringLiteral("Position");
const QString SizeKey = QStringLiteral("Size");
const QString DockLayoutKey = QStringLiteral("DockLayout");

/**
 * The number a saved arrangement carries along.
 *
 * The dock manager compares it on restore and refuses a state that carries a
 * different one. Raising it is how a rework of the areas retires the layouts of
 * every earlier run at once, instead of applying them to areas they were never
 * written for.
 */
constexpr int DockLayoutVersion = 1;

} // namespace

using namespace olbaflinx::core;
using namespace olbaflinx::ui;
using namespace olbaflinx::ui::assistant;

using namespace olbaflinx::core::storage;
using namespace olbaflinx::core::banking;
using namespace olbaflinx::core::logger;
using namespace olbaflinx::ui::models;
using namespace olbaflinx::ui::storage;

using namespace ads;

class App::Private
{
public:
    explicit Private(App *app, Logger *appLogger, Storage *appStorage, ApplicationInfo info)
        : logger(appLogger)
        , storage(appStorage)
        , accountTreeModel(new AccountTreeModel(app))
        , transactionTableModel(new TransactionTableModel(app))
        , fetch(new AccountFetch(std::move(info), appStorage, app))
        , ui(new Ui::UiApp)
        , dockManager(nullptr)
        , centralDockWidget(nullptr)
        , accountDockWidget(nullptr)
        , overview(nullptr)
        , q_ptr(app)
    {
        // Connected before the log is opened, because a file that cannot be
        // opened reports it during the call.
        QObject::connect(logger, &Logger::logFileUnavailable, q_ptr, [this] {
            // Not the way of an error from core: that way leads into the log
            // that is missing.
            q_ptr->statusBar()->showMessage(
                App::tr("No log is being kept. The program runs on, but a report about a "
                        "failure will carry no cause."));
        });

        logger->enable(Logger::LoggerLevel::Notice, Logger::defaultLogFile());

        ui->setupUi(q_ptr);

        QApplication::setWindowIcon(QIcon(QStringLiteral(":/app/olbaflinx-logo-128")));
        q_ptr->setWindowIconText(QApplication::applicationName());

        // Every error core reports on an asynchronous path ends up here. Without
        // this the signal had no receiver at all and the user saw nothing.
        QObject::connect(storage, &Storage::errorOccurred, q_ptr, &App::showError);
    }

    ~Private()
    {
        // Here and not in a close event: the entry for quitting ends the program
        // without one, and an arrangement that only survives the window button
        // would be lost on the other way out.
        saveDockLayout();

        // Logger and Storage belong to whoever created the window. The logger is
        // only shut down here, neither of the two is released.
        logger->disable();

        delete ui;
    }

    void showAbout() const
    {
        QMessageBox::about(q_ptr,
                           App::tr("About %1").arg(QApplication::applicationName()),
                           App::tr("<h3>%1 %2</h3>"
                                   "<p>Multibank-capable online banking software for Linux.</p>"
                                   "<p><a href=\"%3\">%3</a></p>")
                               .arg(QApplication::applicationName(),
                                    QApplication::applicationVersion(),
                                    QApplication::organizationDomain()));
    }

    /**
     * Wires the menu and fills the tool bar.
     *
     * One entry has no story behind it yet and stays disabled: fetching
     * transactions belongs to the next epic. It is created here so that the menu
     * keeps its shape once it is switched on.
     */
    void setUpActions()
    {
        QObject::connect(ui->appAboutAction, &QAction::triggered, q_ptr, [this] { showAbout(); });

        QObject::connect(ui->appNewStorageAction, &QAction::triggered, q_ptr, [this] {
            overview->addStorage();
        });

        QObject::connect(ui->appCloseStorageAction, &QAction::triggered, q_ptr, [this] {
            q_ptr->closeStorage();
        });

        QObject::connect(ui->appQuitAction, &QAction::triggered, q_ptr, [] {
            QApplication::quit();
        });

        // The window does not know what a wizard needs to be built. The assembly
        // does, so the request travels there and the result comes back through
        // setAccounts like any other.
        QObject::connect(ui->appSetupAssistantAction, &QAction::triggered, q_ptr, [this] {
            Q_EMIT q_ptr->setupAssistantRequested();
        });

        QObject::connect(overview, &StorageDialog::storageOpened, q_ptr, [this] {
            applyPage(AppCentralWidget::Page::Banking);
        });

        QObject::connect(overview, &StorageDialog::message, q_ptr, &App::showMessage);

        QObject::connect(ui->appResetLayoutAction, &QAction::triggered, q_ptr, [this] {
            resetDockLayout();
        });

        ui->appFetchTransactionsAction->setEnabled(false);

        ui->appToolBar->addAction(ui->appSetupAssistantAction);
        ui->appToolBar->addAction(ui->appFetchTransactionsAction);
        ui->appToolBar->addSeparator();
        ui->appToolBar->addAction(ui->appCloseStorageAction);
    }

    /**
     * Shows a page and puts the controls into the state that belongs to it.
     *
     * Every command in the tool bar needs an open storage, so the bar itself
     * only belongs on the second page. The menu entries stay where they are and
     * turn grey instead, so that the menu does not change shape underneath the
     * user while he learns it.
     */
    void applyPage(AppCentralWidget::Page page)
    {
        ui->appCentralWidget->setPage(page);

        // A message belongs to the page it was raised on. "Nothing was found,
        // import your accounts" says nothing on the overview, where there is no
        // storage to import into.
        q_ptr->statusBar()->clearMessage();

        const bool storageIsOpen = page == AppCentralWidget::Page::Banking;

        ui->appToolBar->setVisible(storageIsOpen);
        ui->appCloseStorageAction->setEnabled(storageIsOpen);
        ui->appSetupAssistantAction->setEnabled(storageIsOpen);

        // The areas only stand on the second page, so there is nothing to put
        // back on the first.
        ui->appResetLayoutAction->setEnabled(storageIsOpen);

        // Where the keyboard starts on this page. The focus chain is a ring, so
        // which of the two areas comes first is decided by where the walk
        // begins, not by their order in the chain: without this it begins at the
        // area the dock manager built first, and that has to be the central one
        // because the library refuses any other as the first. The accounts stand
        // left of the transactions and are what a user picks from, so the walk
        // starts there and reaches the transactions next.
        if (storageIsOpen) {
            ui->appCentralWidget->accountWidget()->setFocus(Qt::OtherFocusReason);
        }

        // Held back from the start until the areas are on screen, and said once.
        // Repeating it every time a storage is opened would nag about something
        // that was over with the first arrangement that got saved.
        if (storageIsOpen && dockLayoutFellBack) {
            dockLayoutFellBack = false;

            q_ptr->statusBar()->showMessage(
                App::tr("Your arrangement of the areas could not be restored. The standard "
                        "arrangement is in place, and there is nothing you need to do."));
        }
    }

    /**
     * Turns what is picked in the tree into the account the transactions are
     * shown for.
     *
     * A bank node is no account: the tree answers its account roles with an
     * invalid value, and that is what tells the two apart. Whatever the state,
     * the notice of the empty transaction view is set along with it, so that the
     * right words are in place by the time a read comes back with nothing.
     */
    void applySelection(const QModelIndex &index)
    {
        const QVariant uniqueId = accountTreeModel->data(index, AccountTreeModel::UniqueIdRole);

        if (!index.isValid()) {
            transactionTableModel->setAccountId(0);
            ui->appCentralWidget->setTransactionNotice(
                AppCentralWidget::TransactionNotice::NoAccountSelected);
            return;
        }

        if (!uniqueId.isValid()) {
            transactionTableModel->setAccountId(0);
            ui->appCentralWidget->setTransactionNotice(
                AppCentralWidget::TransactionNotice::BankSelected);
            return;
        }

        transactionTableModel->setAccountId(uniqueId.toUInt());
        ui->appCentralWidget->setTransactionNotice(
            AppCentralWidget::TransactionNotice::AccountWithoutTransactions);
    }

    /**
     * A single click on an account is what shows its transactions.
     *
     * The tree is refilled whenever the accounts are read, and an account that
     * the user has since deselected is gone from it. The selection then points
     * nowhere, which is a state of its own and not a bank node, so the reset of
     * the model is followed up here rather than waiting for a click.
     */
    void setUpAccountSelection()
    {
        auto *const view = ui->appCentralWidget->accountWidget();
        auto *const selection = view->selectionModel();

        QObject::connect(selection,
                         &QItemSelectionModel::currentChanged,
                         q_ptr,
                         [this](const QModelIndex &current, const QModelIndex &) {
                             applySelection(current);
                         });

        QObject::connect(accountTreeModel, &QAbstractItemModel::modelReset, q_ptr, [this, view] {
            applySelection(view->currentIndex());
        });
    }

    /**
     * Builds the two dock areas of the second page.
     *
     * The manager gets the page as its parent and not the window. With a
     * QMainWindow as parent it makes itself the central widget, and that would
     * push out the stack which carries the overview on its first page.
     *
     * Two things about the order. The configuration flags are static and only
     * reach a manager that is built after them. And a central area has to be the
     * first area the manager is given; the library refuses it once another one
     * stands.
     *
     * The log area of the earlier draft is not built here. It belongs to a later
     * epic, and an area that shows nothing would take room from the two that do.
     */
    void setUpDockAreas()
    {
        CDockManager::setConfigFlags(CDockManager::DefaultBaseConfig);
        CDockManager::setConfigFlag(CDockManager::OpaqueSplitterResize, true);
        CDockManager::setConfigFlag(CDockManager::XmlCompressionEnabled, false);
        CDockManager::setConfigFlag(CDockManager::FocusHighlighting, true);
        CDockManager::setConfigFlag(CDockManager::DockAreaHasCloseButton, false);
        CDockManager::setConfigFlag(CDockManager::MiddleMouseButtonClosesTab, false);
        CDockManager::setConfigFlag(CDockManager::AllTabsHaveCloseButton, false);
        CDockManager::setConfigFlag(CDockManager::DockAreaHideDisabledButtons, true);

        auto *const page = ui->appCentralWidget->bankingPage();
        dockManager = new CDockManager(page);

        centralDockWidget = new CDockWidget(dockManager, App::tr("Transactions"));
        centralDockWidget->setObjectName(TransactionDockName);
        centralDockWidget->setWidget(ui->appCentralWidget->transactionPanel());

        auto *const centralArea = dockManager->setCentralWidget(centralDockWidget);
        centralArea->setAllowedAreas(OuterDockAreas);

        // Dragging an area is a matter for the mouse; the library offers no key
        // for it. It stays a convenience: every function of the window is
        // reachable without it, and whoever loses his way in a layout gets the
        // grouping back through the menu.
        accountDockWidget = new CDockWidget(dockManager, App::tr("Accounts"));
        accountDockWidget->setObjectName(AccountDockName);
        accountDockWidget->setFeature(CDockWidget::DockWidgetClosable, false);
        accountDockWidget->setFeature(CDockWidget::DockWidgetFloatable, false);
        accountDockWidget->setWidget(ui->appCentralWidget->accountPanel(),
                                     CDockWidget::ForceNoScrollArea);

        dockManager->addDockWidget(LeftDockWidgetArea, accountDockWidget, centralArea);

        // Last, because taking the two panels over emptied the layout of the
        // page. Handing the manager over before that would have put it beside
        // the very widgets it has just taken.
        page->layout()->addWidget(dockManager);

        // What the user gets back when he asks for the grouping again. Taken
        // here, so that the way to it is the arrangement just built and not a
        // second description of it that could drift away.
        defaultDockLayout = dockManager->saveState(DockLayoutVersion);
    }

    /**
     * Puts the accounts side back where a restore may have taken it from.
     *
     * The feature that marks an area as not closable takes the close button on
     * its tab and nothing besides. A restore applies the saved open state
     * without asking the area about it, and an area the saved state does not
     * know at all is closed and taken out of its dock area. Both count as a
     * successful restore, so falling back to the default arrangement never
     * catches them, and without the accounts there is nothing left to choose an
     * account with.
     */
    void ensureAccountsVisible()
    {
        if (accountDockWidget == nullptr) {
            return;
        }

        if (accountDockWidget->dockAreaWidget() == nullptr) {
            dockManager->addDockWidget(LeftDockWidgetArea,
                                       accountDockWidget,
                                       centralDockWidget->dockAreaWidget());
        }

        if (accountDockWidget->isClosed()) {
            accountDockWidget->toggleView(true);
        }
    }

    void saveDockLayout() const
    {
        if (dockManager == nullptr) {
            return;
        }

        storage->storeSetting(DockLayoutKey, dockManager->saveState(DockLayoutVersion), WindowGroup);
    }

    /**
     * Brings the arrangement of the last run back, or leaves the default one.
     *
     * Whether a saved arrangement still fits is what the restore answers; there
     * is no check of our own beside it. It refuses an empty or unreadable state,
     * one from a format or a version it does not know, and one that misses an
     * area the window carries, and it tries the whole state before it changes
     * anything, so nothing is ever applied in halves.
     */
    void restoreDockLayout()
    {
        const QByteArray state = storage->setting(DockLayoutKey, WindowGroup, QByteArray())
                                     .toByteArray();

        if (state.isEmpty()) {
            return;
        }

        if (!dockManager->restoreState(state, DockLayoutVersion)) {
            qCWarning(lcUi) << "the saved dock layout was refused, the default one stands";

            // Told, not shown. The window opens on the overview, and a word about
            // areas the user cannot see yet would be cleared by the very step that
            // brings them up.
            dockLayoutFellBack = true;
        }

        ensureAccountsVisible();
    }

    /**
     * Puts the grouping back the way the window opens with it.
     *
     * The way out of an arrangement the user can no longer undo. Falling back to
     * the default only catches a state the restore refuses, and an arrangement
     * can be perfectly valid and still leave him stuck. Saved right away, so that
     * the next start does not hand him back what he has just left.
     */
    void resetDockLayout()
    {
        dockManager->restoreState(defaultDockLayout, DockLayoutVersion);

        ensureAccountsVisible();
        saveDockLayout();
    }

    void initialize()
    {
        ui->appCentralWidget->initialize(q_ptr);
        ui->appCentralWidget->setAccountModel(accountTreeModel);

        transactionTableModel->setStorage(storage);
        ui->appCentralWidget->setTransactionModel(transactionTableModel);

        setUpAccountSelection();

        // The overview used to be a window of its own, put up next to this one by
        // main. It is the first page of the central area now. It needs the
        // storage, which is why it is built here and not in the central widget.
        overview = new StorageDialog(storage, q_ptr);
        ui->appCentralWidget->setStorageOverview(overview);
        overview->initialize(q_ptr);

        setUpActions();
        setUpDockAreas();
        applyPage(AppCentralWidget::Page::Storages);

        // After the areas stand. There is nothing to restore an arrangement onto
        // before that.
        restoreDockLayout();
    }

    Logger *logger;
    Storage *storage;
    AccountTreeModel *accountTreeModel;
    TransactionTableModel *transactionTableModel;

    // Owned by the window through the object hierarchy. It holds the banking
    // instance of the window and comes up on the first fetch, so a window that
    // never fetches never reaches the banking layer.
    AccountFetch *fetch;

    Ui::UiApp *ui;

    // Owned by the second page through the widget hierarchy. The two areas are
    // owned by the manager once they are docked; they are kept here because the
    // restore of a saved layout has to reach the accounts side again.
    CDockManager *dockManager;
    CDockWidget *centralDockWidget;
    CDockWidget *accountDockWidget;
    QByteArray defaultDockLayout;
    bool dockLayoutFellBack = false;

    // The first page of the central area. Owned by the window through the widget
    // hierarchy; kept here because the menu reaches into it.
    StorageDialog *overview;

private:
    App *q_ptr;
};

App::App(Logger *logger,
         Storage *storage,
         ApplicationInfo applicationInfo,
         QWidget *parent,
         const Qt::WindowFlags &flags)
    : QMainWindow(parent, flags)
    , d_ptr(new Private(this, logger, storage, std::move(applicationInfo)))
{
    // Messages reach the bar from four places, and it swaps its text without a
    // sound. This signal is the one point all four pass through. An empty text
    // means the message was taken away, and there is nothing to announce.
    connect(statusBar(), &QStatusBar::messageChanged, this, [this](const QString &message) {
        if (message.isEmpty()) {
            return;
        }

        QAccessibleAnnouncementEvent announcement(statusBar(), message);
        QAccessible::updateAccessibility(&announcement);
    });
}

App::~App()
{
    delete d_ptr;
}

void App::initialize()
{
    const QPoint pos = d_ptr->storage->setting(PositionKey, WindowGroup, QPoint()).toPoint();
    if (!pos.isNull()) {
        move(pos);
    }

    const QSize size = d_ptr->storage->setting(SizeKey, WindowGroup, QSize()).toSize();
    if (!size.isNull() && size.isValid()) {
        resize(size);
    }

    d_ptr->initialize();
}

void App::setAccounts(const BankingItems &items)
{
    d_ptr->accountTreeModel->setItems(items);
}

void App::closeStorage()
{
    // The page is what says whether a storage is open. Asking the storage itself
    // would answer for the file, and the entry is reachable through its shortcut
    // long before a file was ever opened.
    if (d_ptr->ui->appCentralWidget->page() == AppCentralWidget::Page::Storages) {
        return;
    }

    d_ptr->storage->close();
    d_ptr->accountTreeModel->setItems({});

    // A choice of account does not outlive the storage it was made in. Emptying
    // the tree takes the selection with it, and the transactions of the account
    // that was shown go with it as well. Neither does the filter: it survives a
    // change of account, not the storage it was set in.
    d_ptr->transactionTableModel->setAccountId(0);
    d_ptr->ui->appCentralWidget->resetTransactionFilter();

    // Building the overview is the moment an entry whose file went away leaves
    // the list, so the way back is a good moment to build it.
    d_ptr->overview->reload();

    d_ptr->applyPage(AppCentralWidget::Page::Storages);
}

void App::showError(ErrorCode code, const QString &reason)
{
    // A read that found no record is not a failure. The storage reports it
    // through the same signal as one, with the code for "nothing found", and
    // taken as a failure it would hold the views away from the very notices that
    // are meant for the case: a storage without accounts, an account without
    // transactions, and a filter without a match. The models are empty at this
    // point, which is all those notices need.
    //
    // It is noted rather than reported, and not as a warning: a log that calls
    // it an error says the opposite of what happened.
    if (code == ErrorCode::NotFound) {
        qCDebug(lcUi) << "a read came back empty:" << reason;
        return;
    }

    // The technical message can name a file or a statement, and one out of a
    // foreign library is not translated either. It goes to the log, never to the
    // screen; what the user reads is made from the code alone.
    qCWarning(lcUi) << "error from core:" << reason;

    const QString message = userMessage(code);
    if (message.isEmpty()) {
        return;
    }

    statusBar()->showMessage(message);

    // A failed read is not an empty storage, and the views must not fall into
    // the notice that says nothing is there.
    //
    // Which view it belongs to is what the transaction model answers: while it
    // is reading, the failure is about the transactions, and the accounts on the
    // left are readable. A notice at that view would point at a holding that is
    // in order and hide the tree that shows it.
    if (d_ptr->ui->appCentralWidget->page() == AppCentralWidget::Page::Banking
        && !d_ptr->transactionTableModel->isReading()) {
        d_ptr->ui->appCentralWidget->showAccountsUnreadable(message);
    }
}

void App::showMessage(const QString &message)
{
    statusBar()->showMessage(message);
}

bool App::event(QEvent *event)
{
    return QMainWindow::event(event);
}

void App::moveEvent(QMoveEvent *event)
{
    d_ptr->storage->storeSetting(PositionKey, event->pos(), WindowGroup);
    QMainWindow::moveEvent(event);
}

void App::resizeEvent(QResizeEvent *event)
{
    d_ptr->storage->storeSetting(SizeKey, event->size(), WindowGroup);
    QMainWindow::resizeEvent(event);
}

void App::closeEvent(QCloseEvent *event)
{
    QMainWindow::closeEvent(event);
}
