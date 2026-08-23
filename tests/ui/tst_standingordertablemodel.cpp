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

#include "ui/Models/StandingOrderTableModel.h"

#include "core/ApplicationInfo.h"
#include "core/Banking/Account/Account.h"
#include "core/Storage/Storage.h"

#include "StandingOrderHelpers.h"
#include "TestHelpers.h"

#include <QtTest/QtTest>

#include <QtCore/QLocale>
#include <QtCore/QStandardPaths>
#include <QtCore/QTemporaryDir>

#include <chrono>
#include <memory>

using namespace olbaflinx::core;
using namespace olbaflinx::core::banking;
using namespace olbaflinx::core::banking::account;
using namespace olbaflinx::core::banking::standingorder;
using namespace olbaflinx::core::storage;
using namespace olbaflinx::core::tests;
using namespace olbaflinx::ui::models;

namespace olbaflinx::ui::models::tests {

class StandingOrderTableModelTest final : public QObject
{
    Q_OBJECT

private:
    static constexpr quint32 firstAccount = 815;
    static constexpr quint32 secondAccount = 4711;

    /**
     * The upper bound a wait may take before the test counts as failed. It is
     * not a wait: every use returns the moment the condition holds.
     */
    static constexpr int workerTimeoutMs = 30000;
    static constexpr auto workerTimeout = std::chrono::seconds{30};

    std::unique_ptr<QTemporaryDir> workingDirectory;
    QLocale previousLocale;

    static QString password() { return TestHelpers::password(); }

    static ApplicationInfo applicationInfo()
    {
        return TestHelpers::applicationInfo(QStringLiteral("OlbaFlinxStandingOrderModelTest"));
    }

    [[nodiscard]] QString storageFile() const
    {
        return workingDirectory->filePath(QStringLiteral("storage.obfx"));
    }

    [[nodiscard]] bool openStorage(Storage &storage) const
    {
        if (storage.setKey(password()).isError()) {
            return false;
        }

        storage.setStorageFile(storageFile());

        return !storage.initialize(true).isError();
    }

    /** Runs one write and says whether it went through. */
    static bool storeAndWait(Storage &storage,
                             const BankingItems &items,
                             const StandingOrderRun &run = {})
    {
        QSignalSpy finishedSpy(&storage, &Storage::writeFinished);

        if (storage.storeItems(items, run).isError()) {
            return false;
        }

        return !finishedSpy.isEmpty() || finishedSpy.wait(workerTimeout);
    }

    /** The account every order of this binary hangs on. */
    static bool storeAccount(Storage &storage, quint32 uniqueAccountId)
    {
        const auto account = Account::fromMap(TestHelpers::accountMapWith(uniqueAccountId, 100.0));

        return storeAndWait(storage, BankingItems{account});
    }

    /**
     * An order with the fields a test names and the defaults of the helper for
     * the rest.
     */
    static std::shared_ptr<StandingOrder> orderWith(const StandingOrderSpec &spec)
    {
        return StandingOrderHelpers::fromBackend(spec);
    }

    /**
     * A model holding the given orders, without a storage behind it. The mapping
     * of a row onto a column needs no file.
     */
    static void fill(StandingOrderTableModel &model, const BankingItems &items)
    {
        model.setItems(items);
    }

    static QString shownAt(const StandingOrderTableModel &model, int row, int column)
    {
        return model.data(model.index(row, column), Qt::DisplayRole).toString();
    }

    static QString headerOf(const StandingOrderTableModel &model, int column)
    {
        return model.headerData(column, Qt::Horizontal, Qt::DisplayRole).toString();
    }

    static QStringList columnOf(const StandingOrderTableModel &model, int column)
    {
        auto values = QStringList();

        for (int row = 0; row < model.rowCount(); ++row) {
            values << shownAt(model, row, column);
        }

        return values;
    }

    static QVariantList valuesOf(const StandingOrderTableModel &model, int role)
    {
        auto values = QVariantList();

        for (int row = 0; row < model.rowCount(); ++row) {
            values << model.data(model.index(row, 0), role);
        }

        return values;
    }

    /**
     * Whether the left value may stand above the right one. Compared in the type
     * the role carries and not in the text the column shows: an amount orders
     * numerically, a date chronologically, a name by its characters.
     */
    static bool notAfter(const QVariant &left, const QVariant &right)
    {
        if (left.typeId() == QMetaType::QDate) {
            return left.toDate() <= right.toDate();
        }

        if (left.typeId() == QMetaType::Double) {
            return left.toDouble() <= right.toDouble();
        }

        if (left.typeId() == QMetaType::UInt || left.typeId() == QMetaType::Int) {
            return left.toInt() <= right.toInt();
        }

        return QString::localeAwareCompare(left.toString(), right.toString()) <= 0;
    }

    static bool isOrdered(const QVariantList &values, Qt::SortOrder order)
    {
        for (int index = 1; index < values.size(); ++index) {
            const auto &previous = values.at(index - 1);
            const auto &current = values.at(index);

            const bool inOrder = order == Qt::AscendingOrder ? notAfter(previous, current)
                                                             : notAfter(current, previous);
            if (!inOrder) {
                return false;
            }
        }

        return true;
    }

    /**
     * Three orders of one account, told apart in every field a column shows.
     *
     * Their executions lie far ahead so that the reported date is what the cell
     * shows. One that had passed would be counted forward instead, and the rows
     * would depend on the day the test runs.
     */
    static BankingItems threeOrders(quint32 uniqueAccountId)
    {
        auto items = BankingItems();

        auto first = StandingOrderSpec{};
        first.uniqueAccountId = uniqueAccountId;
        first.uniqueId = 1;
        first.fiId = QStringLiteral("SO-1");
        first.remoteName = QStringLiteral("Charlie");
        first.purpose = QStringLiteral("Rent");
        first.value = 750.0;
        first.period = AB_Transaction_PeriodMonthly;
        first.cycle = 1;
        first.nextDate = QDate(2099, 9, 1);

        auto second = StandingOrderSpec{};
        second.uniqueAccountId = uniqueAccountId;
        second.uniqueId = 2;
        second.fiId = QStringLiteral("SO-2");
        second.remoteName = QStringLiteral("Alice");
        second.purpose = QStringLiteral("Insurance");
        second.value = 42.5;
        second.period = AB_Transaction_PeriodMonthly;
        second.cycle = 3;
        second.nextDate = QDate(2099, 9, 15);

        auto third = StandingOrderSpec{};
        third.uniqueAccountId = uniqueAccountId;
        third.uniqueId = 3;
        third.fiId = QStringLiteral("SO-3");
        third.remoteName = QStringLiteral("Bob");
        third.purpose = QStringLiteral("Savings");
        third.value = 100.0;
        third.period = AB_Transaction_PeriodWeekly;
        third.cycle = 1;
        third.nextDate = QDate(2099, 8, 25);

        items << orderWith(first) << orderWith(second) << orderWith(third);

        return items;
    }

private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void anEmptyModelHasNoRows();
    void onlyStandingOrdersAreTakenOver();
    void theModelCarriesFiveColumnsEachWithAHeader();
    void everyColumnShowsWhatItsHeaderPromises();
    void theIntervalNamesThePairOfPeriodAndCycle();
    void theIntervalNamesThePairOfPeriodAndCycle_data();
    void theCellCountsTheExecutionWhereTheInstitutionNamesNone();
    void anOrderWithoutAnyDateLeavesTheCellEmpty();
    void anAmountCarriesTwoDecimalsAndStandsRight();
    void aDateCarriesTheDayTheMonthAndFourDigitsOfYear();
    void aDateCarriesTheDayTheMonthAndFourDigitsOfYear_data();
    void theCurrencyStandsInTheHeaderOfTheAmount();
    void theHeaderNamesNoCurrencyWhereTheRowsCarryTwo();

    void theDefaultOrderIsThePayeeAscending();
    void everyColumnOrdersUpAndDown();
    void everyColumnOrdersUpAndDown_data();
    void theIntervalOrdersByItsLengthAndNotByItsWord();

    void anAccountWithOrdersShowsThem();
    void anAccountWithoutAnOrderShowsNoRowAndNoFailure();
    void anEndedOrderDoesNotAppear();
    void anAccountWhoseOrdersAllEndedShowsNoRow();
    void aChangeOfAccountSwitchesTheView();
    void aFailedReadLeavesTheRowsStanding();
    void aRequestTheStorageTurnedDownIsReportedRatherThanSwallowed();
};

void StandingOrderTableModelTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);

    QVERIFY(TestHelpers::useTemporaryHome());
}

void StandingOrderTableModelTest::init()
{
    workingDirectory = std::make_unique<QTemporaryDir>();
    QVERIFY(workingDirectory->isValid());

    // The formatting tests set one of their own. Held here so that every case
    // starts from the same one, whatever the machine is set to.
    previousLocale = QLocale();
}

void StandingOrderTableModelTest::cleanup()
{
    QLocale::setDefault(previousLocale);

    workingDirectory.reset();
}

void StandingOrderTableModelTest::anEmptyModelHasNoRows()
{
    const StandingOrderTableModel model;

    QCOMPARE(model.rowCount(), 0);
    QCOMPARE(model.columnCount(), StandingOrderTableModel::ColumnCount);
}

void StandingOrderTableModelTest::onlyStandingOrdersAreTakenOver()
{
    StandingOrderTableModel model;

    BankingItems items;
    items << orderWith({});
    items << Account::fromMap({});

    fill(model, items);

    QCOMPARE(model.rowCount(), 1);
}

void StandingOrderTableModelTest::theModelCarriesFiveColumnsEachWithAHeader()
{
    const StandingOrderTableModel model;

    QCOMPARE(model.columnCount(), 5);

    for (int column = 0; column < model.columnCount(); ++column) {
        QVERIFY2(!headerOf(model, column).isEmpty(), qPrintable(QString::number(column)));
    }
}

void StandingOrderTableModelTest::everyColumnShowsWhatItsHeaderPromises()
{
    QLocale::setDefault(QLocale(QLocale::German, QLocale::Germany));

    StandingOrderTableModel model;

    auto spec = StandingOrderSpec{};
    spec.remoteName = QStringLiteral("Erika Musterfrau");
    spec.purpose = QStringLiteral("Miete");
    spec.value = 750.0;
    spec.period = AB_Transaction_PeriodMonthly;
    spec.cycle = 1;
    spec.nextDate = QDate(2099, 9, 1);

    fill(model, BankingItems{orderWith(spec)});

    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(shownAt(model, 0, StandingOrderTableModel::RemoteNameColumn),
             QStringLiteral("Erika Musterfrau"));
    QCOMPARE(shownAt(model, 0, StandingOrderTableModel::PurposeColumn), QStringLiteral("Miete"));
    QCOMPARE(shownAt(model, 0, StandingOrderTableModel::NextDateColumn),
             QStringLiteral("01.09.2099"));
    QCOMPARE(shownAt(model, 0, StandingOrderTableModel::IntervalColumn), QStringLiteral("Monthly"));
    QCOMPARE(shownAt(model, 0, StandingOrderTableModel::ValueColumn), QStringLiteral("750,00"));
}

void StandingOrderTableModelTest::theIntervalNamesThePairOfPeriodAndCycle_data()
{
    QTest::addColumn<int>("period");
    QTest::addColumn<quint32>("cycle");
    QTest::addColumn<QString>("shown");

    const auto monthly = static_cast<int>(AB_Transaction_PeriodMonthly);
    const auto weekly = static_cast<int>(AB_Transaction_PeriodWeekly);
    const auto unknown = static_cast<int>(AB_Transaction_PeriodUnknown);

    QTest::newRow("monthly") << monthly << 1u << QStringLiteral("Monthly");
    QTest::newRow("bimonthly") << monthly << 2u << QStringLiteral("Bimonthly");
    QTest::newRow("quarterly") << monthly << 3u << QStringLiteral("Quarterly");
    QTest::newRow("semiannually") << monthly << 6u << QStringLiteral("Semiannually");
    QTest::newRow("annually") << monthly << 12u << QStringLiteral("Annually");
    QTest::newRow("weekly") << weekly << 1u << QStringLiteral("Weekly");
    QTest::newRow("fortnightly") << weekly << 2u << QStringLiteral("Fortnightly");
    QTest::newRow("every four weeks") << weekly << 4u << QStringLiteral("Every 4 weeks");

    // Beyond the named pairs the number carries the interval. Dropping it would
    // turn an order that runs every five months into a monthly one.
    QTest::newRow("five months") << monthly << 5u << QStringLiteral("Every 5 months");
    QTest::newRow("three weeks") << weekly << 3u << QStringLiteral("Every 3 weeks");

    // The institution named no period. What it did name stays visible.
    QTest::newRow("no period") << unknown << 0u << QStringLiteral("Unknown");
    QTest::newRow("no period with cycle") << unknown << 3u << QStringLiteral("Unknown, cycle 3");

    // The period is known and the cycle is not. Neither half is invented.
    QTest::newRow("monthly without cycle")
        << monthly << 0u << QStringLiteral("Monthly, cycle unknown");
    QTest::newRow("weekly without cycle")
        << weekly << 0u << QStringLiteral("Weekly, cycle unknown");
}

void StandingOrderTableModelTest::theIntervalNamesThePairOfPeriodAndCycle()
{
    QFETCH(int, period);
    QFETCH(quint32, cycle);
    QFETCH(QString, shown);

    StandingOrderTableModel model;

    auto spec = StandingOrderSpec{};
    spec.period = static_cast<AB_TRANSACTION_PERIOD>(period);
    spec.cycle = cycle;

    fill(model, BankingItems{orderWith(spec)});

    QCOMPARE(shownAt(model, 0, StandingOrderTableModel::IntervalColumn), shown);
}

/**
 * What a real institution sends: the first execution and the cycle, and no next
 * one. The cell counts the run rather than staying empty.
 *
 * The order runs on the first of every month, so the day it falls on follows
 * from the calendar alone and not from the day the test runs.
 */
void StandingOrderTableModelTest::theCellCountsTheExecutionWhereTheInstitutionNamesNone()
{
    QLocale::setDefault(QLocale(QLocale::German, QLocale::Germany));

    StandingOrderTableModel model;

    const auto today = QDate::currentDate();
    const auto firstOfThisMonth = QDate(today.year(), today.month(), 1);

    auto spec = StandingOrderSpec{};
    spec.period = AB_Transaction_PeriodMonthly;
    spec.cycle = 1;
    spec.firstDate = QDate(today.year(), 1, 1);
    spec.lastDate = QDate();
    spec.nextDate = QDate();

    fill(model, BankingItems{orderWith(spec)});

    const auto expected = today == firstOfThisMonth ? today : firstOfThisMonth.addMonths(1);

    QCOMPARE(shownAt(model, 0, StandingOrderTableModel::NextDateColumn),
             expected.toString(QStringLiteral("dd.MM.yyyy")));

    // The raw field of the institution stays what it was. What the column shows
    // is worked out, and the role goes on carrying what came over the wire.
    QVERIFY(
        !model.data(model.index(0, 0), StandingOrderTableModel::NextDateRole).toDate().isValid());
}

void StandingOrderTableModelTest::anOrderWithoutAnyDateLeavesTheCellEmpty()
{
    StandingOrderTableModel model;

    auto spec = StandingOrderSpec{};
    spec.firstDate = QDate();
    spec.lastDate = QDate();
    spec.nextDate = QDate();

    fill(model, BankingItems{orderWith(spec)});

    QVERIFY(shownAt(model, 0, StandingOrderTableModel::NextDateColumn).isEmpty());
}

void StandingOrderTableModelTest::anAmountCarriesTwoDecimalsAndStandsRight()
{
    QLocale::setDefault(QLocale(QLocale::German, QLocale::Germany));

    StandingOrderTableModel model;

    auto spec = StandingOrderSpec{};
    spec.value = 1234.5;

    fill(model, BankingItems{orderWith(spec)});

    QCOMPARE(shownAt(model, 0, StandingOrderTableModel::ValueColumn), QStringLiteral("1.234,50"));

    const auto alignment = model
                               .data(model.index(0, StandingOrderTableModel::ValueColumn),
                                     Qt::TextAlignmentRole)
                               .toInt();

    QVERIFY((alignment & Qt::AlignRight) != 0);
}

void StandingOrderTableModelTest::aDateCarriesTheDayTheMonthAndFourDigitsOfYear_data()
{
    QTest::addColumn<QLocale>("locale");
    QTest::addColumn<QString>("shown");

    QTest::newRow("de_DE") << QLocale(QLocale::German, QLocale::Germany)
                           << QStringLiteral("01.09.2099");
    QTest::newRow("en_GB") << QLocale(QLocale::English, QLocale::UnitedKingdom)
                           << QStringLiteral("01/09/2099");
}

void StandingOrderTableModelTest::aDateCarriesTheDayTheMonthAndFourDigitsOfYear()
{
    QFETCH(QLocale, locale);
    QFETCH(QString, shown);

    QLocale::setDefault(locale);

    StandingOrderTableModel model;

    auto spec = StandingOrderSpec{};
    spec.nextDate = QDate(2099, 9, 1);

    fill(model, BankingItems{orderWith(spec)});

    QCOMPARE(shownAt(model, 0, StandingOrderTableModel::NextDateColumn), shown);
}

void StandingOrderTableModelTest::theCurrencyStandsInTheHeaderOfTheAmount()
{
    StandingOrderTableModel model;

    fill(model, threeOrders(firstAccount));

    QVERIFY(headerOf(model, StandingOrderTableModel::ValueColumn).contains(QStringLiteral("EUR")));
}

void StandingOrderTableModelTest::theHeaderNamesNoCurrencyWhereTheRowsCarryTwo()
{
    StandingOrderTableModel model;

    auto euro = StandingOrderSpec{};
    euro.currency = QStringLiteral("EUR");

    auto franc = StandingOrderSpec{};
    franc.currency = QStringLiteral("CHF");
    franc.remoteName = QStringLiteral("Zweiter Empfaenger");

    fill(model, BankingItems{orderWith(euro), orderWith(franc)});

    const auto header = headerOf(model, StandingOrderTableModel::ValueColumn);

    QVERIFY(!header.contains(QStringLiteral("EUR")));
    QVERIFY(!header.contains(QStringLiteral("CHF")));
}

void StandingOrderTableModelTest::theDefaultOrderIsThePayeeAscending()
{
    StandingOrderTableModel model;

    QCOMPARE(model.sortColumn(), StandingOrderTableModel::RemoteNameColumn);
    QCOMPARE(model.sortOrder(), Qt::AscendingOrder);

    fill(model, threeOrders(firstAccount));

    QCOMPARE(columnOf(model, StandingOrderTableModel::RemoteNameColumn),
             QStringList(
                 {QStringLiteral("Alice"), QStringLiteral("Bob"), QStringLiteral("Charlie")}));
}

void StandingOrderTableModelTest::everyColumnOrdersUpAndDown_data()
{
    QTest::addColumn<int>("column");
    QTest::addColumn<int>("role");

    QTest::newRow("payee") << static_cast<int>(StandingOrderTableModel::RemoteNameColumn)
                           << static_cast<int>(StandingOrderTableModel::RemoteNameRole);
    QTest::newRow("purpose") << static_cast<int>(StandingOrderTableModel::PurposeColumn)
                             << static_cast<int>(StandingOrderTableModel::PurposeRole);
    QTest::newRow("execution") << static_cast<int>(StandingOrderTableModel::NextDateColumn)
                               << static_cast<int>(StandingOrderTableModel::NextDateRole);
    QTest::newRow("amount") << static_cast<int>(StandingOrderTableModel::ValueColumn)
                            << static_cast<int>(StandingOrderTableModel::ValueRole);
}

void StandingOrderTableModelTest::everyColumnOrdersUpAndDown()
{
    QFETCH(int, column);
    QFETCH(int, role);

    StandingOrderTableModel model;
    fill(model, threeOrders(firstAccount));

    model.sort(column, Qt::AscendingOrder);
    QVERIFY(isOrdered(valuesOf(model, role), Qt::AscendingOrder));

    model.sort(column, Qt::DescendingOrder);
    QVERIFY(isOrdered(valuesOf(model, role), Qt::DescendingOrder));
}

void StandingOrderTableModelTest::theIntervalOrdersByItsLengthAndNotByItsWord()
{
    StandingOrderTableModel model;
    fill(model, threeOrders(firstAccount));

    model.sort(StandingOrderTableModel::IntervalColumn, Qt::AscendingOrder);

    // Weekly, monthly, quarterly. By the alphabet the quarterly one would stand
    // in front, which says nothing about how often an order runs.
    QCOMPARE(columnOf(model, StandingOrderTableModel::IntervalColumn),
             QStringList({QStringLiteral("Weekly"),
                          QStringLiteral("Monthly"),
                          QStringLiteral("Quarterly")}));
}

void StandingOrderTableModelTest::anAccountWithOrdersShowsThem()
{
    Storage storage(applicationInfo());
    QVERIFY(openStorage(storage));
    QVERIFY(storeAccount(storage, firstAccount));
    QVERIFY(storeAndWait(storage, threeOrders(firstAccount)));

    StandingOrderTableModel model;
    model.setStorage(&storage);
    model.setAccountId(firstAccount);

    QTRY_VERIFY_WITH_TIMEOUT(!model.isReading(), workerTimeoutMs);
    QCOMPARE(model.rowCount(), 3);
}

void StandingOrderTableModelTest::anAccountWithoutAnOrderShowsNoRowAndNoFailure()
{
    Storage storage(applicationInfo());
    QVERIFY(openStorage(storage));
    QVERIFY(storeAccount(storage, firstAccount));
    QVERIFY(storeAccount(storage, secondAccount));
    QVERIFY(storeAndWait(storage, threeOrders(firstAccount)));

    StandingOrderTableModel model;
    QSignalSpy refusedSpy(&model, &StandingOrderTableModel::readRefused);

    model.setStorage(&storage);
    model.setAccountId(secondAccount);

    QTRY_VERIFY_WITH_TIMEOUT(!model.isReading(), workerTimeoutMs);

    QCOMPARE(model.rowCount(), 0);
    QVERIFY(refusedSpy.isEmpty());
}

void StandingOrderTableModelTest::anEndedOrderDoesNotAppear()
{
    Storage storage(applicationInfo());
    QVERIFY(openStorage(storage));
    QVERIFY(storeAccount(storage, firstAccount));

    const auto orders = threeOrders(firstAccount);
    QVERIFY(storeAndWait(storage, orders));

    // A fetch that went through and reported two of the three. The third is
    // marked as ended by the write itself.
    QVERIFY(storeAndWait(storage,
                         BankingItems{orders.at(0), orders.at(1)},
                         StandingOrderRun{firstAccount, true}));

    StandingOrderTableModel model;
    model.setStorage(&storage);
    model.setAccountId(firstAccount);

    QTRY_VERIFY_WITH_TIMEOUT(!model.isReading(), workerTimeoutMs);
    QCOMPARE(model.rowCount(), 2);
}

void StandingOrderTableModelTest::anAccountWhoseOrdersAllEndedShowsNoRow()
{
    Storage storage(applicationInfo());
    QVERIFY(openStorage(storage));
    QVERIFY(storeAccount(storage, firstAccount));
    QVERIFY(storeAndWait(storage, threeOrders(firstAccount)));

    // A fetch that went through and brought nothing. Every order of the account
    // is marked as ended.
    QVERIFY(storeAndWait(storage, BankingItems{}, StandingOrderRun{firstAccount, true}));

    StandingOrderTableModel model;
    model.setStorage(&storage);
    model.setAccountId(firstAccount);

    QTRY_VERIFY_WITH_TIMEOUT(!model.isReading(), workerTimeoutMs);
    QCOMPARE(model.rowCount(), 0);
}

void StandingOrderTableModelTest::aChangeOfAccountSwitchesTheView()
{
    Storage storage(applicationInfo());
    QVERIFY(openStorage(storage));
    QVERIFY(storeAccount(storage, firstAccount));
    QVERIFY(storeAccount(storage, secondAccount));
    QVERIFY(storeAndWait(storage, threeOrders(firstAccount)));

    auto single = StandingOrderSpec{};
    single.uniqueAccountId = secondAccount;
    single.remoteName = QStringLiteral("Only One");
    QVERIFY(storeAndWait(storage, BankingItems{orderWith(single)}));

    StandingOrderTableModel model;
    model.setStorage(&storage);

    model.setAccountId(firstAccount);
    QTRY_VERIFY_WITH_TIMEOUT(!model.isReading(), workerTimeoutMs);
    QCOMPARE(model.rowCount(), 3);

    model.setAccountId(secondAccount);
    QTRY_COMPARE_WITH_TIMEOUT(model.rowCount(), 1, workerTimeoutMs);
    QCOMPARE(shownAt(model, 0, StandingOrderTableModel::RemoteNameColumn),
             QStringLiteral("Only One"));
}

void StandingOrderTableModelTest::aFailedReadLeavesTheRowsStanding()
{
    Storage storage(applicationInfo());
    QVERIFY(openStorage(storage));
    QVERIFY(storeAccount(storage, firstAccount));
    QVERIFY(storeAndWait(storage, threeOrders(firstAccount)));

    StandingOrderTableModel model;
    model.setStorage(&storage);
    model.setAccountId(firstAccount);

    QTRY_VERIFY_WITH_TIMEOUT(!model.isReading(), workerTimeoutMs);
    QCOMPARE(model.rowCount(), 3);

    QSignalSpy refusedSpy(&model, &StandingOrderTableModel::readRefused);

    // The file is gone from under the model. What stands was read from this
    // account and stays where it is; the failure is reported instead.
    storage.close();
    model.refresh();

    QTRY_VERIFY_WITH_TIMEOUT(!model.isReading(), workerTimeoutMs);

    QCOMPARE(model.rowCount(), 3);
    QCOMPARE(refusedSpy.count(), 1);
}

void StandingOrderTableModelTest::aRequestTheStorageTurnedDownIsReportedRatherThanSwallowed()
{
    Storage storage(applicationInfo());

    // Never opened. A read of it cannot start a run, so nothing of the read path
    // will ever arrive and the model has to say so itself.
    StandingOrderTableModel model;
    QSignalSpy refusedSpy(&model, &StandingOrderTableModel::readRefused);

    model.setStorage(&storage);
    model.setAccountId(firstAccount);

    QCOMPARE(refusedSpy.count(), 1);
    QVERIFY(!model.isReading());
    QCOMPARE(model.rowCount(), 0);
}

} // namespace olbaflinx::ui::models::tests

QTEST_MAIN(olbaflinx::ui::models::tests::StandingOrderTableModelTest)

#include "tst_standingordertablemodel.moc"
