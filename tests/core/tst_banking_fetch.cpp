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

#include <aqbanking/banking.h>
#include <aqbanking/types/balance.h>
#include <aqbanking/types/imexporter_context.h>
#include <aqbanking/types/value.h>

#include <gwenhywfar/gwendate.h>

#include <QtTest/QtTest>

#include <memory>

using namespace olbaflinx::core;
using namespace olbaflinx::core::banking;
using namespace olbaflinx::core::banking::account;
using namespace olbaflinx::core::banking::balance;
using namespace olbaflinx::core::banking::transaction;

namespace olbaflinx::core::banking::tests {

class BankingFetchTest final : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> bankingHome;

    static ApplicationInfo applicationInfo()
    {
        return {QStringLiteral("de.chm-projects.olbaflinx.test"),
                QStringLiteral("OlbaFlinxBankingFetchTest"),
                QStringLiteral("1.0.0")};
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
};

namespace {

constexpr quint32 testAccountId = 4711;
constexpr int bookingCount = 5;

/**
 * The non-interactive interface of gwenhywfar. Banking refuses to come up
 * without one, and this one answers no prompt and shows no dialog.
 *
 * Ownership stays here. Banking takes the pointer and never frees it.
 */
class ScopedConsoleGui
{
public:
    ScopedConsoleGui()
        : m_gui(GWEN_Gui_new())
    {}

    ~ScopedConsoleGui() { GWEN_Gui_free(m_gui); }

    ScopedConsoleGui(const ScopedConsoleGui &) = delete;
    ScopedConsoleGui &operator=(const ScopedConsoleGui &) = delete;

    [[nodiscard]] GWEN_GUI *get() const { return m_gui; }

private:
    GWEN_GUI *m_gui;
};

/**
 * An account the way AqBanking reports one. The backend name decides whether
 * the account has online access at all.
 */
std::shared_ptr<Account> makeAccount(quint32 uniqueId, const char *backendName)
{
    AB_ACCOUNT_SPEC *spec = AB_AccountSpec_new();

    AB_AccountSpec_SetUniqueId(spec, uniqueId);
    AB_AccountSpec_SetBackendName(spec, backendName);
    AB_AccountSpec_SetAccountName(spec, "Girokonto");
    AB_AccountSpec_SetOwnerName(spec, "Erika Mustermann");
    AB_AccountSpec_SetIban(spec, "DE02120300000000202051");
    AB_AccountSpec_SetBankCode(spec, "12030000");
    AB_AccountSpec_SetAccountNumber(spec, "0000202051");
    AB_AccountSpec_SetCurrency(spec, "EUR");

    auto account = std::make_shared<Account>(spec);
    AB_AccountSpec_free(spec);

    return account;
}

/** One balance of the response container. */
struct BalanceSpec
{
    AB_BALANCE_TYPE type;
    QDate date;
    double value;
};

/**
 * The container a session would have filled. Every test builds its own, which
 * is what makes the evaluation measurable without a bank.
 *
 * The caller owns the result and releases it with AB_ImExporterContext_free.
 */
AB_IMEXPORTER_CONTEXT *makeContext(quint32 uniqueId,
                                   int transactionCount,
                                   const QList<BalanceSpec> &balances)
{
    AB_IMEXPORTER_CONTEXT *context = AB_ImExporterContext_new();

    AB_IMEXPORTER_ACCOUNTINFO *info
        = AB_ImExporterContext_GetOrAddAccountInfo(context,
                                                   uniqueId,
                                                   "DE02120300000000202051",
                                                   "12030000",
                                                   "0000202051",
                                                   AB_AccountType_Checking);

    for (int index = 0; index < transactionCount; ++index) {
        AB_TRANSACTION *transaction = AB_Transaction_new();

        AB_Transaction_SetType(transaction, AB_Transaction_TypeStatement);
        AB_Transaction_SetUniqueAccountId(transaction, uniqueId);
        AB_Transaction_SetUniqueId(transaction, static_cast<uint32_t>(index) + 1);

        const auto purpose = QStringLiteral("Booking %1").arg(index + 1).toUtf8();
        AB_Transaction_SetPurpose(transaction, purpose.constData());

        AB_ImExporterAccountInfo_AddTransaction(info, transaction);
    }

    for (const BalanceSpec &spec : balances) {
        AB_BALANCE *balance = AB_Balance_new();

        AB_Balance_SetType(balance, spec.type);

        const auto text = spec.date.toString(QStringLiteral("yyyyMMdd")).toLatin1();
        GWEN_DATE *date = GWEN_Date_fromString(text.constData());
        AB_Balance_SetDate(balance, date);
        GWEN_Date_free(date);

        AB_VALUE *value = AB_Value_fromDouble(spec.value);
        AB_Value_SetCurrency(value, "EUR");
        AB_Balance_SetValue(balance, value);
        AB_Value_free(value);

        AB_ImExporterAccountInfo_AddBalance(info, balance);
    }

    return context;
}

/** The command of the given kind, or null if the list carries none. */
AB_TRANSACTION *commandOfKind(AB_TRANSACTION_LIST2 *commands, AB_TRANSACTION_COMMAND kind)
{
    AB_TRANSACTION_LIST2_ITERATOR *iterator = AB_Transaction_List2_First(commands);
    if (iterator == nullptr) {
        return nullptr;
    }

    AB_TRANSACTION *found = nullptr;

    AB_TRANSACTION *command = AB_Transaction_List2Iterator_Data(iterator);
    while (command != nullptr) {
        if (AB_Transaction_GetCommand(command) == kind) {
            found = command;
            break;
        }
        command = AB_Transaction_List2Iterator_Next(iterator);
    }

    AB_Transaction_List2Iterator_free(iterator);

    return found;
}

/** The items of the given type, in the order the core reported them. */
BankingItems itemsOfType(const BankingItems &items, const QString &type)
{
    BankingItems found;

    for (const BankingItemPtr &item : items) {
        if (item->itemType() == type) {
            found.append(item);
        }
    }

    return found;
}

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
    const auto account = makeAccount(testAccountId, "aqhbci");

    AB_TRANSACTION_LIST2 *commands = Banking::buildFetchCommands(*account, QDate(2026, 1, 1));
    QVERIFY(commands != nullptr);

    QCOMPARE(AB_Transaction_List2_GetSize(commands), 2u);

    AB_TRANSACTION *transactions = commandOfKind(commands, AB_Transaction_CommandGetTransactions);
    AB_TRANSACTION *balance = commandOfKind(commands, AB_Transaction_CommandGetBalance);

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
    const auto account = makeAccount(testAccountId, "aqhbci");

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
    const auto account = makeAccount(testAccountId, "aqhbci");

    AB_TRANSACTION_LIST2 *commands = Banking::buildFetchCommands(*account, QDate(2026, 1, 1));

    AB_IMEXPORTER_CONTEXT *context = makeContext(testAccountId,
                                                 bookingCount,
                                                 {{AB_Balance_TypeNoted, QDate(2026, 2, 2), 17.50},
                                                  {AB_Balance_TypeBooked,
                                                   QDate(2026, 2, 1),
                                                   1234.56}});

    const BankingItems items = Banking::itemsFromContext(context, commands);

    AB_ImExporterContext_free(context);
    AB_Transaction_List2_freeAll(commands);

    const BankingItems transactions = itemsOfType(items, QStringLiteral("Transaction"));
    const BankingItems balances = itemsOfType(items, QStringLiteral("Balance"));

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
    const auto account = makeAccount(testAccountId, "aqhbci");

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
    const auto account = makeAccount(testAccountId, "aqhbci");

    AB_TRANSACTION_LIST2 *commands = Banking::buildFetchCommands(*account, QDate(2026, 1, 1));

    AB_TRANSACTION *balanceCommand = commandOfKind(commands, AB_Transaction_CommandGetBalance);
    QVERIFY(balanceCommand != nullptr);
    AB_Transaction_SetStatus(balanceCommand, AB_Transaction_StatusError);

    AB_IMEXPORTER_CONTEXT *context = makeContext(testAccountId,
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
    const auto account = makeAccount(testAccountId, "aqhbci");

    AB_TRANSACTION_LIST2 *commands = Banking::buildFetchCommands(*account, QDate(2026, 1, 1));

    AB_TRANSACTION *balanceCommand = commandOfKind(commands, AB_Transaction_CommandGetBalance);
    QVERIFY(balanceCommand != nullptr);
    AB_Transaction_SetStatus(balanceCommand, AB_Transaction_StatusAccepted);

    AB_IMEXPORTER_CONTEXT *context = makeContext(testAccountId, bookingCount, {});

    const BankingItems items = Banking::itemsFromContext(context, commands);

    AB_ImExporterContext_free(context);
    AB_Transaction_List2_freeAll(commands);

    QCOMPARE(itemsOfType(items, QStringLiteral("Transaction")).size(), bookingCount);
    QVERIFY(itemsOfType(items, QStringLiteral("Balance")).isEmpty());
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

    const auto account = makeAccount(testAccountId, "");

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

} // namespace olbaflinx::core::banking::tests

QTEST_APPLESS_MAIN(olbaflinx::core::banking::tests::BankingFetchTest)

#include "tst_banking_fetch.moc"
