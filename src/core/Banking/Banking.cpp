/**
 * Copyright (c) 2021, Alexander Saal <alexander.saal@chm-projects.de>
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

#include "Banking.h"

using namespace olbaflinx::core::banking;
using namespace olbaflinx::core::storage::account;

Banking::Banking()
    : QObject(nullptr),
      d_ptr(new BankingPrivate(this))
{

}

Banking::~Banking()
{
    if (!d_ptr.isNull()) {
        d_ptr->deleteLater();
    }
}

bool Banking::initialize(const QString &name, const QString &key, const QString &version) const
{
    return d_ptr->initializeAqBanking(name, key, version);
}

void Banking::deInitialize()
{
    d_ptr->finalizeAqBanking();
}

QWidget *Banking::createSetupDialog(QWidget *widget) const
{
    return d_ptr->createSetupDialog(widget);
}

void Banking::finalizeSetupDialog()
{
    return d_ptr->finalizeSetupDialog();
}

Account *Banking::account(quint32 uniqueId) const
{
    return d_ptr->account(uniqueId);
}

void Banking::receiveAccounts()
{
    d_ptr->receiveAccounts();
}

void Banking::receiveAccountIds()
{
    d_ptr->receiveAccountIds();
}
