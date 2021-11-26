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

#ifndef OLBAFLINX_ACCOUNT_TREE_H
#define OLBAFLINX_ACCOUNT_TREE_H

#include <QtWidgets/QTreeWidget>

#include "core/Container.h"

using namespace olbaflinx::core;

namespace olbaflinx::app::banking
{

class AccountWidget: public QTreeWidget
{

Q_OBJECT

public:
    explicit AccountWidget(QWidget *parent = nullptr);
    ~AccountWidget() override;

    void setAccounts(const AccountList &accounts);

Q_SIGNALS:

private Q_SLOTS:

};

}

#endif // OLBAFLINX_ACCOUNT_TREE_H
