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

#include <QtTest/QtTest>

using namespace olbaflinx::core;
using namespace olbaflinx::core::banking;

namespace olbaflinx::core::banking::tests {

class BankingTest final : public QObject
{
    Q_OBJECT

private:
    static ApplicationInfo applicationInfo()
    {
        return {QStringLiteral("de.chm-projects.olbaflinx.test"),
                QStringLiteral("OlbaFlinxBankingTest"),
                QStringLiteral("1.0.0")};
    }

private Q_SLOTS:
    void bankingIsConstructibleWithoutAnyApplicationInstance();
    void accountsWithoutInitializationReportsAnError();
};

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
}

} // namespace olbaflinx::core::banking::tests

QTEST_APPLESS_MAIN(olbaflinx::core::banking::tests::BankingTest)

#include "tst_banking.moc"
