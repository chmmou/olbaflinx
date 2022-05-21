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
#include <QtWidgets/QMessageBox>

#include "SetupAssistant.h"

#include "core/Banking/OnlineBanking.h"

#include "core/Container.h"

using namespace olbaflinx::core::banking;
using namespace olbaflinx::core::storage;
using namespace olbaflinx::app::assistant;

SetupAssistant::SetupAssistant(QWidget *parent)
    : QWizard(parent)
{
    setupUi(this);

    setWizardStyle(QWizard::ModernStyle);

    QPixmap logoPixmap(":/app/olbaflinx-logo");
    logoPixmap = logoPixmap.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    setPixmap(QWizard::LogoPixmap, logoPixmap);

    optionPage->initialize();
}

SetupAssistant::~SetupAssistant() = default;

void SetupAssistant::done(int result)
{
    if (result == SetupAssistant::Rejected) {
        QWizard::done(result);
        return;
    }

    const auto accountIds = optionPage->selectedAccountIds();
    if (accountIds.isEmpty()) {
        QMessageBox::critical(this,
                              tr("Setup Assistant"),
                              tr("You have not yet selected any accounts to import!"));
        return;
    }

    AccountList accounts = {};
    for (const quint32 uniqueId : qAsConst(accountIds)) {
        accounts.append(OnlineBanking::instance()->account(uniqueId));
    }

    Q_EMIT accountsReceived(accounts);

    qDeleteAll(accounts);
    accounts.clear();

    QWizard::done(result);
}
