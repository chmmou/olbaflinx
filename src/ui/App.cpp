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
#include "ui/Assistant/SetupAssistant.h"
#include "ui/ErrorMessage.h"
#include "ui/Logging.h"
#include "ui/Models/AccountListModel.h"

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

using namespace ads;

class App::Private
{
public:
    explicit Private(App *app, Logger *appLogger, Storage *appStorage)
        : logger(appLogger)
        , storage(appStorage)
        , accountListModel(new AccountListModel(app))
        , ui(new Ui::UiApp)
        , dockManager(nullptr)
        , centralDockWidget(nullptr)
        , logWidgetContainer(nullptr)
        , q_ptr(app)
    {
        logger->enable();

        ui->setupUi(q_ptr);

        QApplication::setWindowIcon(QIcon(QStringLiteral(":/app/olbaflinx-logo-128")));
        q_ptr->setWindowIconText(QApplication::applicationName());

        QObject::connect(ui->appAboutAction, &QAction::triggered, q_ptr, [this] { showAbout(); });

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

    void initialize()
    {
        ui->appCentralWidget->initialize(q_ptr);

        // Kept on purpose as the reference for the pending docking rework, and
        // not activated: accountWidget() returns nullptr, so every call on aw
        // below would dereference a null pointer.
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
    AccountListModel *accountListModel;
    Ui::UiApp *ui;

    CDockManager *dockManager;
    CDockWidget *centralDockWidget;
    CAutoHideDockContainer *logWidgetContainer;

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
    d_ptr->accountListModel->setItems(items);
}

void App::showError(ErrorCode code, const QString &reason)
{
    // The technical message can name a file or a statement. It goes to the log,
    // never to the screen.
    qCWarning(lcUi) << "error from core:" << reason;

    const QString message = userMessage(code);
    if (message.isEmpty()) {
        return;
    }

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
