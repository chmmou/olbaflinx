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
#include "ui/AppCentralWidget.h"
#include "ui/Assistant/SetupAssistant.h"
#include "ui/ErrorMessage.h"
#include "ui/Logging.h"
#include "ui/Models/AccountTreeModel.h"
#include "ui/Models/TransactionTableModel.h"
#include "ui/Storage/StorageDialog.h"

#include "ui_App.h"

#include <QtCore/QDir>

#include <QtWidgets/QApplication>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QStatusBar>

#include <qtadvanceddocking-qt6/AutoHideDockContainer.h>
#include <qtadvanceddocking-qt6/DockAreaWidget.h>
#include <qtadvanceddocking-qt6/DockManager.h>
#include <qtadvanceddocking-qt6/DockWidget.h>

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
    explicit Private(App *app, Logger *appLogger, Storage *appStorage)
        : logger(appLogger)
        , storage(appStorage)
        , accountTreeModel(new AccountTreeModel(app))
        , transactionTableModel(new TransactionTableModel(app))
        , ui(new Ui::UiApp)
        , dockManager(nullptr)
        , centralDockWidget(nullptr)
        , logWidgetContainer(nullptr)
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
        // Logger and Storage belong to whoever created the window. The logger is
        // only shut down here, neither of the two is released.
        logger->disable();

        if (dockManager) {
            dockManager->deleteLater();
        }

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
     * Three entries have no story behind them yet and stay disabled: fetching
     * transactions belongs to the next epic, the two under View to the one that
     * builds the dock areas. They are created here so that the menu keeps its
     * shape once they are switched on.
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

        ui->appFetchTransactionsAction->setEnabled(false);
        ui->appAccountsViewAction->setEnabled(false);
        ui->appResetLayoutAction->setEnabled(false);

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
        applyPage(AppCentralWidget::Page::Storages);

        // Kept on purpose as the reference for the pending docking rework. The
        // accounts sit on the second page of the central area until it is done,
        // and taking the view out of that page here would leave the page empty.
        /*CDockManager::setConfigFlags(CDockManager::DefaultBaseConfig);
        CDockManager::setConfigFlag(CDockManager::OpaqueSplitterResize, true);
        CDockManager::setConfigFlag(CDockManager::XmlCompressionEnabled, false);
        CDockManager::setConfigFlag(CDockManager::FocusHighlighting, true);
        CDockManager::setConfigFlag(CDockManager::DockAreaHasCloseButton, false);
        CDockManager::setConfigFlag(CDockManager::MiddleMouseButtonClosesTab, false);
        CDockManager::setConfigFlag(CDockManager::AllTabsHaveCloseButton, false);
        CDockManager::setConfigFlag(CDockManager::DockAreaHideDisabledButtons, true);

        CDockManager::setAutoHideConfigFlags(CDockManager::DefaultAutoHideConfig);
        CDockManager::setAutoHideConfigFlag(CDockManager::AutoHideShowOnMouseOver, true);

        dockManager = new CDockManager(q_ptr);

        centralDockWidget = new CDockWidget("centralDockWidget", q_ptr);
        centralDockWidget->setWidget(ui->appCentralWidget);

        auto centralWidgetArea = dockManager->setCentralWidget(centralDockWidget);
        centralWidgetArea->setAllowedAreas(OuterDockAreas);

        auto w = new QPlainTextEdit();
        w->setPlaceholderText("Log entries ...");
        w->setReadOnly(true);

        auto logDockWidget = new CDockWidget("Logs");
        logDockWidget->setFeature(CDockWidget::DockWidgetClosable, false);
        logDockWidget->setFeature(CDockWidget::DockWidgetFloatable, false);
        logDockWidget->setWidget(w);
        logDockWidget->setMinimumSizeHintMode(CDockWidget::MinimumSizeHintFromDockWidget);

        logWidgetContainer = dockManager->addAutoHideDockWidget(SideBarBottom, logDockWidget);

        auto accountDockWidget = new CDockWidget("Accounts");
        accountDockWidget->setFeature(CDockWidget::DockWidgetClosable, false);
        accountDockWidget->setFeature(CDockWidget::DockWidgetFloatable, false);

        auto aw = ui->appCentralWidget->accountWidget();
        aw->setHeaderHidden(true);
        aw->setFrameShape(QFrame::NoFrame);

        accountDockWidget->setWidget(aw, CDockWidget::ForceNoScrollArea);
        accountDockWidget->setSizePolicy(aw->sizePolicy());
        accountDockWidget->setMinimumSizeHintMode(
            CDockWidget::MinimumSizeHintFromContentMinimumSize);
        accountDockWidget->setMaximumSize(aw->maximumSize());

        dockManager->addDockWidget(LeftDockWidgetArea, accountDockWidget, centralWidgetArea);*/
    }

    Logger *logger;
    Storage *storage;
    AccountTreeModel *accountTreeModel;
    TransactionTableModel *transactionTableModel;
    Ui::UiApp *ui;

    CDockManager *dockManager;
    CDockWidget *centralDockWidget;
    CAutoHideDockContainer *logWidgetContainer;

    // The first page of the central area. Owned by the window through the widget
    // hierarchy; kept here because the menu reaches into it.
    StorageDialog *overview;

private:
    App *q_ptr;
};

App::App(Logger *logger, Storage *storage, QWidget *parent, const Qt::WindowFlags &flags)
    : QMainWindow(parent, flags)
    , d_ptr(new Private(this, logger, storage))
{}

App::~App()
{
    delete d_ptr;
}

void App::initialize()
{
    const QPoint pos = d_ptr->storage
                           ->setting(QStringLiteral("Position"), QStringLiteral("App"), QPoint())
                           .toPoint();
    if (!pos.isNull()) {
        move(pos);
    }

    const QSize size
        = d_ptr->storage->setting(QStringLiteral("Size"), QStringLiteral("App"), QSize()).toSize();
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
    d_ptr->storage->storeSetting(QStringLiteral("Position"), event->pos(), QStringLiteral("App"));
    QMainWindow::moveEvent(event);
}

void App::resizeEvent(QResizeEvent *event)
{
    d_ptr->storage->storeSetting(QStringLiteral("Size"), event->size(), QStringLiteral("App"));
    QMainWindow::resizeEvent(event);
}
