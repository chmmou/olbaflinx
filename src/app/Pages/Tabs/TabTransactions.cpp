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

#include "TabTransactions.h"

using namespace olbaflinx::app::pages::tabs;

TabTransactions::TabTransactions(QWidget *parent)
    : TabBase(parent, false)
    , m_transactionViewModel(Q_NULLPTR)
{
    m_transactionViewModel = new TransactionViewModel(treeViewTransactions, false);
    treeViewTransactions->setModel(m_transactionViewModel);
    treeViewTransactions->setUniformRowHeights(true);

    connect(this, &TabBase::accountChanged, this, &TabTransactions::accountWasChanged);
}

TabTransactions::~TabTransactions() = default;

void TabTransactions::accountWasChanged(const quint32 accountId)
{
    m_transactionViewModel->clear();

    const auto items = transactions();
    m_transactionViewModel->setTransactions(accountId, items);
}

void TabTransactions::reset()
{
    m_transactionViewModel->clear();
    treeViewTransactions->reset();
}
