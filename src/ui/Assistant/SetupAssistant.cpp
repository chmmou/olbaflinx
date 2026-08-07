/**
 * Copyright (C) 2022-2026, Alexander Saal <developer@olbaflinx.chm-projects.de>
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

#include "ui/Assistant/SetupAssistant.h"

#include "ui/Assistant/Pages/OptionBankingPage.h"

#include "ui_SetupAssistant.h"

using namespace olbaflinx::core;
using namespace olbaflinx::core::banking;
using namespace olbaflinx::ui::assistant;

class SetupAssistant::Private
{
public:
    explicit Private(SetupAssistant *assistant, const ApplicationInfo &applicationInfo)
        : ui(new Ui::UiSetupAssistant)
    {
        ui->setupUi(assistant);
        ui->bankingPage->initialize(applicationInfo);
    }

    ~Private() { delete ui; }

    Ui::UiSetupAssistant *ui;
};

SetupAssistant::SetupAssistant(const ApplicationInfo &applicationInfo,
                               QWidget *parent,
                               Qt::WindowFlags flags)
    : QWizard(parent, flags)
    , d_ptr(new Private(this, applicationInfo))
{}

SetupAssistant::~SetupAssistant()
{
    delete d_ptr;
}

/**
 * A cancelled wizard hands over nothing, which is what keeps it from writing
 * anything at all. QWizard leaves its result at Rejected until the last page is
 * accepted, so the check covers a run that is still going too.
 */
BankingItems SetupAssistant::selectedAccounts() const
{
    if (result() != QDialog::Accepted) {
        return {};
    }

    return d_ptr->ui->bankingPage->selectedAccounts();
}

BankingItems SetupAssistant::offeredAccounts() const
{
    if (result() != QDialog::Accepted) {
        return {};
    }

    return d_ptr->ui->bankingPage->offeredAccounts();
}
