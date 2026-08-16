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

#include "core/Logging.h"

#include "TestHelpers.h"

#include <QtTest/QtTest>

#include <memory>

using namespace olbaflinx::core::logger;

namespace olbaflinx::core::logger::tests {

using namespace olbaflinx::core::tests;

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
    void initTestCase();
    void init();
    void cleanup();

    void enableWritesTheMessageToTheGivenFile();
    void logWithoutEnableWritesNothing();
    void logAfterDisableWritesNothing();
    void enableTwiceKeepsTheFirstFile();
    void setLevelWithoutEnableHasNoEffect();
    void nonAsciiMessageReachesTheFile();
    void nonAsciiPathReachesTheFile();
    void aCategoryMessageReachesTheSameFile();
    void aFileThatCannotBeOpenedIsReportedAndStopsNothing();
    void theDefaultFileSitsInTheDataLocationOfTheApplication();
    void aLoggerDestroyedWithoutDisableTakesTheLogDownWithIt();
    void aLoggerEnabledWithoutAFileLeavesTheDomainToTheNextOne();
    void aSecondLoggerTakesNeitherTheDomainNorTheFileFromTheFirst();
};

void LoggerTest::initTestCase()
{
    // Logger::defaultLogFile() sits below the data location of the application,
    // and one function here measures against it. Without this the run would
    // reach the home directory of whoever started it.
    QStandardPaths::setTestModeEnabled(true);

    QVERIFY(TestHelpers::useTemporaryHome());
}

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
    const auto file = logFile("withoutEnable");

    Logger logger;
    logger.log(QStringLiteral("this must not appear anywhere"));

    // Nothing has been opened, so nothing can have been written.
    QVERIFY(!QFile::exists(file));

    // The file is handed over afterwards, which is what makes the assertion
    // above provable rather than a statement about a file nothing in this binary
    // ever opens: whatever the call did before, it did not reach this file, and
    // a run that had kept the message would show it here.
    logger.enable(Logger::Notice, file);
    logger.log(QStringLiteral("this one belongs in the file"));
    logger.disable();

    const QString contents = contentsOf(file);

    QVERIFY(contents.contains(QStringLiteral("this one belongs in the file")));
    QVERIFY(!contents.contains(QStringLiteral("this must not appear anywhere")));
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

    // Held against the same file afterwards, so that the silence above is shown
    // and not merely asserted about a path nothing was ever handed.
    logger.enable(Logger::Notice, file);
    logger.log(QStringLiteral("written after the domain was opened"));
    logger.disable();

    const QString contents = contentsOf(file);

    QVERIFY(contents.contains(QStringLiteral("written after the domain was opened")));
    QVERIFY(!contents.contains(QStringLiteral("still nothing to write")));
}

void LoggerTest::nonAsciiMessageReachesTheFile()
{
    const auto file = logFile("nonAscii");

    Logger logger;
    logger.enable(Logger::Notice, file);
    logger.log(QStringLiteral("Überweisung an Müller-Groß über 12,50 Euro"));
    logger.disable();

    QVERIFY(
        contentsOf(file).contains(QStringLiteral("Überweisung an Müller-Groß über 12,50 Euro")));
}

/**
 * The path is handed to the C logger as bytes. toLocal8Bit answers the locale
 * codec and drops what it cannot map, so under a locale that is not UTF-8 the
 * logger opened a different file than the caller named, or none at all.
 * QFile::encodeName answers what open() needs.
 */
void LoggerTest::nonAsciiPathReachesTheFile()
{
    const auto file = logFile(QStringLiteral("Überweisungen-Müller-Groß"));

    Logger logger;
    logger.enable(Logger::Notice, file);
    logger.log(QStringLiteral("written to a path with umlauts"));
    logger.disable();

    QVERIFY(QFile::exists(file));
    QVERIFY(contentsOf(file).contains(QStringLiteral("written to a path with umlauts")));
}

/**
 * The technical cause of an error travels through a logging category, not
 * through log(). A file that only held what log() wrote would carry none of it,
 * and the promise that the cause can be looked up would hold for nobody.
 */
void LoggerTest::aCategoryMessageReachesTheSameFile()
{
    const auto file = logFile("categories");

    Logger logger;
    logger.enable(Logger::Notice, file);

    qCWarning(lcStorage) << "the storage could not be read";

    logger.disable();

    QVERIFY(contentsOf(file).contains(QStringLiteral("the storage could not be read")));
}

/**
 * A directory cannot be opened as a file. The run goes on without a log, and the
 * caller hears about it once so that it can be said once.
 */
void LoggerTest::aFileThatCannotBeOpenedIsReportedAndStopsNothing()
{
    Logger logger;
    QSignalSpy unavailableSpy(&logger, &Logger::logFileUnavailable);

    logger.enable(Logger::Notice, workingDirectory->path());

    QCOMPARE(unavailableSpy.count(), 1);

    // Nothing here throws or blocks, which is the whole of what the caller needs.
    logger.log(QStringLiteral("this one is lost"));
    qCWarning(lcStorage) << "and so is this one";

    logger.disable();

    QCOMPARE(unavailableSpy.count(), 1);
}

void LoggerTest::theDefaultFileSitsInTheDataLocationOfTheApplication()
{
    const QString file = Logger::defaultLogFile();

    QVERIFY(!file.isEmpty());
    QVERIFY(QFileInfo(file).isAbsolute());
    QCOMPARE(QFileInfo(file).fileName(), QStringLiteral("olbaflinx.log"));
    QVERIFY(file.startsWith(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)));
}

/**
 * enable() puts the instance into a static owner, hands a QFile to a static
 * pointer and installs the message handler. A member of this class holds none of
 * it, so leaving the scope used to undo nothing: the owner stayed behind
 * pointing at an object that was gone, and the next failed write emitted a
 * signal on it.
 *
 * The file pointer stayed as well, which is what this measures: enable() answers
 * a file that is already open by keeping the one it has, so a second logger
 * could not open one at all.
 */
void LoggerTest::aLoggerDestroyedWithoutDisableTakesTheLogDownWithIt()
{
    const auto first = logFile("destroyedWithoutDisable");
    const auto second = logFile("afterTheDestructor");

    {
        Logger logger;
        logger.enable(Logger::Notice, first);
        logger.log(QStringLiteral("written by the first logger"));
    }

    QVERIFY(contentsOf(first).contains(QStringLiteral("written by the first logger")));

    // A second logger opens the file it names rather than finding one that is
    // still standing.
    Logger logger;
    logger.enable(Logger::Notice, second);

    qCWarning(lcStorage) << "written by the second logger";

    logger.disable();

    QVERIFY(QFile::exists(second));
    QVERIFY(contentsOf(second).contains(QStringLiteral("written by the second logger")));

    // And nothing of the second run reached the file of the first.
    QVERIFY(!contentsOf(first).contains(QStringLiteral("written by the second logger")));
}

/**
 * A call without a file opens the logging domain of gwenhywfar just the same,
 * and only sets the level while it does. It used to leave no owner behind, so
 * nothing closed the domain: the next logger found it open, skipped the open and
 * the level with it, and wrote to the console at the level of the first call
 * rather than to the file it was handed.
 */
void LoggerTest::aLoggerEnabledWithoutAFileLeavesTheDomainToTheNextOne()
{
    {
        Logger console;
        console.enable(Logger::Notice);
    }

    const auto file = logFile("afterAConsoleLogger");

    Logger logger;
    logger.enable(Logger::Debug, file);
    logger.log(QStringLiteral("this one names a file and has to reach it"));
    logger.disable();

    QVERIFY(QFile::exists(file));
    QVERIFY(contentsOf(file).contains(QStringLiteral("this one names a file and has to reach it")));
}

/**
 * The domain and the file go together and go with one owner. A second logger
 * that came up while the first still holds them used to take the file for
 * itself, and its destructor then closed what the first was still writing to:
 * the owner logged into nothing while it still counted as enabled.
 */
void LoggerTest::aSecondLoggerTakesNeitherTheDomainNorTheFileFromTheFirst()
{
    const auto held = logFile("theOwnerKeepsIt");
    const auto taken = logFile("theSecondOneGetsNothing");

    Logger owner;
    owner.enable(Logger::Notice, held);

    {
        Logger second;
        second.enable(Logger::Notice, taken);
    }

    owner.log(QStringLiteral("the owner is still writing"));

    QVERIFY(!QFile::exists(taken));
    QVERIFY(contentsOf(held).contains(QStringLiteral("the owner is still writing")));

    owner.disable();
}

} // namespace olbaflinx::core::logger::tests

QTEST_APPLESS_MAIN(olbaflinx::core::logger::tests::LoggerTest)

#include "tst_logger.moc"
