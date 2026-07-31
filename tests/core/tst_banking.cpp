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
#include "core/Banking/Banking.h"
#include "core/Error.h"

#include <QtTest/QtTest>

#include <memory>

using namespace olbaflinx::core;
using namespace olbaflinx::core::banking;

namespace olbaflinx::core::banking::tests {

class BankingTest final : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> bankingHome;

    static ApplicationInfo applicationInfo()
    {
        return {QStringLiteral("de.chm-projects.olbaflinx.test"),
                QStringLiteral("OlbaFlinxBankingTest"),
                QStringLiteral("1.0.0")};
    }

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();

    void bankingIsConstructibleWithoutAnyApplicationInstance();
    void accountsWithoutInitializationReportsAnError();
    void initializeOpensTheBackendUnderTheTemporaryHome();
    void accountsWithoutAnyConfiguredAccountReportsAFailure();
};

/**
 * AqBanking keeps its configuration below AQBANKING_HOME. Without pointing that
 * at a directory of our own, every run would write into the configuration of
 * whoever started it.
 */
void BankingTest::initTestCase()
{
    bankingHome = std::make_unique<QTemporaryDir>();
    QVERIFY(bankingHome->isValid());

    QVERIFY(qputenv("AQBANKING_HOME", bankingHome->path().toUtf8()));
}

void BankingTest::cleanupTestCase()
{
    qunsetenv("AQBANKING_HOME");
    bankingHome.reset();
}

/**
 * Shows that Banking takes name and version from ApplicationInfo. This target
 * uses QTEST_APPLESS_MAIN, so there is no application instance the values could
 * otherwise come from.
 */
void BankingTest::bankingIsConstructibleWithoutAnyApplicationInstance()
{
    QVERIFY(QCoreApplication::instance() == nullptr);

    Banking banking(applicationInfo());

    QVERIFY(banking.parent() == nullptr);
}

/**
 * The failure case: without the backend being set up, accounts() must report no
 * accounts and pass the error on instead.
 */
void BankingTest::accountsWithoutInitializationReportsAnError()
{
    Banking banking(applicationInfo());

    QSignalSpy errorSpy(&banking, &Banking::errorOccurred);
    QSignalSpy itemsSpy(&banking, &Banking::itemsReceived);
    QSignalSpy finishedSpy(&banking, &Banking::finished);

    banking.accounts();

    QCOMPARE(errorSpy.count(), 1);
    QCOMPARE(itemsSpy.count(), 0);
    QCOMPARE(finishedSpy.count(), 1);

    QCOMPARE(errorSpy.takeFirst().at(0).value<ErrorCode>(), ErrorCode::BankingFailure);
}

void BankingTest::initializeOpensTheBackendUnderTheTemporaryHome()
{
    Banking banking(applicationInfo());

    const auto error = banking.initialize(QStringLiteral("OlbaFlinxBankingTest"),
                                          QStringLiteral("1.0.0"),
                                          QStringLiteral("0123456789ABCDEF"));

    QVERIFY2(!error.isError(), qPrintable(error.message()));

    banking.finalize();

    // The backend writes its configuration where AQBANKING_HOME points, and
    // nowhere else.
    QVERIFY(!QDir(bankingHome->path()).isEmpty());
}

/**
 * A fresh home carries no account. AqBanking answers that with GWEN_ERROR_NOT_FOUND,
 * which is -51 and therefore not AB_SUCCESS. accounts() turns every value other
 * than AB_SUCCESS into BankingFailure, so the empty case cannot be told apart
 * from a backend that broke. The NotFound branch further down, which was written
 * for exactly this case, is unreachable.
 *
 * This test states what the class does today. It fails as soon as the empty case
 * gets its own code, which is the point at which it has to be rewritten.
 */
void BankingTest::accountsWithoutAnyConfiguredAccountReportsAFailure()
{
    Banking banking(applicationInfo());

    QVERIFY(!banking
                 .initialize(QStringLiteral("OlbaFlinxBankingTest"),
                             QStringLiteral("1.0.0"),
                             QStringLiteral("0123456789ABCDEF"))
                 .isError());

    QSignalSpy errorSpy(&banking, &Banking::errorOccurred);
    QSignalSpy itemsSpy(&banking, &Banking::itemsReceived);
    QSignalSpy finishedSpy(&banking, &Banking::finished);

    banking.accounts();

    QCOMPARE(itemsSpy.count(), 0);
    QCOMPARE(errorSpy.count(), 1);
    QCOMPARE(finishedSpy.count(), 1);

    const auto arguments = errorSpy.takeFirst();
    QCOMPARE(arguments.at(0).value<ErrorCode>(), ErrorCode::BankingFailure);
    QVERIFY(arguments.at(1).toString().contains(QStringLiteral("-51")));

    banking.finalize();
}

} // namespace olbaflinx::core::banking::tests

QTEST_APPLESS_MAIN(olbaflinx::core::banking::tests::BankingTest)

#include "tst_banking.moc"
