/**
 * Copyright (c) 2021, Alexander Saal <developer@olbaflinx.chm-projects.de>
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
#include <QtGui/QPixmap>

#include "SetupAssistant.h"

#include "core/Banking/Banking.h"
#include "core/Storage/Storage.h"

using namespace olbaflinx::core::banking;
using namespace olbaflinx::core::storage;
using namespace olbaflinx::app::banking::assistant;

SetupAssistant::SetupAssistant(QWidget *parent)
    : QWizard(parent)
{
    setupUi(this);

    setWizardStyle(QWizard::ModernStyle);

    QPixmap logoPixmap(":/app/olbaflinx-logo");
    logoPixmap = logoPixmap.scaled(
        64,
        64,
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation
    );

    setPixmap(QWizard::LogoPixmap, logoPixmap);

    verticalSpacerOptionPage->changeSize(1, 1, QSizePolicy::Fixed, QSizePolicy::Fixed);

    optionPage->initialize();
}

SetupAssistant::~SetupAssistant()
{
    Banking::instance()->finalizeSetupDialog();
}

void SetupAssistant::closeEvent(QCloseEvent *event)
{
    Banking::instance()->finalizeSetupDialog();
    QDialog::closeEvent(event);
}

void SetupAssistant::done(int result)
{
    Banking::instance()->finalizeSetupDialog();

    connect(
        Banking::instance(),
        &Banking::accountsReceived,
        Storage::instance(),
        &Storage::storeAccounts
    );

    Banking::instance()->receiveAccounts();

    QWizard::done(result);
}
