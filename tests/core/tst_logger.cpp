/**
 * Copyright (C) 2021-2026, Alexander Saal <developer@olbaflinx.chm-projects.de>
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

#include <QtTest/QtTest>

#include <memory>

using namespace olbaflinx::core::logger;

namespace olbaflinx::core::logger::tests {

/**
 * The logger binds a domain of the Gwenhywfar logger, which is global state. The
 * observable effect is the file it writes to, so that is what the tests read.
 * Every function closes the domain again, otherwise the order of the functions
 * would matter.
 */
class LoggerTest final : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> workingDirectory;

    QString logFile(const QString &name) const
    {
        return workingDirectory->filePath(name + QStringLiteral(".log"));
    }

    static QString contentsOf(const QString &path)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return {};
        }

        const QString contents = QString::fromUtf8(file.readAll());
        file.close();

        return contents;
    }

private Q_SLOTS:
    void init();
    void cleanup();

    void enableWritesTheMessageToTheGivenFile();
    void logWithoutEnableWritesNothing();
    void logAfterDisableWritesNothing();
    void enableTwiceKeepsTheFirstFile();
    void setLevelWithoutEnableHasNoEffect();
    void nonAsciiMessageReachesTheFile();
};

void LoggerTest::init()
{
    workingDirectory = std::make_unique<QTemporaryDir>();
    QVERIFY(workingDirectory->isValid());

    // The domain may still be open from a function that failed midway.
    Logger().disable();
}

void LoggerTest::cleanup()
{
    Logger().disable();
    workingDirectory.reset();
}

void LoggerTest::enableWritesTheMessageToTheGivenFile()
{
    const auto file = logFile("enabled");

    Logger logger;
    logger.enable(Logger::Notice, file);
    logger.log(QStringLiteral("a message worth keeping"));
    logger.disable();

    QVERIFY(QFile::exists(file));
    QVERIFY(contentsOf(file).contains(QStringLiteral("a message worth keeping")));
}

/**
 * The failure case. Without an open domain there is nothing to write to, and the
 * call has to stay silent instead of opening one of its own.
 */
void LoggerTest::logWithoutEnableWritesNothing()
{
    const auto file = logFile("neverEnabled");

    Logger logger;
    logger.log(QStringLiteral("this must not appear anywhere"));

    QVERIFY(!QFile::exists(file));
}

void LoggerTest::logAfterDisableWritesNothing()
{
    const auto file = logFile("disabled");

    Logger logger;
    logger.enable(Logger::Notice, file);
    logger.log(QStringLiteral("before"));
    logger.disable();

    logger.log(QStringLiteral("after"));

    const QString contents = contentsOf(file);

    QVERIFY(contents.contains(QStringLiteral("before")));
    QVERIFY(!contents.contains(QStringLiteral("after")));
}

/**
 * enable guards on GWEN_Logger_IsOpen. A second call therefore keeps the domain
 * as it stands and does not move the output to another file.
 */
void LoggerTest::enableTwiceKeepsTheFirstFile()
{
    const auto first = logFile("first");
    const auto second = logFile("second");

    Logger logger;
    logger.enable(Logger::Notice, first);
    logger.enable(Logger::Debug, second);
    logger.log(QStringLiteral("goes to the first file"));
    logger.disable();

    QVERIFY(contentsOf(first).contains(QStringLiteral("goes to the first file")));
    QVERIFY(!QFile::exists(second));
}

void LoggerTest::setLevelWithoutEnableHasNoEffect()
{
    const auto file = logFile("levelWithoutEnable");

    Logger logger;
    logger.setLevel(Logger::Debug);
    logger.log(QStringLiteral("still nothing to write"));

    QVERIFY(!QFile::exists(file));
}

void LoggerTest::nonAsciiMessageReachesTheFile()
{
    const auto file = logFile("nonAscii");

    Logger logger;
    logger.enable(Logger::Notice, file);
    logger.log(QStringLiteral("Überweisung an Müller-Groß über 12,50 Euro"));
    logger.disable();

    QVERIFY(contentsOf(file).contains(QStringLiteral("Überweisung an Müller-Groß über 12,50 Euro")));
}

} // namespace olbaflinx::core::logger::tests

QTEST_APPLESS_MAIN(olbaflinx::core::logger::tests::LoggerTest)

#include "tst_logger.moc"
