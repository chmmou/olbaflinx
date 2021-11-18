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
#ifndef OLBAFLINX_FILTERWIDGET_H
#define OLBAFLINX_FILTERWIDGET_H

#include <QWidget>

#include "ui_FilterWidget.h"

namespace olbaflinx::app::components
{

class FilterWidget: public QWidget, private Ui::UiFilterWidget
{
Q_OBJECT

public:
    explicit FilterWidget(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
    ~FilterWidget() override;

Q_SIGNALS:
    void dateTimePeriodChanged(const QDate &from, const QDate &to);
    void dateChanged(const QDate &date);
    void searchTextChanged(const QString &searchText, const bool isRegularExpression);
    void errorOccurred(const int reason, const QString &message);

public Q_SLOTS:
    void slotTimePeriodChanged(const int timePeriod);
    void slotDateEditChanged(const QDate &dateEdit);
    void slotSearchTextChanged(const QString &searchText);
    void slotUseSearchTextAsRegEx(const int checkState);

private:
    bool mUseSearchTextAsRegEx;

};

} // olbaflinx::app::components

#endif //OLBAFLINX_FILTERWIDGET_H
