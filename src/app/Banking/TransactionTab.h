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
#ifndef OLBAFLINX_TRANSACTION_TAB_H
#define OLBAFLINX_TRANSACTION_TAB_H

#include <QtWidgets/QWidget>

#include "ui_Transaction.h"

namespace olbaflinx::app::banking
{

class TransactionTab: public QWidget, private Ui::UiTransaction
{

Q_OBJECT

public:
    explicit TransactionTab(QWidget *parent = nullptr);
    ~TransactionTab() override;

private Q_SLOTS:
    void searchTextChanged(const QString &searchText, bool isRegularExpression);
    void dateTimePeriodChanged(const QDate &from, const QDate &to);
    void dateChanged(const QDate &date);
};

} // olbaflinx::app::transaction

#endif //OLBAFLINX_TRANSACTION_TAB_H
