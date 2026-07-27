/**
 * Copyright (C) 2022-2026, Alexander Saal <developer@olbaflinx.chm-projects.de>
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
#pragma once

#include "core/OlbaFlinxCore.h"
#include "core/Singleton.h"

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

    /**
     * Enables the logging functionality for the application, specifying the desired log level
     * and an optional log file where the logs should be written.
     *
     * @param level The log level to use. It determines the severity of messages that should be logged.
     *              Possible values are defined in the LoggerLevel enumeration, such as Notice, Debug, Error, etc.
     * @param logFile A QString that specifies the path to the log file. If the log file path is empty,
     *                the logs will be written to the console. Otherwise, they will be written to the specified file.
     */
    void enable(LoggerLevel level = LoggerLevel::Notice, const QString &logFile = QString());

    /**
     * Disables the logging functionality for the application.
     */
    void disable();

    /**
     * Updates the current logging level for the application. This setting controls the minimum severity
     * of messages that will be logged. Only messages with severity equal to or higher than the specified
     * level will be logged.
     *
     * @param level The desired logging level. Possible values are defined in the LoggerLevel enumeration
     *              (e.g., Emergency, Debug, Notice, Error, etc.).
     */
    void setLevel(LoggerLevel level);

    /**
     * Logs a message to the configured log output if logging is enabled.
     *
     * @param message The message to be logged. It is a QString object containing the textual information
     *                that will be written to the log output.
     */
    void log(const QString &message);

private:
    [[nodiscard]] bool isEnabled() const;

protected:
    Logger();
    Q_DISABLE_COPY(Logger)
};

} // namespace olbaflinx::core::logger
