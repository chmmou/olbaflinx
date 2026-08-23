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

#include "core/Banking/StandingOrder/StandingOrder.h"

#include "StandingOrderHelpers.h"

#include <QtTest/QtTest>

using namespace olbaflinx::core::banking;
using namespace olbaflinx::core::banking::standingorder;

namespace olbaflinx::core::banking::standingorder::tests {

using namespace olbaflinx::core::tests;

/**
 * The value object of a standing order: what it reads out of the backend, what
 * it writes into a property map, and the fingerprint it is recognised by.
 */
class StandingOrderTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void anOrderFromTheBackendCarriesItsFields();
    void anOrderFromAMapCarriesItsFields();
    void theFingerprintCoversTheTenFieldsThatIdentifyAnOrder();
    void theFingerprintStaysTheSameOnASecondCall();
    void anOrderThatCarriesAFingerprintKeepsIt();
    void aDateThatCannotBeReadIsInvalidAndNotToday();
    void anOrderWithoutAFirstDateStillCarriesAFingerprint();
    void theMapNamesTheWayTheOrderIsIdentifiedBy();
    void anOrderOfAnotherTypeIsNotValid();
    void theReportedExecutionStandsWhereItIsStillToCome();
    void theExecutionIsCountedWhereNoneIsReported_data();
    void theExecutionIsCountedWhereNoneIsReported();
    void anOrderWithoutACycleFallsBackOnItsFirstExecution();
    void anOrderPastItsLastExecutionRunsNoMore();
    void anOrderWithoutAnyDateHasNoNextExecution();
};

void StandingOrderTest::anOrderFromTheBackendCarriesItsFields()
{
    const auto order = StandingOrderHelpers::fromBackend();

    QVERIFY(order->isValid());
    QCOMPARE(order->itemType(), QStringLiteral("StandingOrder"));
    QCOMPARE(order->uniqueAccountId(), 815u);
    QCOMPARE(order->uniqueId(), 4711u);
    QCOMPARE(order->remoteName(), QStringLiteral("Erika Musterfrau"));
    QCOMPARE(order->remoteIban(), QStringLiteral("DE02120300000000202051"));
    QCOMPARE(order->value(), 42.5);
    QCOMPARE(order->currency(), QStringLiteral("EUR"));
    QCOMPARE(order->purpose(), QStringLiteral("Miete"));
    QCOMPARE(order->period(), AB_Transaction_PeriodMonthly);
    QCOMPARE(order->cycle(), 1u);
    QCOMPARE(order->executionDay(), 1u);
    QCOMPARE(order->firstDate(), QDate(2026, 1, 1));
    QCOMPARE(order->nextDate(), QDate(2026, 3, 1));
    QCOMPARE(order->status(), AB_Transaction_StatusAccepted);
}

void StandingOrderTest::anOrderFromAMapCarriesItsFields()
{
    auto spec = StandingOrderSpec{};
    spec.fiId = QStringLiteral("DA-0815");
    spec.lastDate = QDate(2027, 12, 1);

    const auto order = StandingOrder::fromMap(StandingOrderHelpers::standingOrderMap(spec));

    QVERIFY(order->isValid());
    QCOMPARE(order->fiId(), QStringLiteral("DA-0815"));
    QCOMPARE(order->uniqueAccountId(), 815u);
    QCOMPARE(order->localIban(), QStringLiteral("DE02500105170137075030"));
    QCOMPARE(order->remoteBic(), QStringLiteral("BYLADEM1001"));
    QCOMPARE(order->value(), 42.5);
    QCOMPARE(order->firstDate(), QDate(2026, 1, 1));
    QCOMPARE(order->lastDate(), QDate(2027, 12, 1));
    QCOMPARE(order->nextDate(), QDate(2026, 3, 1));
}

/**
 * Ten fields carry the identity of an order, and no eleventh does. Each of the
 * ten is changed on its own and has to move the fingerprint; the three that
 * change with every execution or after the fact must leave it where it is.
 */
void StandingOrderTest::theFingerprintCoversTheTenFieldsThatIdentifyAnOrder()
{
    const auto fingerprintOf = [](const StandingOrderSpec &spec) {
        return StandingOrderHelpers::fromBackend(spec)->calculateFingerprint();
    };

    const auto reference = fingerprintOf({});
    QVERIFY(!reference.isEmpty());

    auto changed = StandingOrderSpec{};
    changed.uniqueAccountId = 816;
    QVERIFY(fingerprintOf(changed) != reference);

    changed = {};
    changed.remoteIban = QStringLiteral("DE02100500000054540402");
    QVERIFY(fingerprintOf(changed) != reference);

    changed = {};
    changed.remoteName = QStringLiteral("Max Mustermann");
    QVERIFY(fingerprintOf(changed) != reference);

    changed = {};
    changed.value = 42.6;
    QVERIFY(fingerprintOf(changed) != reference);

    changed = {};
    changed.currency = QStringLiteral("CHF");
    QVERIFY(fingerprintOf(changed) != reference);

    changed = {};
    changed.purpose = QStringLiteral("Nebenkosten");
    QVERIFY(fingerprintOf(changed) != reference);

    changed = {};
    changed.period = AB_Transaction_PeriodWeekly;
    QVERIFY(fingerprintOf(changed) != reference);

    changed = {};
    changed.cycle = 3;
    QVERIFY(fingerprintOf(changed) != reference);

    changed = {};
    changed.executionDay = 15;
    QVERIFY(fingerprintOf(changed) != reference);

    changed = {};
    changed.firstDate = QDate(2026, 2, 1);
    QVERIFY(fingerprintOf(changed) != reference);

    // The next execution moves with every run of the order, the last one can be
    // agreed after the fact, and the state is no property of the order itself.
    // An order that carried any of them in its fingerprint would be a new one on
    // every fetch.
    changed = {};
    changed.nextDate = QDate(2026, 4, 1);
    QCOMPARE(fingerprintOf(changed), reference);

    changed = {};
    changed.lastDate = QDate(2030, 1, 1);
    QCOMPARE(fingerprintOf(changed), reference);

    changed = {};
    changed.status = AB_Transaction_StatusPending;
    QCOMPARE(fingerprintOf(changed), reference);

    // The identifier of the institution is the other way of recognising an
    // order, not a part of this one.
    changed = {};
    changed.fiId = QStringLiteral("DA-0815");
    QCOMPARE(fingerprintOf(changed), reference);
}

void StandingOrderTest::theFingerprintStaysTheSameOnASecondCall()
{
    const auto order = StandingOrderHelpers::fromBackend();

    const auto first = order->calculateFingerprint();
    QVERIFY(!first.isEmpty());
    QCOMPARE(order->calculateFingerprint(), first);
}

void StandingOrderTest::anOrderThatCarriesAFingerprintKeepsIt()
{
    auto spec = StandingOrderSpec{};
    spec.fingerprint = QStringLiteral("a fingerprint of its own");

    const auto order = StandingOrderHelpers::fromBackend(spec);

    QCOMPARE(order->calculateFingerprint(), QStringLiteral("a fingerprint of its own"));
}

void StandingOrderTest::aDateThatCannotBeReadIsInvalidAndNotToday()
{
    auto spec = StandingOrderSpec{};
    spec.firstDate = {};
    spec.lastDate = {};
    spec.nextDate = {};

    const auto order = StandingOrderHelpers::fromBackend(spec);

    QVERIFY(!order->firstDate().isValid());
    QVERIFY(!order->lastDate().isValid());
    QVERIFY(!order->nextDate().isValid());
    QVERIFY(order->firstDate() != QDate::currentDate());
}

/**
 * A missing first execution does not take the field out of the fingerprint. It
 * goes in as the invalid date it is, so two orders that differ in nothing else
 * still share the value, and one that differs elsewhere does not.
 */
void StandingOrderTest::anOrderWithoutAFirstDateStillCarriesAFingerprint()
{
    auto spec = StandingOrderSpec{};
    spec.firstDate = {};

    const auto withoutDate = StandingOrderHelpers::fromBackend(spec)->calculateFingerprint();
    QVERIFY(!withoutDate.isEmpty());
    QVERIFY(withoutDate != StandingOrderHelpers::fromBackend({})->calculateFingerprint());

    auto second = spec;
    second.purpose = QStringLiteral("Nebenkosten");
    QVERIFY(StandingOrderHelpers::fromBackend(second)->calculateFingerprint() != withoutDate);

    QCOMPARE(StandingOrderHelpers::fromBackend(spec)->calculateFingerprint(), withoutDate);
}

/**
 * The column says which of the two ways carries the record, and the value
 * follows from the identifier alone: an order that has one is found by it, an
 * order without one by its fingerprint.
 */
void StandingOrderTest::theMapNamesTheWayTheOrderIsIdentifiedBy()
{
    const auto withoutId = StandingOrderHelpers::fromBackend()->toMap();
    QCOMPARE(withoutId.value(QStringLiteral("identified_by")).toInt(), 2);
    QVERIFY(!withoutId.value(QStringLiteral("fingerprint")).toString().isEmpty());

    auto spec = StandingOrderSpec{};
    spec.fiId = QStringLiteral("DA-0815");

    const auto withId = StandingOrderHelpers::fromBackend(spec)->toMap();
    QCOMPARE(withId.value(QStringLiteral("identified_by")).toInt(), 1);

    // Formed for both, so that an order which gains an identifier later is found
    // again rather than written a second time.
    QVERIFY(!withId.value(QStringLiteral("fingerprint")).toString().isEmpty());
}

void StandingOrderTest::anOrderOfAnotherTypeIsNotValid()
{
    AB_TRANSACTION *abTransaction = AB_Transaction_new();
    AB_Transaction_SetType(abTransaction, AB_Transaction_TypeStatement);

    const StandingOrder order(abTransaction);
    AB_Transaction_free(abTransaction);

    QVERIFY(!order.isValid());
}

/**
 * A date the institution itself reports is the answer, and nothing is counted
 * beside it. Only a date that has passed gives way, because an execution behind
 * the day asked about is not the next one.
 */
void StandingOrderTest::theReportedExecutionStandsWhereItIsStillToCome()
{
    auto spec = StandingOrderSpec{};
    spec.firstDate = QDate(2026, 1, 1);
    spec.nextDate = QDate(2026, 3, 1);

    const auto order = StandingOrderHelpers::fromBackend(spec);

    QCOMPARE(order->nextExecution(QDate(2026, 2, 15)), QDate(2026, 3, 1));
    QCOMPARE(order->nextExecution(QDate(2026, 3, 1)), QDate(2026, 3, 1));

    // Past the reported one, so it is counted from the first execution instead.
    QCOMPARE(order->nextExecution(QDate(2026, 3, 2)), QDate(2026, 4, 1));
}

void StandingOrderTest::theExecutionIsCountedWhereNoneIsReported_data()
{
    QTest::addColumn<int>("period");
    QTest::addColumn<quint32>("cycle");
    QTest::addColumn<QDate>("firstDate");
    QTest::addColumn<QDate>("from");
    QTest::addColumn<QDate>("expected");

    const auto monthly = static_cast<int>(AB_Transaction_PeriodMonthly);
    const auto weekly = static_cast<int>(AB_Transaction_PeriodWeekly);

    QTest::newRow("monthly, still ahead")
        << monthly << 1u << QDate(2027, 1, 15) << QDate(2026, 8, 23) << QDate(2027, 1, 15);
    QTest::newRow("monthly, on an execution")
        << monthly << 1u << QDate(2026, 1, 15) << QDate(2026, 8, 15) << QDate(2026, 8, 15);
    QTest::newRow("monthly, between two")
        << monthly << 1u << QDate(2026, 1, 15) << QDate(2026, 8, 16) << QDate(2026, 9, 15);
    QTest::newRow("quarterly") << monthly << 3u << QDate(2026, 1, 15) << QDate(2026, 8, 23)
                               << QDate(2026, 10, 15);
    QTest::newRow("annually") << monthly << 12u << QDate(2020, 3, 1) << QDate(2026, 8, 23)
                              << QDate(2027, 3, 1);

    // A short month shortens its own execution and none of the ones after it.
    // Counting from the execution before would keep the 28th for good.
    QTest::newRow("month end, shortened")
        << monthly << 1u << QDate(2026, 1, 31) << QDate(2026, 2, 1) << QDate(2026, 2, 28);
    QTest::newRow("month end, back to full length")
        << monthly << 1u << QDate(2026, 1, 31) << QDate(2026, 3, 1) << QDate(2026, 3, 31);

    QTest::newRow("weekly") << weekly << 1u << QDate(2026, 1, 1) << QDate(2026, 1, 20)
                            << QDate(2026, 1, 22);
    QTest::newRow("fortnightly") << weekly << 2u << QDate(2026, 1, 1) << QDate(2026, 2, 1)
                                 << QDate(2026, 2, 12);
}

void StandingOrderTest::theExecutionIsCountedWhereNoneIsReported()
{
    QFETCH(int, period);
    QFETCH(quint32, cycle);
    QFETCH(QDate, firstDate);
    QFETCH(QDate, from);
    QFETCH(QDate, expected);

    auto spec = StandingOrderSpec{};
    spec.period = static_cast<AB_TRANSACTION_PERIOD>(period);
    spec.cycle = cycle;
    spec.firstDate = firstDate;
    spec.nextDate = QDate();

    QCOMPARE(StandingOrderHelpers::fromBackend(spec)->nextExecution(from), expected);
}

/**
 * Without a period or with a cycle of nought there is no run to count on, and
 * the first execution is what the order carries instead.
 */
void StandingOrderTest::anOrderWithoutACycleFallsBackOnItsFirstExecution()
{
    auto spec = StandingOrderSpec{};
    spec.firstDate = QDate(2026, 1, 1);
    spec.nextDate = QDate();
    spec.cycle = 0;

    QCOMPARE(StandingOrderHelpers::fromBackend(spec)->nextExecution(QDate(2026, 8, 23)),
             QDate(2026, 1, 1));

    spec.cycle = 1;
    spec.period = AB_Transaction_PeriodUnknown;

    QCOMPARE(StandingOrderHelpers::fromBackend(spec)->nextExecution(QDate(2026, 8, 23)),
             QDate(2026, 1, 1));
}

void StandingOrderTest::anOrderPastItsLastExecutionRunsNoMore()
{
    auto spec = StandingOrderSpec{};
    spec.firstDate = QDate(2026, 1, 1);
    spec.lastDate = QDate(2026, 6, 1);
    spec.nextDate = QDate();

    const auto order = StandingOrderHelpers::fromBackend(spec);

    QCOMPARE(order->nextExecution(QDate(2026, 5, 2)), QDate(2026, 6, 1));
    QVERIFY(!order->nextExecution(QDate(2026, 6, 2)).isValid());
}

void StandingOrderTest::anOrderWithoutAnyDateHasNoNextExecution()
{
    auto spec = StandingOrderSpec{};
    spec.firstDate = QDate();
    spec.nextDate = QDate();

    QVERIFY(!StandingOrderHelpers::fromBackend(spec)->nextExecution(QDate(2026, 8, 23)).isValid());
}

} // namespace olbaflinx::core::banking::standingorder::tests

QTEST_APPLESS_MAIN(olbaflinx::core::banking::standingorder::tests::StandingOrderTest)

#include "tst_standingorder.moc"
