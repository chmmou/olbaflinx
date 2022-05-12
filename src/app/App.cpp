/**
 * Copyright (C) 2021, Alexander Saal <developer@olbaflinx.chm-projects.de>
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

#include <QtGui/QKeySequence>

#include <QtWidgets/QLabel>
#include <QtWidgets/QMessageBox>

#include "core/Banking/Banking.h"
#include "core/SingleApplication/SingleApplication.h"
#include "core/Storage/VaultStorage.h"

#include "App.h"

using namespace olbaflinx::app;
using namespace olbaflinx::app::pages;

using namespace olbaflinx::core;
using namespace olbaflinx::core::banking;
using namespace olbaflinx::core::storage;
using namespace olbaflinx::core::logger;

App::App(QWidget *parent, Qt::WindowFlags flags)
    : QMainWindow(parent, flags)
{
    setupUi(this);
}

App::~App() = default;

void App::initialize()
{
    bool initialized = Banking::instance()->initialize(SingleApplication::applicationName(),
                                                       "3E1B97FF72A24783EC2215B12",
                                                       SingleApplication::applicationVersion());

    if (!initialized) {
        QMessageBox::critical(this,
                              SingleApplication::applicationName(),
                              tr("Banking backend can't be initialized"));
    }

    connect(pageDataVaults, &PageDataVaults::vaultOpen, this, &App::openVault);
    connect(pageBanking, &PageBanking::vaultClosed, this, &App::vaultClosed);

    pageDataVaults->initialize(this);
}

void App::openVault()
{
    stackedWidgetMain->setCurrentIndex(1);
    pageBanking->initialize(this);
}

void App::vaultClosed()
{
    stackedWidgetMain->setCurrentIndex(0);
    VaultStorage::instance()->close();
}

void App::closeEvent(QCloseEvent *event)
{
    VaultStorage::instance()->close();
    Banking::instance()->deInitialize();

    QMainWindow::closeEvent(event);
}
