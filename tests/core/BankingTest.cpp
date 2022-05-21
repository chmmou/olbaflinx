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

#include <QtCore/QObject>
#include <QtCore/QRandomGenerator>
#include <QtTest/QtTest>

#include "core/Banking/OnlineBanking.h"
#include "core/SingleApplication/SingleApplication.h"

#include "BaseTest.h"

using namespace olbaflinx::core;
using namespace olbaflinx::core::banking;

namespace olbaflinx::core::banking::tests {

using namespace olbaflinx::core::tests;

class BankingTest : public QObject
{
    Q_OBJECT

public:
    BankingTest();
    ~BankingTest() override;

private:
    QList<int> metaTypeIds;

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void testAccountInvalid();
};

BankingTest::BankingTest()
    : metaTypeIds(QList<int>())
{
    SingleApplication::setApplicationName("OlbaFlinx");
    SingleApplication::setApplicationVersion("1.0.0");
    SingleApplication::setOrganizationName("de.chm-projects.olbaflinx.test");
    SingleApplication::setOrganizationDomain("https://olbaflinx.chm-projects.de");
}

BankingTest::~BankingTest() = default;

void BankingTest::initTestCase()
{
    metaTypeIds << qRegisterMetaType<AccountBalanceList>("AccountBalanceList");
    metaTypeIds << qRegisterMetaType<AccountList>("AccountList");
    metaTypeIds << qRegisterMetaType<AccountIds>("AccountIds");

    QCOMPARE(metaTypeIds.count(), 3);

    bool initialized = OnlineBanking::instance()
                           ->initialize(SingleApplication::applicationName(),
                                        QString("%1").arg(QRandomGenerator::system()->generate()),
                                        SingleApplication::applicationVersion());

    QVERIFY(initialized);
}

void BankingTest::cleanupTestCase()
{
    for (const int id : qAsConst(metaTypeIds)) {
        QMetaType::unregisterType(id);
    }
    metaTypeIds.clear();
    QVERIFY(metaTypeIds.isEmpty());

    OnlineBanking::instance()->finalize();
}

void BankingTest::testAccountInvalid()
{
    const auto banking = OnlineBanking::instance();
    const quint32 accountId = QRandomGenerator::system()->generate();
    const auto account = banking->account(accountId);

    QVERIFY(account != nullptr);
    QCOMPARE(account->uniqueId(), accountId);
    QCOMPARE(account->isValid(), false);
}

} // namespace olbaflinx::core::banking::tests

QTEST_MAIN(banking::tests::BankingTest)

#include "BankingTest.moc"
