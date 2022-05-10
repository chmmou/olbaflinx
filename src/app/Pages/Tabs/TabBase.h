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

#ifndef OLBAFLINX_TABBASE_H
#define OLBAFLINX_TABBASE_H

#include <QtWidgets/QWidget>

#include "ui_TabTransactions.h"
#include "core/Container.h"

namespace olbaflinx::app::pages::tabs {

using namespace olbaflinx::core;

class TabBase : public QWidget, protected Ui::UiTabTransactions
{
    Q_OBJECT

public:
    explicit TabBase(QWidget *parent = nullptr);
    ~TabBase() override;

    void setAccountId(const quint32 id);
    TransactionList transactions(bool isStandingOrder = false, bool force = false);
    int transactionCount(bool isStandingOrder = false);


private:
    quint32 m_accountId;
    TransactionList m_transactions;
};

} // namespace olbaflinx::app::pages::tabs

#endif //OLBAFLINX_TABBASE_H
