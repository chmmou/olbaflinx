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

#include <gwenhywfar/error.h>

#include <QtTest/QtTest>

#include <QtCore/QList>
#include <QtCore/QSet>

#include <memory>
#include <utility>

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

    /** The backend, brought up against the given interface. */
    static Error initialized(Banking &banking, GWEN_GUI *gui)
    {
        return banking.initialize(QStringLiteral("OlbaFlinxBankingFetchTest"),
                                  QStringLiteral("1.0.0"),
                                  QStringLiteral("0123456789ABCDEF"),
                                  gui);
    }

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();

    void commandsCarryBothRequestsAndTheIdOfTheGivenAccount();
    void commandsLeaveTheFieldTheBackendKeepsEmpty();
    void anAccountThatOffersNoTransactionsIsAskedForItsBalanceAlone();
    void anAccountThatOffersNothingIsAskedForNothing();
    void aDescriptionThatNamesNoOrderRefusesNone();
    void fiveBookingsAndABookedBalanceArriveAsFiveTransactionsAndOneBalance();
    void anEmptyContainerAnswersWithAnEmptyResult();
    void aFailedCommandDropsTheWholeAccount();
    void aCommandThatBringsNothingLeavesTheTransactionsAlone();
    void anAccountWithoutOnlineAccessIsSkippedWithAReason();

    void bothOrdersStartThirtyDaysBeforeTheYoungestStoredBooking();
    void withoutAStoredBookingTheOrdersCarryNoStartingPoint();
    void aPeriodNarrowerThanAskedForIsNoFailure();

    void aFetchAnswersOnlyAfterItHasReturned();
    void theSessionRunsBesideTheCallerAndAsksTheInterfaceItWasGiven();
    void aSecondFetchIsRefusedWhileOneRunsAndNeverReachesTheBackend();
    void aFetchIsTakenAgainOnceTheRunningOneHasEnded();
    void theBackendGoesDownAfterASessionHasEnded();

    void anAbortByTheUserIsToldApartFromAFailure();
    void aSessionCutInTheMiddleIsAFailureAndNoAbort();
    void aRefusedOrderIsAFailureOfItsAccountAndNoEmptyResult();

    void aFailedAccountLeavesTheOthersOfTheSameListAlone();
    void twoAccountsOfOneInstitutionFallTogetherWhenTheirSessionFails();
    void aFetchForAllAccountsPassesOverEveryAccountWithoutOnlineAccess();
};

namespace {

constexpr quint32 testAccountId = 4711;
constexpr int bookingCount = 5;

/**
 * How long a spy waits for something a session has to produce first. The call
 * returns the moment the signal arrives, so nothing sleeps for it.
 */
constexpr int sessionTimeoutMs = 30000;

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

namespace {

/** The description the backend holds, carrying limits for the given orders. */
AB_ACCOUNT_SPEC *descriptionOffering(std::initializer_list<AB_TRANSACTION_COMMAND> commands)
{
    AB_ACCOUNT_SPEC *spec = AB_AccountSpec_new();
    AB_AccountSpec_SetUniqueId(spec, testAccountId);

    AB_TRANSACTION_LIMITS_LIST *limits = AB_TransactionLimits_List_new();

    for (const auto command : commands) {
        AB_TRANSACTION_LIMITS *entry = AB_TransactionLimits_new();
        AB_TransactionLimits_SetCommand(entry, command);
        AB_TransactionLimits_List_Add(entry, limits);
    }

    // The setter takes the list over, it does not copy it. Releasing it here
    // would leave the description pointing at freed memory.
    AB_AccountSpec_SetTransactionLimitsList(spec, limits);

    return spec;
}

/** The kinds of order in a list, in the order they were put in. */
QList<AB_TRANSACTION_COMMAND> commandsIn(AB_TRANSACTION_LIST2 *commands)
{
    QList<AB_TRANSACTION_COMMAND> kinds;

    AB_TRANSACTION_LIST2_ITERATOR *iterator = AB_Transaction_List2_First(commands);
    if (iterator == nullptr) {
        return kinds;
    }

    AB_TRANSACTION *command = AB_Transaction_List2Iterator_Data(iterator);
    while (command != nullptr) {
        kinds << AB_Transaction_GetCommand(command);
        command = AB_Transaction_List2Iterator_Next(iterator);
    }

    AB_Transaction_List2Iterator_free(iterator);

    return kinds;
}

} // namespace

/**
 * An order the backend cannot build for an account never reaches the bank: it is
 * marked as failed while the queue is filled, and that failure counts against
 * the whole account. The balance the same session brought would go down with it.
 *
 * The backend says beforehand which orders it holds, by writing their limits
 * into the description of the account.
 */
void BankingFetchTest::anAccountThatOffersNoTransactionsIsAskedForItsBalanceAlone()
{
    const auto account = BankingHelpers::accountFromBackend(testAccountId);

    AB_ACCOUNT_SPEC *offered = descriptionOffering({AB_Transaction_CommandGetBalance});

    QVERIFY(!Banking::accountOffers(offered, AB_Transaction_CommandGetTransactions));
    QVERIFY(Banking::accountOffers(offered, AB_Transaction_CommandGetBalance));

    AB_TRANSACTION_LIST2 *commands = Banking::buildFetchCommands(*account,
                                                                 QDate(2026, 1, 1),
                                                                 offered);
    QVERIFY(commands != nullptr);

    const auto kinds = commandsIn(commands);
    QCOMPARE(kinds.size(), 1);
    QCOMPARE(kinds.at(0), AB_Transaction_CommandGetBalance);

    AB_Transaction_List2_freeAll(commands);
    AB_AccountSpec_free(offered);
}

/**
 * The counterpart, and the one the caller has to tell apart: nothing is left to
 * send, so no session is worth running for this account. A description that
 * names other orders and neither of these two is what says so.
 */
void BankingFetchTest::anAccountThatOffersNothingIsAskedForNothing()
{
    const auto account = BankingHelpers::accountFromBackend(testAccountId);

    AB_ACCOUNT_SPEC *offered = descriptionOffering({AB_Transaction_CommandSepaTransfer});

    AB_TRANSACTION_LIST2 *commands = Banking::buildFetchCommands(*account,
                                                                 QDate(2026, 1, 1),
                                                                 offered);
    QVERIFY(commands != nullptr);
    QVERIFY(commandsIn(commands).isEmpty());

    AB_Transaction_List2_freeAll(commands);
    AB_AccountSpec_free(offered);
}

/**
 * Silence is not a refusal, and the difference decides whether a fetch happens
 * at all. The field is documented as one a backend may leave empty, and the
 * description a stored account carries is empty in exactly that way.
 */
void BankingFetchTest::aDescriptionThatNamesNoOrderRefusesNone()
{
    const auto account = BankingHelpers::accountFromBackend(testAccountId);

    // Nothing at all: the backend does not hold this account.
    QVERIFY(Banking::accountOffers(nullptr, AB_Transaction_CommandGetTransactions));
    QVERIFY(Banking::accountOffers(nullptr, AB_Transaction_CommandGetBalance));

    // Held, but naming no order.
    AB_ACCOUNT_SPEC *silent = descriptionOffering({});

    QVERIFY(Banking::accountOffers(silent, AB_Transaction_CommandGetTransactions));
    QVERIFY(Banking::accountOffers(silent, AB_Transaction_CommandGetBalance));

    AB_TRANSACTION_LIST2 *commands = Banking::buildFetchCommands(*account,
                                                                 QDate(2026, 1, 1),
                                                                 silent);
    QVERIFY(commands != nullptr);
    QCOMPARE(commandsIn(commands).size(), 2);

    AB_Transaction_List2_freeAll(commands);
    AB_AccountSpec_free(silent);
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
    QVERIFY(!initialized(banking, gui.get()).isError());

    QSignalSpy skippedSpy(&banking, &Banking::accountSkipped);
    QSignalSpy errorSpy(&banking, &Banking::errorOccurred);
    QSignalSpy itemsSpy(&banking, &Banking::itemsReceived);
    QSignalSpy finishedSpy(&banking, &Banking::finished);

    const auto account = BankingHelpers::accountFromBackend(testAccountId, "");

    banking.fetchAccount(*account, QDate(2026, 1, 1));

    // Reported through the event loop, like every other way out of a fetch.
    QVERIFY(skippedSpy.wait(sessionTimeoutMs));

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

/**
 * The promise is that a fetch returns at once and answers later. A caller that
 * sets its own state after the call would otherwise see the end of a fetch
 * before its start.
 *
 * No clock is measured here. A session cannot be held to a duration, and the
 * clock would then be measuring the bank.
 */
void BankingFetchTest::aFetchAnswersOnlyAfterItHasReturned()
{
    Banking banking(applicationInfo());

    const ScopedConsoleGui gui;
    QVERIFY(!initialized(banking, gui.get()).isError());

    QSignalSpy finishedSpy(&banking, &Banking::finished);

    const auto account = BankingHelpers::accountFromBackend(testAccountId);

    banking.fetchAccount(*account, QDate(2026, 1, 1));

    // Back here with nothing reported yet, and the thread that asked goes on.
    QCOMPARE(finishedSpy.count(), 0);

    QVERIFY(finishedSpy.wait(sessionTimeoutMs));

    banking.finalize();
}

/**
 * The interface of gwenhywfar lives per thread. The one set where initialize
 * ran does not reach the session, and without one there the library aborts the
 * process instead of reporting a failure. What shows that the session set it is
 * that the interface answers at all, and it answers from another thread.
 */
void BankingFetchTest::theSessionRunsBesideTheCallerAndAsksTheInterfaceItWasGiven()
{
    Banking banking(applicationInfo());

    const ScopedHoldingGui gui;
    QVERIFY(!initialized(banking, gui.get()).isError());

    QSignalSpy finishedSpy(&banking, &Banking::finished);

    const auto account = BankingHelpers::accountFromBackend(testAccountId);

    banking.fetchAccount(*account, QDate(2026, 1, 1));

    QTRY_COMPARE_WITH_TIMEOUT(ScopedHoldingGui::progressCount(), 1, sessionTimeoutMs);

    QVERIFY(ScopedHoldingGui::sessionThread() != nullptr);
    QVERIFY(ScopedHoldingGui::sessionThread() != QThread::currentThread());

    ScopedHoldingGui::release();
    QVERIFY(finishedSpy.wait(sessionTimeoutMs));

    banking.finalize();
}

/**
 * Two fetches do not run at once. The refused one is told so and gets no
 * finished of its own: that one belongs to the fetch that is running and would
 * declare it over.
 */
void BankingFetchTest::aSecondFetchIsRefusedWhileOneRunsAndNeverReachesTheBackend()
{
    Banking banking(applicationInfo());

    const ScopedHoldingGui gui;
    QVERIFY(!initialized(banking, gui.get()).isError());

    QSignalSpy errorSpy(&banking, &Banking::errorOccurred);
    QSignalSpy finishedSpy(&banking, &Banking::finished);

    const auto account = BankingHelpers::accountFromBackend(testAccountId);

    banking.fetchAccount(*account, QDate(2026, 1, 1));

    // The session stands at its first progress from here on.
    QTRY_COMPARE_WITH_TIMEOUT(ScopedHoldingGui::progressCount(), 1, sessionTimeoutMs);

    banking.fetchAccount(*account, QDate(2026, 1, 1));

    QVERIFY(errorSpy.wait(sessionTimeoutMs));
    QCOMPARE(errorSpy.count(), 1);
    QCOMPARE(errorSpy.first().at(0).value<ErrorCode>(), ErrorCode::InvalidInput);
    QCOMPARE(finishedSpy.count(), 0);

    // The refused fetch never opened a session of its own: nothing has been
    // released yet, so a second one would be standing here too.
    QCOMPARE(ScopedHoldingGui::progressCount(), 1);

    ScopedHoldingGui::release();
    QVERIFY(finishedSpy.wait(sessionTimeoutMs));

    banking.finalize();
}

/**
 * What is refused while a fetch runs is taken once it has ended, and no run is
 * left behind that would keep the next one out.
 */
void BankingFetchTest::aFetchIsTakenAgainOnceTheRunningOneHasEnded()
{
    Banking banking(applicationInfo());

    const ScopedHoldingGui gui;
    QVERIFY(!initialized(banking, gui.get()).isError());

    QSignalSpy errorSpy(&banking, &Banking::errorOccurred);
    QSignalSpy finishedSpy(&banking, &Banking::finished);

    const auto account = BankingHelpers::accountFromBackend(testAccountId);

    banking.fetchAccount(*account, QDate(2026, 1, 1));
    QTRY_COMPARE_WITH_TIMEOUT(ScopedHoldingGui::progressCount(), 1, sessionTimeoutMs);

    ScopedHoldingGui::release();
    QVERIFY(finishedSpy.wait(sessionTimeoutMs));

    const int afterTheFirst = ScopedHoldingGui::progressCount();

    errorSpy.clear();
    finishedSpy.clear();

    banking.fetchAccount(*account, QDate(2026, 1, 1));
    QVERIFY(finishedSpy.wait(sessionTimeoutMs));

    // Taken, not refused: the second one opened a session of its own, and no
    // refusal was reported for it.
    QVERIFY(ScopedHoldingGui::progressCount() > afterTheFirst);
    for (const QList<QVariant> &arguments : std::as_const(errorSpy)) {
        QVERIFY(arguments.at(0).value<ErrorCode>() != ErrorCode::InvalidInput);
    }

    banking.finalize();
}

/**
 * A session reaches into the backend from a thread of its own. Taking the
 * backend away under it would leave it writing into freed memory, so the
 * shutdown has to be safe once the session has ended.
 */
void BankingFetchTest::theBackendGoesDownAfterASessionHasEnded()
{
    auto banking = std::make_unique<Banking>(applicationInfo());

    const ScopedConsoleGui gui;
    QVERIFY(!initialized(*banking, gui.get()).isError());

    QSignalSpy finishedSpy(banking.get(), &Banking::finished);

    const auto account = BankingHelpers::accountFromBackend(testAccountId);

    banking->fetchAccount(*account, QDate(2026, 1, 1));
    QVERIFY(finishedSpy.wait(sessionTimeoutMs));

    // The backend first, the interface after it.
    banking.reset();

    QVERIFY(true);
}

/**
 * The user interface needs the abort told apart from the failure: it says
 * something else, and it keeps what an earlier account of the same run had
 * already brought.
 */
void BankingFetchTest::anAbortByTheUserIsToldApartFromAFailure()
{
    const auto account = BankingHelpers::accountFromBackend(testAccountId);

    AB_TRANSACTION_LIST2 *commands = Banking::buildFetchCommands(*account, QDate(2026, 1, 1));

    QCOMPARE(Banking::outcomeOfSession(GWEN_ERROR_USER_ABORTED, commands, testAccountId),
             FetchOutcome::Aborted);

    AB_Transaction_List2_freeAll(commands);
}

/**
 * A session that is cut in the middle, say because the far end went away,
 * answers with something else than the abort and stays a failure. Reading it as
 * an abort would tell the user they stopped something they did not.
 */
void BankingFetchTest::aSessionCutInTheMiddleIsAFailureAndNoAbort()
{
    const auto account = BankingHelpers::accountFromBackend(testAccountId);

    AB_TRANSACTION_LIST2 *commands = Banking::buildFetchCommands(*account, QDate(2026, 1, 1));

    QCOMPARE(Banking::outcomeOfSession(GWEN_ERROR_IO, commands, testAccountId),
             FetchOutcome::Failed);

    AB_Transaction_List2_freeAll(commands);
}

/**
 * A session can come back successful and still carry an order the bank refused.
 * Without the distinction the account would answer with an empty list, and a
 * refusal would read like an account with nothing new.
 */
void BankingFetchTest::aRefusedOrderIsAFailureOfItsAccountAndNoEmptyResult()
{
    const auto account = BankingHelpers::accountFromBackend(testAccountId);

    AB_TRANSACTION_LIST2 *commands = Banking::buildFetchCommands(*account, QDate(2026, 1, 1));

    QCOMPARE(Banking::outcomeOfSession(GWEN_SUCCESS, commands, testAccountId),
             FetchOutcome::Received);

    AB_TRANSACTION *balanceCommand = BankingHelpers::commandOfKind(commands,
                                                                   AB_Transaction_CommandGetBalance);
    QVERIFY(balanceCommand != nullptr);
    AB_Transaction_SetStatus(balanceCommand, AB_Transaction_StatusRejected);

    QCOMPARE(Banking::outcomeOfSession(GWEN_SUCCESS, commands, testAccountId), FetchOutcome::Failed);

    AB_Transaction_List2_freeAll(commands);
}

namespace {

/**
 * The orders of several accounts in one list, the way a fetch over all of them
 * sends them. The lists of the single accounts are released, their orders are
 * not: they travel into the list that is returned, and that one owns them.
 */
AB_TRANSACTION_LIST2 *commandsForAll(const QList<std::shared_ptr<Account>> &accounts,
                                     const QDate &latestStoredDate = {})
{
    AB_TRANSACTION_LIST2 *all = AB_Transaction_List2_new();

    for (const auto &account : accounts) {
        AB_TRANSACTION_LIST2 *commands = Banking::buildFetchCommands(*account, latestStoredDate);

        AB_TRANSACTION_LIST2_ITERATOR *iterator = AB_Transaction_List2_First(commands);
        if (iterator != nullptr) {
            AB_TRANSACTION *command = AB_Transaction_List2Iterator_Data(iterator);
            while (command != nullptr) {
                AB_Transaction_List2_PushBack(all, command);
                command = AB_Transaction_List2Iterator_Next(iterator);
            }
            AB_Transaction_List2Iterator_free(iterator);
        }

        AB_Transaction_List2_free(commands);
    }

    return all;
}

/** Puts the given outcome on every order of one account. */
void setStatusOfAccount(AB_TRANSACTION_LIST2 *commands,
                        quint32 uniqueAccountId,
                        AB_TRANSACTION_STATUS status)
{
    AB_TRANSACTION_LIST2_ITERATOR *iterator = AB_Transaction_List2_First(commands);
    if (iterator == nullptr) {
        return;
    }

    AB_TRANSACTION *command = AB_Transaction_List2Iterator_Data(iterator);
    while (command != nullptr) {
        if (AB_Transaction_GetUniqueAccountId(command) == uniqueAccountId) {
            AB_Transaction_SetStatus(command, status);
        }
        command = AB_Transaction_List2Iterator_Next(iterator);
    }

    AB_Transaction_List2Iterator_free(iterator);
}

/** The identifiers of the accounts the given records belong to. */
QSet<quint32> accountsOf(const BankingItems &items)
{
    QSet<quint32> accounts;

    for (const BankingItemPtr &item : items) {
        if (const auto transaction = std::dynamic_pointer_cast<Transaction>(item)) {
            accounts.insert(transaction->uniqueAccountId());
        } else if (const auto balance = std::dynamic_pointer_cast<Balance>(item)) {
            accounts.insert(balance->uniqueAccountId());
        }
    }

    return accounts;
}

constexpr quint32 secondAccountId = 4712;
constexpr quint32 thirdAccountId = 4713;

} // namespace

/**
 * The promise of a collective fetch: one account the bank refuses does not take
 * the others with it.
 *
 * It rests on a value that was read on 2026-08-18: _sendProviderQueues in
 * banking_online.c answers zero whatever a single institution did, and the
 * outcome of an account stands on its own orders. The evaluation is measured
 * here, on a container three accounts answered and one of them badly.
 */
void BankingFetchTest::aFailedAccountLeavesTheOthersOfTheSameListAlone()
{
    const QList<std::shared_ptr<Account>> accounts
        = {BankingHelpers::accountFromBackend(testAccountId),
           BankingHelpers::accountFromBackend(secondAccountId),
           BankingHelpers::accountFromBackend(thirdAccountId)};

    AB_TRANSACTION_LIST2 *commands = commandsForAll(accounts, QDate(2026, 1, 1));

    setStatusOfAccount(commands, secondAccountId, AB_Transaction_StatusRejected);

    AB_IMEXPORTER_CONTEXT *context = AB_ImExporterContext_new();
    for (const quint32 id : {testAccountId, secondAccountId, thirdAccountId}) {
        BankingHelpers::addAccountToContext(context,
                                            id,
                                            bookingCount,
                                            {{AB_Balance_TypeBooked, QDate(2026, 2, 1), 1234.56}});
    }

    const BankingItems items = Banking::itemsFromContext(context, commands);

    // The session itself answered success. Only the orders of the second account
    // carry the refusal, and that is what tells the three apart.
    QCOMPARE(Banking::outcomeOfSession(GWEN_SUCCESS, commands, testAccountId),
             FetchOutcome::Received);
    QCOMPARE(Banking::outcomeOfSession(GWEN_SUCCESS, commands, secondAccountId),
             FetchOutcome::Failed);
    QCOMPARE(Banking::outcomeOfSession(GWEN_SUCCESS, commands, thirdAccountId),
             FetchOutcome::Received);

    AB_ImExporterContext_free(context);
    AB_Transaction_List2_freeAll(commands);

    const QSet<quint32> delivered = accountsOf(items);

    QVERIFY(delivered.contains(testAccountId));
    QVERIFY(delivered.contains(thirdAccountId));
    QVERIFY(!delivered.contains(secondAccountId));

    QCOMPARE(BankingHelpers::itemsOfType(items, QStringLiteral("Transaction")).size(),
             bookingCount * 2);
}

/**
 * How far that promise reaches. The banking layer sorts the orders by
 * institution and runs one session per institution, so two accounts of the same
 * bank stand in the same session and what brings it down brings down both.
 *
 * The sorting itself needs a bank and is not run here; it is read in
 * banking_online.c and held in FR-033. What is measured is the evaluation that
 * has to follow from it: both accounts are named failed, not one of them.
 */
void BankingFetchTest::twoAccountsOfOneInstitutionFallTogetherWhenTheirSessionFails()
{
    const QList<std::shared_ptr<Account>> accounts
        = {BankingHelpers::accountFromBackend(testAccountId),
           BankingHelpers::accountFromBackend(secondAccountId),
           BankingHelpers::accountFromBackend(thirdAccountId)};

    AB_TRANSACTION_LIST2 *commands = commandsForAll(accounts, QDate(2026, 1, 1));

    // The two that share a session. A failing session marks every order it
    // carried, and both accounts hang on those orders.
    setStatusOfAccount(commands, testAccountId, AB_Transaction_StatusError);
    setStatusOfAccount(commands, secondAccountId, AB_Transaction_StatusError);

    AB_IMEXPORTER_CONTEXT *context = AB_ImExporterContext_new();
    for (const quint32 id : {testAccountId, secondAccountId, thirdAccountId}) {
        BankingHelpers::addAccountToContext(context, id, bookingCount, {});
    }

    const BankingItems items = Banking::itemsFromContext(context, commands);

    QCOMPARE(Banking::outcomeOfSession(GWEN_SUCCESS, commands, testAccountId), FetchOutcome::Failed);
    QCOMPARE(Banking::outcomeOfSession(GWEN_SUCCESS, commands, secondAccountId),
             FetchOutcome::Failed);
    QCOMPARE(Banking::outcomeOfSession(GWEN_SUCCESS, commands, thirdAccountId),
             FetchOutcome::Received);

    AB_ImExporterContext_free(context);
    AB_Transaction_List2_freeAll(commands);

    QCOMPARE(accountsOf(items), QSet<quint32>{thirdAccountId});
}

/**
 * An account without online access has to stay out of the list. The backend
 * sorts the queues by institution before it sends anything and refuses the whole
 * call over one account that carries none, so such an account in the list would
 * take every other one with it.
 */
void BankingFetchTest::aFetchForAllAccountsPassesOverEveryAccountWithoutOnlineAccess()
{
    Banking banking(applicationInfo());

    const ScopedConsoleGui gui;
    QVERIFY(!initialized(banking, gui.get()).isError());

    QSignalSpy skippedSpy(&banking, &Banking::accountSkipped);
    QSignalSpy errorSpy(&banking, &Banking::errorOccurred);
    QSignalSpy failedSpy(&banking, &Banking::accountFailed);
    QSignalSpy finishedSpy(&banking, &Banking::finished);

    const QList<std::shared_ptr<Account>> accounts
        = {BankingHelpers::accountFromBackend(testAccountId, ""),
           BankingHelpers::accountFromBackend(secondAccountId, ""),
           BankingHelpers::accountFromBackend(thirdAccountId, "")};

    banking.fetchAccounts(accounts, {});

    QVERIFY(finishedSpy.wait(sessionTimeoutMs));

    // Every one of them by its own identifier, and no session at all: with
    // nothing left to send, nothing is sent.
    QCOMPARE(skippedSpy.count(), 3);
    QCOMPARE(errorSpy.count(), 0);
    QCOMPARE(failedSpy.count(), 0);
    QCOMPARE(finishedSpy.count(), 1);

    QSet<quint32> reported;
    for (const auto &arguments : std::as_const(skippedSpy)) {
        reported.insert(arguments.at(0).value<quint32>());
        QVERIFY(!arguments.at(1).toString().isEmpty());
    }

    QCOMPARE(reported, (QSet<quint32>{testAccountId, secondAccountId, thirdAccountId}));

    banking.finalize();
}

} // namespace olbaflinx::core::banking::tests

QTEST_GUILESS_MAIN(olbaflinx::core::banking::tests::BankingFetchTest)

#include "tst_banking_fetch.moc"
