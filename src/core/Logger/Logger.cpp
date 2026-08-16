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

#include "core/Logger/Logger.h"

#include <gwenhywfar/logger.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QMutex>
#include <QtCore/QStandardPaths>
#include <QtCore/QTextStream>

#include <memory>

#define OLBAFLINX_CORE_LOGDOMAIN "de.chm-projects.olbaflinx"
#define OLBAFLINX_CORE_LOGDOMAIN_IDENT "olbaflinx"

using namespace olbaflinx::core::logger;

namespace {

// The message handler is a free function without state of its own, and it is
// called from every thread that logs. What it needs to reach the file therefore
// lives here, under a lock.
QMutex s_logMutex;
QFile *s_logFile = nullptr;
QtMessageHandler s_previousHandler = nullptr;
Logger *s_owner = nullptr;
bool s_lossReported = false;

/**
 * Reports the loss of the log once, through the logger that installed this
 * handler.
 *
 * The handler runs in whichever thread logged, so the signal is queued into the
 * thread of the logger rather than emitted here.
 */
void reportLoss()
{
    if (s_lossReported || s_owner == nullptr) {
        return;
    }

    s_lossReported = true;

    Logger *owner = s_owner;
    QMetaObject::invokeMethod(
        owner, [owner] { Q_EMIT owner->logFileUnavailable(); }, Qt::QueuedConnection);
}

void appendToLogFile(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    // The console keeps what it always had. The file is what a user who starts
    // the program from a menu can be asked for.
    //
    // Read under the lock and called outside it. The pointer is replaced under
    // the lock while other threads log, and a handler that logs in turn would
    // wait for a lock this call still held.
    QtMessageHandler previousHandler = nullptr;

    {
        const QMutexLocker locker(&s_logMutex);
        previousHandler = s_previousHandler;
    }

    if (previousHandler != nullptr) {
        previousHandler(type, context, message);
    }

    const QMutexLocker locker(&s_logMutex);
    if (s_logFile == nullptr || !s_logFile->isOpen()) {
        return;
    }

    QTextStream stream(s_logFile);
    stream << qFormatLogMessage(type, context, message) << '\n';
    stream.flush();

    if (s_logFile->error() != QFileDevice::NoError) {
        // A full disk or a file that went away. Closing it here keeps every
        // further entry from running into the same error, and the run goes on.
        s_logFile->close();
        reportLoss();
    }
}

} // namespace

Logger::Logger(QObject *parent)
    : QObject(parent)
{}

Logger::~Logger()
{
    // enable() puts this instance into s_owner, hands a QFile to a static
    // pointer and installs the message handler. A member of this class holds
    // none of it, so nothing of it is undone by leaving the scope: s_owner would
    // stay behind pointing at an object that is gone, and the next failed write
    // would emit a signal on it.
    //
    // Only the instance that took the log down takes it down. A second logger
    // that never enabled anything must not close the file another one keeps.
    bool owns = false;

    {
        const QMutexLocker locker(&s_logMutex);
        owns = s_owner == this;
    }

    if (owns) {
        disable();
    }
}

QString Logger::defaultLogFile()
{
    const QString location = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (location.isEmpty()) {
        return {};
    }

    return QDir(location).filePath(QStringLiteral("olbaflinx.log"));
}

void Logger::enable(LoggerLevel level, const QString &logFile)
{
    if (!GWEN_Logger_IsOpen(OLBAFLINX_CORE_LOGDOMAIN)) {
        // A path is a sequence of bytes for the file system, not text for a
        // reader. toLocal8Bit answers the locale codec, which drops what it
        // cannot map; encodeName answers what open() actually needs. On a path
        // with characters outside the locale the other way opens the wrong
        // file, or none.
        const QByteArray encodedLogFile = QFile::encodeName(logFile);

        GWEN_Logger_Enable(OLBAFLINX_CORE_LOGDOMAIN, 1);
        GWEN_Logger_Open(OLBAFLINX_CORE_LOGDOMAIN,
                         OLBAFLINX_CORE_LOGDOMAIN_IDENT,
                         logFile.isEmpty() ? nullptr : encodedLogFile.constData(),
                         logFile.isEmpty() ? GWEN_LoggerType_Console : GWEN_LoggerType_File,
                         GWEN_LoggerFacility_User);
        GWEN_Logger_SetLevel(OLBAFLINX_CORE_LOGDOMAIN, (GWEN_LOGGER_LEVEL) level);

        // Whoever opened the domain is the one who closes it. Noted here rather
        // than below, because a call without a file opens it just the same and
        // would otherwise leave no owner at all: the domain would outlive every
        // instance, and the next call would find it open, skip the open and the
        // level with it, and log to the console at the level of the first
        // call.
        const QMutexLocker locker(&s_logMutex);
        if (s_owner == nullptr) {
            s_owner = this;
        }
    }

    if (logFile.isEmpty()) {
        return;
    }

    bool lost = false;

    {
        const QMutexLocker locker(&s_logMutex);

        // The file goes with the domain and both go with one owner. An instance
        // that found the domain open is not that owner, and a file it opened
        // here would be closed by the destructor of the one that is: the owner
        // would then log to nothing while it still counts as enabled.
        if (s_logFile != nullptr || (s_owner != nullptr && s_owner != this)) {
            return;
        }

        s_owner = this;
        s_lossReported = false;

        // The directory below the writable data location does not exist before
        // the application has written anything there.
        QDir().mkpath(QFileInfo(logFile).absolutePath());

        auto file = std::make_unique<QFile>(logFile);
        if (file->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            s_logFile = file.release();
            s_previousHandler = qInstallMessageHandler(appendToLogFile);
        } else {
            s_lossReported = true;
            lost = true;
        }
    }

    // Outside the lock. A receiver that logs would otherwise wait for the lock
    // this call holds.
    if (lost) {
        Q_EMIT logFileUnavailable();
    }
}

void Logger::disable()
{
    if (isEnabled()) {
        GWEN_Logger_Enable(OLBAFLINX_CORE_LOGDOMAIN, 0);
        GWEN_Logger_Close(OLBAFLINX_CORE_LOGDOMAIN);
    }

    const QMutexLocker locker(&s_logMutex);
    if (s_logFile == nullptr) {
        s_owner = nullptr;
        return;
    }

    qInstallMessageHandler(s_previousHandler);
    s_previousHandler = nullptr;

    delete s_logFile;
    s_logFile = nullptr;
    s_owner = nullptr;
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
                        QStringLiteral(" %1").arg(message).toLocal8Bit().constData());
    }
}

bool Logger::isEnabled() const
{
    return GWEN_Logger_IsOpen(OLBAFLINX_CORE_LOGDOMAIN)
           && GWEN_Logger_IsEnabled(OLBAFLINX_CORE_LOGDOMAIN);
}
