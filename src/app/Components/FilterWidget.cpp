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

#include <QtCore/QDate>
#include <QtCore/QDebug>
#include <QtCore/QLocale>
#include <QtCore/QSignalBlocker>
#include <QtCore/QRegExp>

#include "Container.h"
#include "FilterWidget.h"

using namespace olbaflinx::core;
using namespace olbaflinx::app::components;

FilterWidget::FilterWidget(QWidget *parent, Qt::WindowFlags f)
    : QWidget(parent, f),
      mUseSearchTextAsRegEx(false)
{
    setupUi(this);

    const auto dateFormat = QLocale::system().dateFormat(QLocale::NarrowFormat);

    dateEditFrom->setDisplayFormat(dateFormat);
    dateEditTo->setDisplayFormat(dateFormat);

    whileSignalBlocking(dateEditFrom)->setDate(QDate::currentDate().addDays(-1));
    whileSignalBlocking(dateEditTo)->setDate(QDate::currentDate());

    cbxTimePeriod->addItem(tr("Today"), 0);
    cbxTimePeriod->addItem(tr("Last 7 Days"), 7);
    cbxTimePeriod->addItem(tr("Last 30 Days"), 30);
    cbxTimePeriod->addItem(tr("Last 90 Days"), 90);
    cbxTimePeriod->addItem(tr("Last 365 Days"), 365);
    cbxTimePeriod->addItem(tr("Week: This"), 0);
    cbxTimePeriod->addItem(tr("Week: Last"), 7);
    cbxTimePeriod->addItem(tr("Week: Before last"), 14);
    cbxTimePeriod->addItem(tr("Year: This"), 0);
    cbxTimePeriod->addItem(tr("Year: Last"), 1);
    cbxTimePeriod->addItem(tr("Year: Before last"), 2);
}

FilterWidget::~FilterWidget() = default;

void FilterWidget::slotTimePeriodChanged(const int timePeriod)
{
    whileSignalBlocking(dateEditFrom)->setDate(QDate::currentDate().addDays(-1));
    whileSignalBlocking(dateEditTo)->setDate(QDate::currentDate());

    const int itemData = cbxTimePeriod->itemData(timePeriod).toInt();

    switch (timePeriod) {
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
            whileSignalBlocking(dateEditFrom)->setDate(QDate::currentDate().addDays(0 - itemData));
            break;
        case 6:
        case 7:
        case 8: {

            const auto dayOfWeek = QDate::currentDate().dayOfWeek();
            const auto firstWeekDate = QDate::currentDate().addDays(1 - dayOfWeek);
            const auto endWeekDate = QDate::currentDate().addDays(7 - dayOfWeek);

            whileSignalBlocking(dateEditFrom)->setDate(firstWeekDate.addDays(0 - itemData));
            whileSignalBlocking(dateEditTo)->setDate(endWeekDate.addDays(0 - itemData));

            break;
        }
        case 9:
        case 10:
        case 11: {

            const auto dayOfYear = QDate::currentDate().dayOfYear();
            const auto firstYearDate = QDate::currentDate().addDays(1 - dayOfYear);
            const auto endYearDate = QDate::currentDate().addDays(365 - dayOfYear);

            whileSignalBlocking(dateEditFrom)->setDate(firstYearDate.addYears(0 - itemData));
            whileSignalBlocking(dateEditTo)->setDate(endYearDate.addYears(0 - itemData));

            break;
        }
        default:
            break;
    }

    Q_EMIT dateTimePeriodChanged(dateEditFrom->date(), dateEditTo->date());
}

void FilterWidget::slotDateEditChanged(const QDate &dateEdit)
{
    const auto _dateEdit = qobject_cast<QDateEdit *>(sender());
    if (_dateEdit == nullptr) {
        return;
    }
    Q_EMIT dateChanged(_dateEdit->date());
}

void FilterWidget::slotSearchTextChanged(const QString &searchText)
{
    if (mUseSearchTextAsRegEx) {
        QRegExp regExp(QRegExp::escape(searchText));
        if (regExp.isValid()) {
            Q_EMIT searchTextChanged(regExp.pattern(), true);
            return;
        }
    }

    Q_EMIT searchTextChanged(searchText, false);
}

void FilterWidget::slotUseSearchTextAsRegEx(const int checkState)
{
    mUseSearchTextAsRegEx = (checkState == Qt::Checked);
}
