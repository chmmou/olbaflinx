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

#ifndef OLBAFLINX_TABTRANSACTIONS_H
#define OLBAFLINX_TABTRANSACTIONS_H

#include <QWidget>

#include "ui_Transaction.h"

namespace olbaflinx::app::pages::tabs
{
class TabTransactions : public QWidget, private Ui::UiTransaction
{
    Q_OBJECT

public:
    explicit TabTransactions(QWidget *parent = nullptr);
    ~TabTransactions() override;

};

}
// namespace olbaflinx::app::pages
#endif // OLBAFLINX_TABTRANSACTIONS_H
