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

#include <QtCore/QObject>

namespace olbaflinx::core::logger {

/**
 * Binds the Gwenhywfar logger to the application.
 *
 * Ownership: the creator owns the instance. If a parent is set, the parent
 * releases it, otherwise the enclosing scope does. It releases no instance that
 * belongs to someone else.
 *
 * Between enable() and disable() the class does hold something of its own, and
 * it holds it statically: the log file, the previous message handler and a
 * pointer to the instance that opened them. An instance that is destroyed
 * without disable() takes them down in its destructor, so a caller need not do
 * it; calling disable() explicitly stays the clearer way and is what the
 * application does.
 */
class OLBAFLINX_CORE_EXPORT Logger : public QObject
{
    Q_OBJECT

public:
    explicit Logger(QObject *parent = nullptr);

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
     * The file the application logs to when no other one is named.
     *
     * It sits in the writable data location of the application, so that a user
     * who is asked for a log has somewhere to fetch it from. A console has no
     * such place: whoever starts the program from a menu never sees one.
     *
     * An absolute path, empty when the platform names no such location.
     */
    [[nodiscard]] static QString defaultLogFile();

    /**
     * With a file named, the messages of the Qt logging categories go there as
     * well as to the console. Without one, both stay on the console.
     *
     * A file that cannot be opened is reported through logFileUnavailable() and
     * stops nothing: the caller keeps running and the console keeps its output.
     *
     * Only messages of the given level and above are logged.
     */
    void enable(LoggerLevel level = LoggerLevel::Notice, const QString &logFile = QString());

    void disable();

    /**
     * Only messages of this level and above are logged from now on.
     */
    void setLevel(LoggerLevel level);

    /**
     * Does nothing while logging is disabled.
     */
    void log(const QString &message);

Q_SIGNALS:
    /**
     * No log is being kept, and the run goes on without one.
     *
     * Raised at most once between enable() and disable(), so that a caller can
     * tell the user about it once instead of at every entry that is lost.
     */
    void logFileUnavailable();

private:
    [[nodiscard]] bool isEnabled() const;

    Q_DISABLE_COPY(Logger)
};

} // namespace olbaflinx::core::logger
