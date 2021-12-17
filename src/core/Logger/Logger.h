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

#ifndef OLBAFLINX_LOGGER_H
#define OLBAFLINX_LOGGER_H

#include <QtCore/QObject>

#include "core/Singleton.h"

namespace olbaflinx::core::logger
{
class Logger: public QObject, public Singleton<Logger>
{
    Q_OBJECT
    friend class Singleton<Logger>;

public:
    enum LoggerLevel {
        Emergency = 0,
        Alert,
        Critical,
        Error,
        Warning,
        Notice,
        Info,
        Debug,
        Verbose
    };
    Q_ENUM(LoggerLevel)

    ~Logger() override;

    void enable(LoggerLevel level = LoggerLevel::Notice);
    void disable();

    void setLevel(LoggerLevel level);

    void log(const QString & message);

private:
    bool isEnabled() const;

protected:
    Logger();
    Q_DISABLE_COPY(Logger)
};
}

#endif // OLBAFLINX_LOGGER_H
