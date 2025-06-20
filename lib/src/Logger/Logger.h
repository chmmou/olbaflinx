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
#ifndef OLBAFLINX_CORE_LOGGER_H
#define OLBAFLINX_CORE_LOGGER_H

#include "OlbaFlinxCore.h"
#include "Singleton.h"

#include <QtCore/QObject>

namespace olbaflinx::core::logger {

class OLBAFLINX_CORE_EXPORT Logger : public QObject, public Singleton<Logger>
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

    void enable(LoggerLevel level = LoggerLevel::Notice, const QString &logFile = QString());
    void disable();

    void setLevel(LoggerLevel level);

    void log(const QString &message);

private:
    [[nodiscard]] bool isEnabled() const;

protected:
    Logger();
    Q_DISABLE_COPY(Logger)
};

} // namespace olbaflinx::core::logger

#endif //OLBAFLINX_CORE_LOGGER_H
