/**
* Copyright (C) 2022, Alexander Saal <developer@olbaflinx.chm-projects.de>
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

#include "TabBase.h"
#include <QDebug>
#include <QMetaObject>

using namespace olbaflinx::app::pages::tabs;

TabBase::TabBase(QWidget *parent)
    : QWidget(parent)
    , m_accountId(0)
    , m_transactions({})
{
    setupUi(this);
}

TabBase::~TabBase() = default;

void TabBase::setAccountId(const quint32 id)
{
    m_accountId = id;
}

TransactionList TabBase::transactions(const Type type) const
{
    // result caching
    return m_transactions;
}
