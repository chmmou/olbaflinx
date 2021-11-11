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

#include "Transaction.h"

#include "app/Components/FilterWidget.h"

using namespace olbaflinx::app::components;
using namespace olbaflinx::app::banking;

Transaction::Transaction(QWidget *parent)
    : QWidget(parent)
{
    setupUi(this);

    connect(filterWidget, &FilterWidget::searchTextChanged, this, &Transaction::searchTextChanged);
    connect(filterWidget, &FilterWidget::dateTimePeriodChanged, this, &Transaction::dateTimePeriodChanged);
    connect(filterWidget, &FilterWidget::dateChanged, this, &Transaction::dateChanged);
}

Transaction::~Transaction() = default;

void Transaction::searchTextChanged(const QString &searchText, const bool isRegularExpression)
{
    // @todo
}

void Transaction::dateTimePeriodChanged(const QDate &from, const QDate &to)
{
    // @todo
}
void Transaction::dateChanged(const QDate &date)
{
    // @todo
}
