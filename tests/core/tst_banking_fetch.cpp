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
#include "core/Banking/Balance/Balance.h"
#include "core/Banking/Banking.h"
#include "core/Banking/Transaction/Transaction.h"
#include "core/Error.h"

#include "BankingHelpers.h"
#include "TestHelpers.h"

#include <aqbanking/banking.h>

#include <QtTest/QtTest>

#include <memory>

using namespace olbaflinx::core;
using namespace olbaflinx::core::banking;
using namespace olbaflinx::core::banking::account;
using namespace olbaflinx::core::banking::balance;
using namespace olbaflinx::core::banking::transaction;

namespace olbaflinx::core::banking::tests {

using namespace olbaflinx::core::tests;

class BankingFetchTest final : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> bankingHome;

    static ApplicationInfo applicationInfo()
    {
        return TestHelpers::applicationInfo(QStringLiteral("OlbaFlinxBankingFetchTest"));
    }

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();

    void commandsCarryBothRequestsAndTheIdOfTheGivenAccount();
    void commandsLeaveTheFieldTheBackendKeepsEmpty();
    void fiveBookingsAndABookedBalanceArriveAsFiveTransactionsAndOneBalance();
    void anEmptyContainerAnswersWithAnEmptyResult();
    void aFailedCommandDropsTheWholeAccount();
    void aCommandThatBringsNothingLeavesTheTransactionsAlone();
    void anAccountWithoutOnlineAccessIsSkippedWithAReason();

    void bothOrdersStartThirtyDaysBeforeTheYoungestStoredBooking();
    void withoutAStoredBookingTheOrdersCarryNoStartingPoint();
    void aPeriodNarrowerThanAskedForIsNoFailure();
};

namespace {

constexpr quint32 testAccountId = 4711;
constexpr int bookingCount = 5;

} // namespace

/**
 * AqBanking keeps its configuration below AQBANKING_HOME. Without pointing that
 * at a directory of our own, every run would write into the configuration of
 * whoever started it.
 */
void BankingFetchTest::initTestCase()
{
    bankingHome = std::make_unique<QTemporaryDir>();
    QVERIFY(bankingHome->isValid());

    QVERIFY(qputenv("AQBANKING_HOME", bankingHome->path().toUtf8()));
}

void BankingFetchTest::cleanupTestCase()
{
    qunsetenv("AQBANKING_HOME");
    bankingHome.reset();
}

/**
 * The order has to be readable before it is sent, which is what keeps this
 * measurable without a bank.
 */
void BankingFetchTest::commandsCarryBothRequestsAndTheIdOfTheGivenAccount()
{
    const auto account = BankingHelpers::accountFromBackend(testAccountId);

    AB_TRANSACTION_LIST2 *commands = Banking::buildFetchCommands(*account, QDate(2026, 1, 1));
    QVERIFY(commands != nullptr);

    QCOMPARE(AB_Transaction_List2_GetSize(commands), 2u);

    AB_TRANSACTION *transactions
        = BankingHelpers::commandOfKind(commands, AB_Transaction_CommandGetTransactions);
    AB_TRANSACTION *balance = BankingHelpers::commandOfKind(commands,
                                                            AB_Transaction_CommandGetBalance);

    QVERIFY(transactions != nullptr);
    QVERIFY(balance != nullptr);

    QCOMPARE(AB_Transaction_GetUniqueAccountId(transactions), testAccountId);
    QCOMPARE(AB_Transaction_GetUniqueAccountId(balance), testAccountId);

    // The period travels in FirstDate and LastDate; the FinTS backend reads them
    // as the range to fetch.
    QVERIFY(AB_Transaction_GetFirstDate(transactions) != nullptr);

    AB_Transaction_List2_freeAll(commands);
}

/**
 * AB_Transaction_free does not release StringIdForApplication. The project
 * therefore never fills it, so nothing is leaked through it.
 */
void BankingFetchTest::commandsLeaveTheFieldTheBackendKeepsEmpty()
{
    const auto account = BankingHelpers::accountFromBackend(testAccountId);

    AB_TRANSACTION_LIST2 *commands = Banking::buildFetchCommands(*account, QDate(2026, 1, 1));
    QVERIFY(commands != nullptr);

    AB_TRANSACTION_LIST2_ITERATOR *iterator = AB_Transaction_List2_First(commands);
    QVERIFY(iterator != nullptr);

    AB_TRANSACTION *command = AB_Transaction_List2Iterator_Data(iterator);
    while (command != nullptr) {
        const char *stringId = AB_Transaction_GetStringIdForApplication(command);
        QVERIFY(stringId == nullptr || *stringId == '\0');

        command = AB_Transaction_List2Iterator_Next(iterator);
    }

    AB_Transaction_List2Iterator_free(iterator);
    AB_Transaction_List2_freeAll(commands);
}

/**
 * The booked balance wins over the more recent noted one. The type decides
 * before the date does.
 */
void BankingFetchTest::fiveBookingsAndABookedBalanceArriveAsFiveTransactionsAndOneBalance()
{
    const auto account = BankingHelpers::accountFromBackend(testAccountId);

    AB_TRANSACTION_LIST2 *commands = Banking::buildFetchCommands(*account, QDate(2026, 1, 1));

    AB_IMEXPORTER_CONTEXT *context
        = BankingHelpers::responseContext(testAccountId,
                                          bookingCount,
                                          {{AB_Balance_TypeNoted, QDate(2026, 2, 2), 17.50},
                                           {AB_Balance_TypeBooked, QDate(2026, 2, 1), 1234.56}});

    const BankingItems items = Banking::itemsFromContext(context, commands);

    AB_ImExporterContext_free(context);
    AB_Transaction_List2_freeAll(commands);

    const BankingItems transactions = BankingHelpers::itemsOfType(items,
                                                                  QStringLiteral("Transaction"));
    const BankingItems balances = BankingHelpers::itemsOfType(items, QStringLiteral("Balance"));

    QCOMPARE(transactions.size(), bookingCount);
    QCOMPARE(balances.size(), 1);

    for (const BankingItemPtr &item : transactions) {
        const auto transaction = std::static_pointer_cast<Transaction>(item);
        QCOMPARE(transaction->uniqueAccountId(), testAccountId);
    }

    const auto balance = std::static_pointer_cast<Balance>(balances.first());
    QCOMPARE(balance->uniqueAccountId(), testAccountId);
    QCOMPARE(balance->type(), AB_Balance_TypeBooked);
    QCOMPARE(balance->date(), QDate(2026, 2, 1));
    QCOMPARE(balance->currency(), QStringLiteral("EUR"));
}

/**
 * An account with nothing new is not an error. Storage answers an empty read
 * with NotFound, and a fetch deliberately does not.
 */
void BankingFetchTest::anEmptyContainerAnswersWithAnEmptyResult()
{
    const auto account = BankingHelpers::accountFromBackend(testAccountId);

    AB_TRANSACTION_LIST2 *commands = Banking::buildFetchCommands(*account, QDate(2026, 1, 1));
    AB_IMEXPORTER_CONTEXT *context = AB_ImExporterContext_new();

    const BankingItems items = Banking::itemsFromContext(context, commands);

    AB_ImExporterContext_free(context);
    AB_Transaction_List2_freeAll(commands);

    QVERIFY(items.isEmpty());
}

/**
 * All or nothing per account: a command that ends in an error takes the other
 * one down with it, transactions included.
 */
void BankingFetchTest::aFailedCommandDropsTheWholeAccount()
{
    const auto account = BankingHelpers::accountFromBackend(testAccountId);

    AB_TRANSACTION_LIST2 *commands = Banking::buildFetchCommands(*account, QDate(2026, 1, 1));

    AB_TRANSACTION *balanceCommand = BankingHelpers::commandOfKind(commands,
                                                                   AB_Transaction_CommandGetBalance);
    QVERIFY(balanceCommand != nullptr);
    AB_Transaction_SetStatus(balanceCommand, AB_Transaction_StatusError);

    AB_IMEXPORTER_CONTEXT *context = BankingHelpers::responseContext(testAccountId,
                                                                     bookingCount,
                                                                     {{AB_Balance_TypeBooked,
                                                                       QDate(2026, 2, 1),
                                                                       1234.56}});

    const BankingItems items = Banking::itemsFromContext(context, commands);

    AB_ImExporterContext_free(context);
    AB_Transaction_List2_freeAll(commands);

    QVERIFY(items.isEmpty());
}

/**
 * The other half of the same decision: a command that runs through and brings
 * nothing is not a failure, so the transactions stand and only the balance is
 * missing.
 */
void BankingFetchTest::aCommandThatBringsNothingLeavesTheTransactionsAlone()
{
    const auto account = BankingHelpers::accountFromBackend(testAccountId);

    AB_TRANSACTION_LIST2 *commands = Banking::buildFetchCommands(*account, QDate(2026, 1, 1));

    AB_TRANSACTION *balanceCommand = BankingHelpers::commandOfKind(commands,
                                                                   AB_Transaction_CommandGetBalance);
    QVERIFY(balanceCommand != nullptr);
    AB_Transaction_SetStatus(balanceCommand, AB_Transaction_StatusAccepted);

    AB_IMEXPORTER_CONTEXT *context = BankingHelpers::responseContext(testAccountId,
                                                                     bookingCount,
                                                                     {});

    const BankingItems items = Banking::itemsFromContext(context, commands);

    AB_ImExporterContext_free(context);
    AB_Transaction_List2_freeAll(commands);

    QCOMPARE(BankingHelpers::itemsOfType(items, QStringLiteral("Transaction")).size(), bookingCount);
    QVERIFY(BankingHelpers::itemsOfType(items, QStringLiteral("Balance")).isEmpty());
}

/**
 * An account without online access is skipped before any session starts. It is
 * not a failure of the session: AqBanking refuses the whole run when an account
 * carries no backend, so the other accounts of a run would go down with it.
 */
void BankingFetchTest::anAccountWithoutOnlineAccessIsSkippedWithAReason()
{
    Banking banking(applicationInfo());

    const ScopedConsoleGui gui;

    QVERIFY(!banking
                 .initialize(QStringLiteral("OlbaFlinxBankingFetchTest"),
                             QStringLiteral("1.0.0"),
                             QStringLiteral("0123456789ABCDEF"),
                             gui.get())
                 .isError());

    QSignalSpy skippedSpy(&banking, &Banking::accountSkipped);
    QSignalSpy errorSpy(&banking, &Banking::errorOccurred);
    QSignalSpy itemsSpy(&banking, &Banking::itemsReceived);
    QSignalSpy finishedSpy(&banking, &Banking::finished);

    const auto account = BankingHelpers::accountFromBackend(testAccountId, "");

    banking.fetchAccount(*account, QDate(2026, 1, 1));

    QCOMPARE(skippedSpy.count(), 1);
    QCOMPARE(errorSpy.count(), 0);
    QCOMPARE(itemsSpy.count(), 0);
    QCOMPARE(finishedSpy.count(), 1);

    const auto arguments = skippedSpy.takeFirst();
    QCOMPARE(arguments.at(0).value<quint32>(), testAccountId);
    QVERIFY(!arguments.at(1).toString().isEmpty());

    banking.finalize();
}

/**
 * The lead time of thirty days, measured where it is formed. A bank corrects
 * bookings after it has reported them, so a fetch that started where the
 * holding ends would never see the correction.
 */
void BankingFetchTest::bothOrdersStartThirtyDaysBeforeTheYoungestStoredBooking()
{
    const auto account = BankingHelpers::accountFromBackend(testAccountId);

    AB_TRANSACTION_LIST2 *commands = Banking::buildFetchCommands(*account, QDate(2026, 6, 1));
    QVERIFY(commands != nullptr);

    AB_TRANSACTION *transactions
        = BankingHelpers::commandOfKind(commands, AB_Transaction_CommandGetTransactions);
    AB_TRANSACTION *balance = BankingHelpers::commandOfKind(commands,
                                                            AB_Transaction_CommandGetBalance);

    QVERIFY(transactions != nullptr);
    QVERIFY(balance != nullptr);

    QCOMPARE(Transaction(transactions).firstDate(), QDate(2026, 5, 2));
    QCOMPARE(Transaction(balance).firstDate(), QDate(2026, 5, 2));

    AB_Transaction_List2_freeAll(commands);
}

/**
 * The first fetch of an account. Without a starting point the bank delivers
 * what it holds, and nothing is subtracted from a date that is not there.
 */
void BankingFetchTest::withoutAStoredBookingTheOrdersCarryNoStartingPoint()
{
    const auto account = BankingHelpers::accountFromBackend(testAccountId);

    AB_TRANSACTION_LIST2 *commands = Banking::buildFetchCommands(*account, QDate());
    QVERIFY(commands != nullptr);

    AB_TRANSACTION *transactions
        = BankingHelpers::commandOfKind(commands, AB_Transaction_CommandGetTransactions);
    AB_TRANSACTION *balance = BankingHelpers::commandOfKind(commands,
                                                            AB_Transaction_CommandGetBalance);

    QVERIFY(transactions != nullptr);
    QVERIFY(balance != nullptr);

    QVERIFY(AB_Transaction_GetFirstDate(transactions) == nullptr);
    QVERIFY(AB_Transaction_GetFirstDate(balance) == nullptr);

    AB_Transaction_List2_freeAll(commands);
}

/**
 * A bank may deliver less than it was asked for. What comes back is taken as it
 * is: the core compares no period and reports no failure over one.
 */
void BankingFetchTest::aPeriodNarrowerThanAskedForIsNoFailure()
{
    const auto account = BankingHelpers::accountFromBackend(testAccountId);

    AB_TRANSACTION_LIST2 *commands = Banking::buildFetchCommands(*account, QDate(2026, 6, 1));

    // Asked for from 2 May on, answered from 20 June on.
    AB_IMEXPORTER_CONTEXT *context = BankingHelpers::responseContext(testAccountId,
                                                                     bookingCount,
                                                                     {},
                                                                     QDate(2026, 6, 20));

    const BankingItems items = Banking::itemsFromContext(context, commands);

    const BankingItems transactions = BankingHelpers::itemsOfType(items,
                                                                  QStringLiteral("Transaction"));

    QCOMPARE(transactions.size(), bookingCount);

    for (const BankingItemPtr &item : transactions) {
        const auto transaction = std::static_pointer_cast<Transaction>(item);
        QVERIFY(transaction->date() >= QDate(2026, 6, 20));
    }

    AB_ImExporterContext_free(context);
    AB_Transaction_List2_freeAll(commands);
}

} // namespace olbaflinx::core::banking::tests

QTEST_APPLESS_MAIN(olbaflinx::core::banking::tests::BankingFetchTest)

#include "tst_banking_fetch.moc"
