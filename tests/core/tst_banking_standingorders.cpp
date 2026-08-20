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
#include "core/Banking/StandingOrder/StandingOrder.h"
#include "core/Error.h"

#include "BankingHelpers.h"
#include "StandingOrderHelpers.h"
#include "TestHelpers.h"

#include <aqbanking/banking.h>

#include <gwenhywfar/error.h>

#include <QtTest/QtTest>

#include <QtCore/QElapsedTimer>
#include <QtCore/QList>
#include <QtCore/QLoggingCategory>
#include <QtCore/QStringList>

#include <memory>

using namespace olbaflinx::core;
using namespace olbaflinx::core::banking;
using namespace olbaflinx::core::banking::account;
using namespace olbaflinx::core::banking::standingorder;

namespace olbaflinx::core::banking::tests {

using namespace olbaflinx::core::tests;

class BankingStandingOrdersTest final : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> bankingHome;

    static ApplicationInfo applicationInfo()
    {
        return TestHelpers::applicationInfo(QStringLiteral("OlbaFlinxStandingOrderFetchTest"));
    }

    /** The backend, brought up against the given interface. */
    static Error initialized(Banking &banking, GWEN_GUI *gui)
    {
        return banking.initialize(QStringLiteral("OlbaFlinxStandingOrderFetchTest"),
                                  QStringLiteral("1.0.0"),
                                  QStringLiteral("0123456789ABCDEF"),
                                  gui);
    }

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();

    void theOrderCarriesTheStandingOrderCommandAndTheIdOfTheGivenAccount();
    void theOrderCarriesNoStartingPoint();
    void aDescriptionThatNamesOtherOrdersAndNotThisOneBuildsNothing();
    void aDescriptionThatNamesNoOrderBuildsTheOrder();

    void fiveStandingOrdersArriveAsFiveRecordsUnderTheirAccount();
    void aBookingInTheSameContainerDoesNotComeAlong();
    void anEmptyContainerAnswersWithAnEmptyResult();
    void aStandingOrderWithoutAnAccountIdIsDroppedAndLogged();
    void noLogEntryCarriesTheDetailsOfAStandingOrder();

    void aRefusedOrderIsAFailureOfItsAccountAndNoEmptyResult();

    void anAccountWithoutOnlineAccessIsSkippedWithAReason();
    void theCallReturnsWhileTheSessionIsStillRunning();
    void aSecondStandingOrderFetchIsRefusedWhileOneRuns();
    void aStandingOrderFetchIsRefusedWhileATransactionFetchRuns();
    void aFetchForAllAccountsPassesOverEveryAccountWithoutOnlineAccess();
};

namespace {

constexpr quint32 testAccountId = 4711;
constexpr int orderCount = 5;

/**
 * How long a spy waits for something a session has to produce first. The call
 * returns the moment the signal arrives, so nothing sleeps for it.
 */
constexpr int sessionTimeoutMs = 30000;

/**
 * What the call to start a fetch may take before it is back.
 *
 * It copies an account and starts a run; everything beyond that is a lock taken
 * in the calling thread, which is what this number is here to catch. The value
 * is set rather than measured.
 */
constexpr qint64 immediateReturnMs = 100;

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

/**
 * Collects what the application writes to the log while it stands.
 *
 * Only one may be up at a time: the handler of Qt is global and carries no
 * place to hang an instance on.
 */
class ScopedLogCollector
{
public:
    ScopedLogCollector()
        : m_previous(qInstallMessageHandler(&ScopedLogCollector::collect))
    {
        s_messages.clear();
        QLoggingCategory::setFilterRules(QStringLiteral("*.debug=true\n*.info=true"));
    }

    ~ScopedLogCollector()
    {
        QLoggingCategory::setFilterRules({});
        qInstallMessageHandler(m_previous);
    }

    ScopedLogCollector(const ScopedLogCollector &) = delete;
    ScopedLogCollector &operator=(const ScopedLogCollector &) = delete;
    ScopedLogCollector(ScopedLogCollector &&) = delete;
    ScopedLogCollector &operator=(ScopedLogCollector &&) = delete;

    [[nodiscard]] static QStringList messages() { return s_messages; }

private:
    static void collect(QtMsgType, const QMessageLogContext &, const QString &message)
    {
        s_messages << message;
    }

    inline static QStringList s_messages;

    QtMessageHandler m_previous;
};

} // namespace

/**
 * AqBanking keeps its configuration below AQBANKING_HOME. Without pointing that
 * at a directory of our own, every run would write into the configuration of
 * whoever started it.
 */
void BankingStandingOrdersTest::initTestCase()
{
    bankingHome = std::make_unique<QTemporaryDir>();
    QVERIFY(bankingHome->isValid());

    QVERIFY(qputenv("AQBANKING_HOME", bankingHome->path().toUtf8()));
}

void BankingStandingOrdersTest::cleanupTestCase()
{
    qunsetenv("AQBANKING_HOME");
    bankingHome.reset();
}

/**
 * The order has to be readable before it is sent, which is what keeps this
 * measurable without a bank.
 */
void BankingStandingOrdersTest::theOrderCarriesTheStandingOrderCommandAndTheIdOfTheGivenAccount()
{
    const auto account = BankingHelpers::accountFromBackend(testAccountId);

    AB_TRANSACTION_LIST2 *commands = Banking::buildStandingOrderCommands(*account);
    QVERIFY(commands != nullptr);

    QCOMPARE(AB_Transaction_List2_GetSize(commands), 1u);

    AB_TRANSACTION *order
        = BankingHelpers::commandOfKind(commands, AB_Transaction_CommandSepaGetStandingOrders);
    QVERIFY(order != nullptr);

    QCOMPARE(AB_Transaction_GetUniqueAccountId(order), testAccountId);

    AB_Transaction_List2_freeAll(commands);
}

/**
 * The order asks for the holding, not for a span. FirstDate would be read by
 * the FinTS backend as the day a fetch starts at, and there is no such day here.
 */
void BankingStandingOrdersTest::theOrderCarriesNoStartingPoint()
{
    const auto account = BankingHelpers::accountFromBackend(testAccountId);

    AB_TRANSACTION_LIST2 *commands = Banking::buildStandingOrderCommands(*account);
    QVERIFY(commands != nullptr);

    AB_TRANSACTION *order
        = BankingHelpers::commandOfKind(commands, AB_Transaction_CommandSepaGetStandingOrders);
    QVERIFY(order != nullptr);

    QVERIFY(AB_Transaction_GetFirstDate(order) == nullptr);
    QVERIFY(AB_Transaction_GetLastDate(order) == nullptr);

    AB_Transaction_List2_freeAll(commands);
}

void BankingStandingOrdersTest::aDescriptionThatNamesOtherOrdersAndNotThisOneBuildsNothing()
{
    const auto account = BankingHelpers::accountFromBackend(testAccountId);

    AB_ACCOUNT_SPEC *offered = descriptionOffering(
        {AB_Transaction_CommandGetTransactions, AB_Transaction_CommandGetBalance});

    QVERIFY(!Banking::accountOffers(offered, AB_Transaction_CommandSepaGetStandingOrders));

    AB_TRANSACTION_LIST2 *commands = Banking::buildStandingOrderCommands(*account, offered);
    QVERIFY(commands != nullptr);

    QCOMPARE(AB_Transaction_List2_GetSize(commands), 0u);

    AB_Transaction_List2_freeAll(commands);
    AB_AccountSpec_free(offered);
}

/**
 * A description that names no order at all is no statement. The one a stored
 * account carries is such a description, and reading its silence as a refusal
 * would leave every fetch empty.
 */
void BankingStandingOrdersTest::aDescriptionThatNamesNoOrderBuildsTheOrder()
{
    const auto account = BankingHelpers::accountFromBackend(testAccountId);

    QVERIFY(Banking::accountOffers(nullptr, AB_Transaction_CommandSepaGetStandingOrders));

    AB_ACCOUNT_SPEC *silent = descriptionOffering({});
    QVERIFY(Banking::accountOffers(silent, AB_Transaction_CommandSepaGetStandingOrders));

    AB_TRANSACTION_LIST2 *commands = Banking::buildStandingOrderCommands(*account, silent);
    QVERIFY(commands != nullptr);

    QCOMPARE(AB_Transaction_List2_GetSize(commands), 1u);

    AB_Transaction_List2_freeAll(commands);
    AB_AccountSpec_free(silent);
}

void BankingStandingOrdersTest::fiveStandingOrdersArriveAsFiveRecordsUnderTheirAccount()
{
    const auto account = BankingHelpers::accountFromBackend(testAccountId);

    AB_TRANSACTION_LIST2 *commands = Banking::buildStandingOrderCommands(*account);

    AB_IMEXPORTER_CONTEXT *context = AB_ImExporterContext_new();
    StandingOrderHelpers::addStandingOrdersToContext(context, testAccountId, orderCount);

    const BankingItems items = Banking::standingOrdersFromContext(context, commands);

    QCOMPARE(items.size(), orderCount);

    for (const BankingItemPtr &item : items) {
        QCOMPARE(item->itemType(), QStringLiteral("StandingOrder"));

        const auto order = std::static_pointer_cast<StandingOrder>(item);
        QCOMPARE(order->uniqueAccountId(), testAccountId);
    }

    AB_ImExporterContext_free(context);
    AB_Transaction_List2_freeAll(commands);
}

/**
 * The type is what tells the two apart, and nothing else does. Both travel in
 * the same list of the same entry.
 */
void BankingStandingOrdersTest::aBookingInTheSameContainerDoesNotComeAlong()
{
    const auto account = BankingHelpers::accountFromBackend(testAccountId);

    AB_TRANSACTION_LIST2 *commands = Banking::buildStandingOrderCommands(*account);

    AB_IMEXPORTER_CONTEXT *context = BankingHelpers::responseContext(testAccountId, 3, {});
    StandingOrderHelpers::addStandingOrdersToContext(context, testAccountId, orderCount);

    const BankingItems items = Banking::standingOrdersFromContext(context, commands);

    QCOMPARE(items.size(), orderCount);
    QCOMPARE(BankingHelpers::itemsOfType(items, QStringLiteral("Transaction")).size(), 0);

    AB_ImExporterContext_free(context);
    AB_Transaction_List2_freeAll(commands);
}

void BankingStandingOrdersTest::anEmptyContainerAnswersWithAnEmptyResult()
{
    const auto account = BankingHelpers::accountFromBackend(testAccountId);

    AB_TRANSACTION_LIST2 *commands = Banking::buildStandingOrderCommands(*account);
    AB_IMEXPORTER_CONTEXT *context = AB_ImExporterContext_new();

    const BankingItems items = Banking::standingOrdersFromContext(context, commands);

    QVERIFY(items.isEmpty());

    AB_ImExporterContext_free(context);
    AB_Transaction_List2_freeAll(commands);
}

/**
 * An order names no account of its own; only the entry it sits in does. Where
 * the entry carries none either, there is nothing to store the order against.
 */
void BankingStandingOrdersTest::aStandingOrderWithoutAnAccountIdIsDroppedAndLogged()
{
    const auto account = BankingHelpers::accountFromBackend(testAccountId);

    AB_TRANSACTION_LIST2 *commands = Banking::buildStandingOrderCommands(*account);

    AB_IMEXPORTER_CONTEXT *context = AB_ImExporterContext_new();
    StandingOrderHelpers::addStandingOrdersToContext(context, 0, 1);

    const ScopedLogCollector log;

    const BankingItems items = Banking::standingOrdersFromContext(context, commands);

    QVERIFY(items.isEmpty());
    QVERIFY(!ScopedLogCollector::messages().filter(QStringLiteral("account id")).isEmpty());

    AB_ImExporterContext_free(context);
    AB_Transaction_List2_freeAll(commands);
}

/**
 * A standing order carries a payee, an account of theirs, a purpose and an
 * amount. None of them belongs in a log.
 */
void BankingStandingOrdersTest::noLogEntryCarriesTheDetailsOfAStandingOrder()
{
    const auto account = BankingHelpers::accountFromBackend(testAccountId);

    AB_TRANSACTION_LIST2 *commands = Banking::buildStandingOrderCommands(*account);

    AB_IMEXPORTER_CONTEXT *context = AB_ImExporterContext_new();
    StandingOrderHelpers::addStandingOrdersToContext(context, testAccountId, orderCount);
    StandingOrderHelpers::addStandingOrdersToContext(context, 0, 1);

    const ScopedLogCollector log;

    const BankingItems items = Banking::standingOrdersFromContext(context, commands);
    QCOMPARE(items.size(), orderCount);

    const auto spec = StandingOrderSpec{};

    for (const QString &message : ScopedLogCollector::messages()) {
        QVERIFY(!message.contains(QStringLiteral("Partner")));
        QVERIFY(!message.contains(QStringLiteral("Order ")));
        QVERIFY(!message.contains(spec.remoteIban));
        QVERIFY(!message.contains(spec.remoteName));
        QVERIFY(!message.contains(spec.localIban));
    }

    AB_ImExporterContext_free(context);
    AB_Transaction_List2_freeAll(commands);
}

/**
 * A session that comes back successful can still carry an order the bank
 * refused, and that account is a failure rather than an account without orders.
 */
void BankingStandingOrdersTest::aRefusedOrderIsAFailureOfItsAccountAndNoEmptyResult()
{
    const auto account = BankingHelpers::accountFromBackend(testAccountId);

    AB_TRANSACTION_LIST2 *commands = Banking::buildStandingOrderCommands(*account);

    QCOMPARE(Banking::outcomeOfSession(GWEN_SUCCESS, commands, testAccountId),
             FetchOutcome::Received);

    AB_TRANSACTION *order
        = BankingHelpers::commandOfKind(commands, AB_Transaction_CommandSepaGetStandingOrders);
    QVERIFY(order != nullptr);
    AB_Transaction_SetStatus(order, AB_Transaction_StatusRejected);

    QCOMPARE(Banking::outcomeOfSession(GWEN_SUCCESS, commands, testAccountId), FetchOutcome::Failed);

    // Nothing of the refused account comes through, so an empty result and a
    // refusal cannot be told apart by the item list alone.
    AB_IMEXPORTER_CONTEXT *context = AB_ImExporterContext_new();
    StandingOrderHelpers::addStandingOrdersToContext(context, testAccountId, orderCount);

    QVERIFY(Banking::standingOrdersFromContext(context, commands).isEmpty());

    AB_ImExporterContext_free(context);
    AB_Transaction_List2_freeAll(commands);
}

void BankingStandingOrdersTest::anAccountWithoutOnlineAccessIsSkippedWithAReason()
{
    Banking banking(applicationInfo());

    const ScopedConsoleGui gui;
    QVERIFY(!initialized(banking, gui.get()).isError());

    QSignalSpy skippedSpy(&banking, &Banking::accountSkipped);
    QSignalSpy notOfferedSpy(&banking, &Banking::standingOrdersNotOffered);
    QSignalSpy finishedSpy(&banking, &Banking::finished);
    QSignalSpy errorSpy(&banking, &Banking::errorOccurred);

    const auto account = BankingHelpers::accountFromBackend(testAccountId, "");

    banking.fetchStandingOrders(*account);

    QVERIFY(finishedSpy.wait(sessionTimeoutMs));

    QCOMPARE(skippedSpy.count(), 1);
    QCOMPARE(skippedSpy.first().at(0).toUInt(), testAccountId);
    QVERIFY(!skippedSpy.first().at(1).toString().isEmpty());
    QCOMPARE(errorSpy.count(), 0);

    // The two are apart. An account without online access is not an account the
    // bank holds no such request for, and one signal for both would tell the
    // user of either that the account cannot go online.
    QCOMPARE(notOfferedSpy.count(), 0);

    banking.finalize();
}

/**
 * Two measures, because either one alone lets a defect through: a call that
 * blocked and then started the session would still be back while the session
 * runs, and a call that was quick but ran the session in this thread would
 * still be under the bound.
 */
void BankingStandingOrdersTest::theCallReturnsWhileTheSessionIsStillRunning()
{
    Banking banking(applicationInfo());

    const ScopedHoldingGui gui;
    QVERIFY(!initialized(banking, gui.get()).isError());

    QSignalSpy finishedSpy(&banking, &Banking::finished);

    const auto account = BankingHelpers::accountFromBackend(testAccountId);

    QElapsedTimer timer;
    timer.start();

    banking.fetchStandingOrders(*account);

    const qint64 elapsed = timer.elapsed();

    QCOMPARE(finishedSpy.count(), 0);
    QVERIFY2(elapsed < immediateReturnMs,
             qPrintable(QStringLiteral("the call took %1 ms").arg(elapsed)));

    QTRY_COMPARE_WITH_TIMEOUT(ScopedHoldingGui::progressCount(), 1, sessionTimeoutMs);

    ScopedHoldingGui::release();
    QVERIFY(finishedSpy.wait(sessionTimeoutMs));

    banking.finalize();
}

void BankingStandingOrdersTest::aSecondStandingOrderFetchIsRefusedWhileOneRuns()
{
    Banking banking(applicationInfo());

    const ScopedHoldingGui gui;
    QVERIFY(!initialized(banking, gui.get()).isError());

    QSignalSpy errorSpy(&banking, &Banking::errorOccurred);
    QSignalSpy finishedSpy(&banking, &Banking::finished);

    const auto account = BankingHelpers::accountFromBackend(testAccountId);

    banking.fetchStandingOrders(*account);

    QTRY_COMPARE_WITH_TIMEOUT(ScopedHoldingGui::progressCount(), 1, sessionTimeoutMs);

    banking.fetchStandingOrders(*account);

    QVERIFY(errorSpy.wait(sessionTimeoutMs));
    QCOMPARE(errorSpy.count(), 1);
    QCOMPARE(errorSpy.first().at(0).value<ErrorCode>(), ErrorCode::InvalidInput);
    QCOMPARE(finishedSpy.count(), 0);

    QCOMPARE(ScopedHoldingGui::progressCount(), 1);

    ScopedHoldingGui::release();
    QVERIFY(finishedSpy.wait(sessionTimeoutMs));

    banking.finalize();
}

/**
 * The refusal reaches across the two kinds of fetch. The instance belongs to
 * one thread at a time, and that is a property of the instance rather than of
 * the order it is sending.
 */
void BankingStandingOrdersTest::aStandingOrderFetchIsRefusedWhileATransactionFetchRuns()
{
    Banking banking(applicationInfo());

    const ScopedHoldingGui gui;
    QVERIFY(!initialized(banking, gui.get()).isError());

    QSignalSpy errorSpy(&banking, &Banking::errorOccurred);
    QSignalSpy finishedSpy(&banking, &Banking::finished);

    const auto account = BankingHelpers::accountFromBackend(testAccountId);

    banking.fetchAccount(*account, QDate(2026, 1, 1));

    QTRY_COMPARE_WITH_TIMEOUT(ScopedHoldingGui::progressCount(), 1, sessionTimeoutMs);

    banking.fetchStandingOrders(*account);

    QVERIFY(errorSpy.wait(sessionTimeoutMs));
    QCOMPARE(errorSpy.count(), 1);
    QCOMPARE(finishedSpy.count(), 0);

    QCOMPARE(ScopedHoldingGui::progressCount(), 1);

    ScopedHoldingGui::release();
    QVERIFY(finishedSpy.wait(sessionTimeoutMs));

    banking.finalize();
}

/**
 * The backend refuses a whole run over one account without a backend of its
 * own, so such an account is kept out of the list rather than sent along.
 */
void BankingStandingOrdersTest::aFetchForAllAccountsPassesOverEveryAccountWithoutOnlineAccess()
{
    Banking banking(applicationInfo());

    const ScopedConsoleGui gui;
    QVERIFY(!initialized(banking, gui.get()).isError());

    QSignalSpy skippedSpy(&banking, &Banking::accountSkipped);
    QSignalSpy finishedSpy(&banking, &Banking::finished);
    QSignalSpy errorSpy(&banking, &Banking::errorOccurred);

    const QList<std::shared_ptr<Account>> accounts{
        BankingHelpers::accountFromBackend(testAccountId, ""),
        BankingHelpers::accountFromBackend(testAccountId + 1, ""),
    };

    banking.fetchStandingOrdersForAll(accounts);

    QVERIFY(finishedSpy.wait(sessionTimeoutMs));

    QCOMPARE(skippedSpy.count(), 2);
    QCOMPARE(errorSpy.count(), 0);

    banking.finalize();
}

} // namespace olbaflinx::core::banking::tests

QTEST_MAIN(olbaflinx::core::banking::tests::BankingStandingOrdersTest)

#include "tst_banking_standingorders.moc"
