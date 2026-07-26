/**
 * Copyright (C) 2022-2025, Alexander Saal <developer@olbaflinx.chm-projects.de>
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

#include "core/Logger/Logger.h"

#include <gwenhywfar/logger.h>

#define OLBAFLINX_CORE_LOGDOMAIN "de.chm-projects.oblaflinx"
#define OLBAFLINX_CORE_LOGDOMAIN_IDENT "oblaflinx"

using namespace olbaflinx::core::logger;

Logger::Logger()
    : QObject(Q_NULLPTR)
{ }
Logger::~Logger() = default;

void Logger::enable(LoggerLevel level, const QString &logFile)
{
    if (!GWEN_Logger_IsOpen(OLBAFLINX_CORE_LOGDOMAIN)) {
        GWEN_Logger_Enable(OLBAFLINX_CORE_LOGDOMAIN, 1);
        GWEN_Logger_Open(OLBAFLINX_CORE_LOGDOMAIN,
                         OLBAFLINX_CORE_LOGDOMAIN_IDENT,
                         logFile.isEmpty() ? nullptr : logFile.toLocal8Bit().constData(),
                         logFile.isEmpty() ? GWEN_LoggerType_Console : GWEN_LoggerType_File,
                         GWEN_LoggerFacility_User);
        GWEN_Logger_SetLevel(OLBAFLINX_CORE_LOGDOMAIN, (GWEN_LOGGER_LEVEL) level);
    }
}

void Logger::disable()
{
    if (isEnabled()) {
        GWEN_Logger_Enable(OLBAFLINX_CORE_LOGDOMAIN, 0);
        GWEN_Logger_Close(OLBAFLINX_CORE_LOGDOMAIN);
    }
}

void Logger::setLevel(LoggerLevel level)
{
    if (isEnabled()) {
        GWEN_Logger_SetLevel(OLBAFLINX_CORE_LOGDOMAIN, (GWEN_LOGGER_LEVEL) level);
    }
}
void Logger::log(const QString &message)
{
    if (isEnabled()) {
        GWEN_Logger_Log(OLBAFLINX_CORE_LOGDOMAIN,
                        (GWEN_LOGGER_LEVEL) GWEN_Logger_GetLevel(OLBAFLINX_CORE_LOGDOMAIN),
                        QString(" %1").arg(message).toLocal8Bit().constData());
    }
}

bool Logger::isEnabled() const
{
    return GWEN_Logger_IsOpen(OLBAFLINX_CORE_LOGDOMAIN)
           && GWEN_Logger_IsEnabled(OLBAFLINX_CORE_LOGDOMAIN);
}
