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

#include "core/ApplicationInfo.h"
#include "core/Error.h"
#include "core/Logger/Logger.h"
#include "core/Storage/Storage.h"
#include "ui/App.h"
#include "ui/ErrorMessage.h"

#include <QtTest/QtTest>

#include <QtWidgets/QStatusBar>

using namespace olbaflinx::core;
using namespace olbaflinx::core::logger;
using namespace olbaflinx::core::storage;
using namespace olbaflinx::ui;

namespace olbaflinx::ui::tests {

/**
 * The errors of core used to be sent through a signal that had no receiver in
 * the whole project. These tests hold the connection in place.
 */
class AppErrorHandlingTest final : public QObject
{
    Q_OBJECT

private:
    static ApplicationInfo applicationInfo()
    {
        return {QStringLiteral("de.chm-projects.olbaflinx.test"),
                QStringLiteral("OlbaFlinxAppErrorHandlingTest"),
                QStringLiteral("1.0.0")};
    }

private Q_SLOTS:
    void initTestCase();

    void everyErrorCodeHasAUserMessage();
    void userMessageCarriesNoTechnicalDetail();
    void storageErrorReachesTheWindow();
};

void AppErrorHandlingTest::initTestCase()
{
    // Keeps QSettings out of the real user configuration, see QStandardPaths docs.
    QStandardPaths::setTestModeEnabled(true);
}

void AppErrorHandlingTest::everyErrorCodeHasAUserMessage()
{
    const auto codes = QList<ErrorCode>{ErrorCode::NotFound,
                                        ErrorCode::PermissionDenied,
                                        ErrorCode::InvalidInput,
                                        ErrorCode::IoFailure,
                                        ErrorCode::DatabaseFailure,
                                        ErrorCode::BankingFailure,
                                        ErrorCode::NotImplemented};

    for (const auto code : codes) {
        const QString message = userMessage(code);

        QVERIFY2(!message.isEmpty(),
                 qPrintable(QStringLiteral("no message for code %1").arg(int(code))));

        // A bare code is not enough, the message has to say something.
        QVERIFY(message.length() > 20);
    }

    // The absence of an error is not something to tell the user about.
    QVERIFY(userMessage(ErrorCode::None).isEmpty());
}

void AppErrorHandlingTest::userMessageCarriesNoTechnicalDetail()
{
    const auto codes = QList<ErrorCode>{ErrorCode::NotFound,
                                        ErrorCode::PermissionDenied,
                                        ErrorCode::InvalidInput,
                                        ErrorCode::IoFailure,
                                        ErrorCode::DatabaseFailure,
                                        ErrorCode::BankingFailure,
                                        ErrorCode::NotImplemented};

    for (const auto code : codes) {
        const QString message = userMessage(code);

        // No SQL, no path of the development machine.
        QVERIFY(!message.contains(QStringLiteral("SELECT"), Qt::CaseInsensitive));
        QVERIFY(!message.contains(QStringLiteral("INSERT"), Qt::CaseInsensitive));
        QVERIFY(!message.contains(QStringLiteral("PRAGMA"), Qt::CaseInsensitive));
        QVERIFY(!message.contains(QLatin1Char('/')));
    }
}

/**
 * The whole path: Storage reports, the window picks it up and puts the short
 * form into the status bar. Nothing modal, the window stays usable.
 */
void AppErrorHandlingTest::storageErrorReachesTheWindow()
{
    Logger logger;
    Storage storage(applicationInfo());

    App app(&logger, &storage);

    QCOMPARE(app.statusBar()->currentMessage(), QString());

    Q_EMIT storage.errorOccurred(ErrorCode::PermissionDenied,
                                 QStringLiteral("PRAGMA key failed on /home/somebody/vault.obfx"));

    const QString shown = app.statusBar()->currentMessage();

    QCOMPARE(shown, userMessage(ErrorCode::PermissionDenied));
    QVERIFY(!shown.contains(QStringLiteral("PRAGMA")));
    QVERIFY(!shown.contains(QStringLiteral("/home/")));
}

} // namespace olbaflinx::ui::tests

QTEST_MAIN(olbaflinx::ui::tests::AppErrorHandlingTest)

#include "tst_apperrorhandling.moc"
