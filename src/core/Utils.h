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

#ifndef OLBAFLINX_UTILS_H
#define OLBAFLINX_UTILS_H

#include <QtCore/QDate>

#include <gwenhywfar/error.h>
#include <gwenhywfar/gwendate.h>

#include "Constant.h"

class Utils
{
public:
    static QDate gwenDateToQDate(const GWEN_DATE *gwenDate)
    {
        if (gwenDate) {
            auto buffer = GWEN_Buffer_new(Q_NULLPTR, 16, 0, 1);
            int rv = GWEN_Date_toStringWithTemplate(gwenDate, "DD.MM.YYYY HH:mm:ss", buffer);
            if (rv != GWEN_SUCCESS) {
                return QDate::currentDate();
            }

            auto start = GWEN_Buffer_GetStart(buffer);
            QDate qDate = QDate::fromString(QString(start), "dd.MM.yyyy HH:mm:ss");

            GWEN_Buffer_Reset(buffer);
            GWEN_Buffer_free(buffer);

            return qDate;
        }

        return QDate::currentDate();
    }

    static GWEN_DATE *qDateToGwenDate(const QDate &date)
    {
        if (!date.isValid() || date.isNull()) {
            return GWEN_Date_CurrentDate();
        }

        const auto fd = date.toString(QString(GwenDateFormat)).toLocal8Bit();
        return GWEN_Date_fromString(fd.constData());
    }
};

#endif //OLBAFLINX_UTILS_H
