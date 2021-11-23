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

#include "SetupAssistant.h"

#include "core/Banking/Banking.h"

using namespace olbaflinx::core::banking;
using namespace olbaflinx::app::banking::assistant;

SetupAssistant::SetupAssistant(QWidget *parent)
    : QWizard(parent),
      mWizardPages({})
{
    setupUi(this);

    auto configWidget = Banking::instance()->createSetupDialog(this);
    optionPage->setLayout(new QVBoxLayout());
    optionPage->layout()->setContentsMargins(QMargins());
    optionPage->layout()->addWidget(configWidget);

    mWizardPages << welcomePage << optionPage;
}

SetupAssistant::~SetupAssistant()
{
    qDeleteAll(mWizardPages);
    mWizardPages.clear();
}
