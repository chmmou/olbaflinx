/**
 * Copyright (c) 2021, Alexander Saal <alexander.saal@chm-projects.de>
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

#include "Banking/Banking.h"
#include "SingleApplication/SingleApplication.h"

using namespace olbaflinx::core;
using namespace olbaflinx::core::banking;

namespace olbaflinx::core::banking::tests
{

class BankingTest: public QObject
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
    void testReceiveAccount();
    void testReceiveAccounts();
    void testReceiveAccountIds();
    void testCreateAccount();
};

BankingTest::BankingTest()
    : metaTypeIds(QList<int>())
{
    SingleApplication::setApplicationName("OlbaFlinx");
    SingleApplication::setApplicationVersion("1.0.0");
    SingleApplication::setOrganizationName("de.chm-projects.olbaflinx");
    SingleApplication::setOrganizationDomain("https://olbaflinx.chm-projects.de");
}

BankingTest::~BankingTest() = default;

void BankingTest::initTestCase()
{
    metaTypeIds << qRegisterMetaType<AccountList>("AccountList");
    metaTypeIds << qRegisterMetaType<AccountIds>("AccountIds");
    QCOMPARE(metaTypeIds.count(), 2);

    bool initialized = Banking::instance()->initialize(
        SingleApplication::applicationName(),
        QString("%1").arg(QRandomGenerator::system()->generate()),
        SingleApplication::applicationVersion()
    );

    QVERIFY(initialized);
}

void BankingTest::cleanupTestCase()
{
    for (const int id: qAsConst(metaTypeIds)) {
        QMetaType::unregisterType(id);
    }
    metaTypeIds.clear();
    QVERIFY(metaTypeIds.isEmpty());

    bool initialized = Banking::instance()->deInitialize();
    QVERIFY(initialized);
}

void BankingTest::testReceiveAccount()
{
    const auto banking = Banking::instance();
    const quint32 accountId = QRandomGenerator::system()->generate();
    const auto account = banking->account(accountId);

    QVERIFY(account != nullptr);
    QCOMPARE(account->uniqueId(), accountId);
    QCOMPARE(account->isValid(), false);
}

void BankingTest::testReceiveAccounts()
{
    const auto banking = Banking::instance();
    QSignalSpy spy(banking, &Banking::accountsReceived);

    banking->receiveAccounts();

    QCOMPARE(spy.count(), 1);
}

void BankingTest::testReceiveAccountIds()
{
    const auto banking = Banking::instance();
    QSignalSpy spy(banking, &Banking::accountIdsReceived);

    banking->receiveAccountIds();

    QCOMPARE(spy.count(), 1);
}

void BankingTest::testCreateAccount()
{
    const auto banking = Banking::instance();
    bool ok = banking->createAccount();
    QVERIFY(ok);
}

}

QTEST_MAIN(tests::BankingTest)

#include "BankingTest.moc"
