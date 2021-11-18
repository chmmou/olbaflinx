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

#include "TransactionTab.h"

#include "app/Components/FilterWidget.h"

using namespace olbaflinx::app::components;
using namespace olbaflinx::app::banking;

TransactionTab::TransactionTab(QWidget *parent)
    : QWidget(parent)
{
    setupUi(this);

    connect(filterWidget,
            &FilterWidget::searchTextChanged,
            this,
            &TransactionTab::searchTextChanged
    );
    connect(filterWidget,
            &FilterWidget::dateTimePeriodChanged,
            this,
            &TransactionTab::dateTimePeriodChanged
    );
    connect(filterWidget, &FilterWidget::dateChanged, this, &TransactionTab::dateChanged);
}

TransactionTab::~TransactionTab() = default;

void TransactionTab::searchTextChanged(const QString &searchText, const bool isRegularExpression)
{
    // @todo
}

void TransactionTab::dateTimePeriodChanged(const QDate &from, const QDate &to)
{
    // @todo
}
void TransactionTab::dateChanged(const QDate &date)
{
    // @todo
}
