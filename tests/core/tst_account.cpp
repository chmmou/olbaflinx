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

#include "core/Banking/Account/Account.h"

#include "TestHelpers.h"

#include <aqbanking/account_type.h>

#include <QtTest/QtTest>

using namespace olbaflinx::core::banking::account;

namespace olbaflinx::core::banking::account::tests {

using namespace olbaflinx::core::tests;

/**
 * The instance owns a C structure and frees it in its destructor. A copy would
 * hand the same pointer to two destructors, and the compiler generated
 * operations were there for the taking until they were withdrawn. Balance and
 * ReferenceAccount withdrew theirs from the start.
 *
 * Held at compile time, because that is the only place it can be held: an
 * implementation that offers the operations again cannot be caught by anything
 * that runs.
 */
static_assert(!std::is_copy_constructible_v<Account>);
static_assert(!std::is_copy_assignable_v<Account>);
static_assert(!std::is_move_constructible_v<Account>);
static_assert(!std::is_move_assignable_v<Account>);

class AccountTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void fromMapReturnsAnEmptyPointerForAnEmptyMap();
    void fromMapCarriesEveryColumnOfTheRow();
    void fromMapKeepsIdsBeyondTheSignedRange();
    void fromMapKeepsNonAsciiNames();
    void fromMapCountsAnAccountWithoutAStateAsKept();
    void toMapAndBackCarriesTheState();
    void toMapAndBackYieldsTheSameAccount();
    void toMapAndBackKeepsTheReferenceAccounts();
    void toMapKeepsTheReferenceAccountsAfterTheAccountIsGone();
    void isValidRejectsTheInvalidType();
    void isValidAcceptsAnAccountTheLibraryDidNotSort();
    void isValidAcceptsAKnownType();
    void typeStringIsEmptyForATypeOutsideTheEnum();
    void toStringNamesTheAccountAndTheBank();
    void itemTypeIsTheNameOfTheClass();
};

/**
 * The failure case. Without a row there is nothing to build from, and the caller
 * has to be able to tell that apart from an account with default values.
 */
void AccountTest::fromMapReturnsAnEmptyPointerForAnEmptyMap()
{
    const auto account = Account::fromMap({});

    QVERIFY(account == nullptr);
}

void AccountTest::fromMapCarriesEveryColumnOfTheRow()
{
    const auto map = TestHelpers::createFakeAccountMap(AB_AccountType_Bank);
    const auto account = Account::fromMap(map);

    QVERIFY(account != nullptr);

    QCOMPARE(account->type(), map.value(QStringLiteral("type")).toInt());
    QCOMPARE(account->uniqueId(), map.value(QStringLiteral("unique_id")).toUInt());
    QCOMPARE(account->backendName(), map.value(QStringLiteral("backend_name")).toString());
    QCOMPARE(account->ownerName(), map.value(QStringLiteral("owner_name")).toString());
    QCOMPARE(account->accountName(), map.value(QStringLiteral("account_name")).toString());
    QCOMPARE(account->currency(), map.value(QStringLiteral("currency")).toString());
    QCOMPARE(account->iban(), map.value(QStringLiteral("iban")).toString());
    QCOMPARE(account->bic(), map.value(QStringLiteral("bic")).toString());
    QCOMPARE(account->bankCode(), map.value(QStringLiteral("bank_code")).toString());
    QCOMPARE(account->bankName(), map.value(QStringLiteral("bank_name")).toString());
    QCOMPARE(account->accountNumber(), map.value(QStringLiteral("account_number")).toString());
    QCOMPARE(account->balance(), map.value(QStringLiteral("balance")).toDouble());
}

/**
 * The key used to read "uniqueId" while toMap and the column both write
 * "unique_id", so every account read back carried an id of zero. The id is a
 * quint32 and has to survive beyond the range of a signed int.
 */
void AccountTest::fromMapKeepsIdsBeyondTheSignedRange()
{
    auto map = TestHelpers::createFakeAccountMap(AB_AccountType_Bank);
    map[QStringLiteral("unique_id")] = 3000000000u;

    const auto account = Account::fromMap(map);

    QVERIFY(account != nullptr);
    QCOMPARE(account->uniqueId(), 3000000000u);
}

void AccountTest::fromMapKeepsNonAsciiNames()
{
    auto map = TestHelpers::createFakeAccountMap(AB_AccountType_Bank);
    map[QStringLiteral("owner_name")] = QStringLiteral("Erika Müller-Groß");
    map[QStringLiteral("bank_name")] = QStringLiteral("Sparkasse Köln/Bonn");

    const auto account = Account::fromMap(map);

    QVERIFY(account != nullptr);
    QCOMPARE(account->ownerName(), QStringLiteral("Erika Müller-Groß"));
    QCOMPARE(account->bankName(), QStringLiteral("Sparkasse Köln/Bonn"));
}

/**
 * A row from a store written before the column existed carries no state, and
 * neither does an account the wizard has not decided about. Such an account is
 * kept, the same answer the default of the column gives.
 *
 * It does not hand that answer on, though: toMap leaves the property out, so
 * that writing this account cannot overwrite the state a stored one already has.
 */
void AccountTest::fromMapCountsAnAccountWithoutAStateAsKept()
{
    auto map = TestHelpers::createFakeAccountMap(AB_AccountType_Bank);
    QVERIFY(!map.contains(QStringLiteral("active")));

    const auto account = Account::fromMap(map);

    QVERIFY(account != nullptr);
    QVERIFY(account->isActive());
    QVERIFY(!account->toMap().contains(QStringLiteral("active")));
}

/**
 * The state travels the same way as the rest, through toMap into the row and
 * back out through fromMap. It is the one property the bank does not report,
 * which is why it is easy to lose on the way.
 */
void AccountTest::toMapAndBackCarriesTheState()
{
    const auto account = TestHelpers::createFakeAccount(AB_AccountType_Bank);
    QVERIFY(account != nullptr);
    QVERIFY(account->isActive());

    account->setActive(false);

    const auto map = account->toMap();
    QCOMPARE(map.value(QStringLiteral("active")).toBool(), false);

    const auto readBack = Account::fromMap(map);
    QVERIFY(readBack != nullptr);
    QVERIFY(!readBack->isActive());

    // The database answers with the integer the column holds, not with a bool.
    auto fromColumn = map;
    fromColumn[QStringLiteral("active")] = 1;

    const auto reactivated = Account::fromMap(fromColumn);
    QVERIFY(reactivated != nullptr);
    QVERIFY(reactivated->isActive());
}

/**
 * The round trip is what the storage relies on: an account is written through
 * toMap and read back through fromMap.
 */
void AccountTest::toMapAndBackYieldsTheSameAccount()
{
    const auto first = TestHelpers::createFakeAccount(AB_AccountType_Bank);
    QVERIFY(first != nullptr);

    const auto second = Account::fromMap(first->toMap());
    QVERIFY(second != nullptr);

    QCOMPARE(second->type(), first->type());
    QCOMPARE(second->uniqueId(), first->uniqueId());
    QCOMPARE(second->ownerName(), first->ownerName());
    QCOMPARE(second->accountName(), first->accountName());
    QCOMPARE(second->iban(), first->iban());
    QCOMPARE(second->bic(), first->bic());
    QCOMPARE(second->bankCode(), first->bankCode());
    QCOMPARE(second->bankName(), first->bankName());
    QCOMPARE(second->accountNumber(), first->accountNumber());
    QCOMPARE(second->balance(), first->balance());
    QCOMPARE(second->toString(), first->toString());
}

/**
 * referenceAccounts used to release the list the account spec holds, which left
 * the spec with a dangling list and its destructor walking into an assertion.
 */
void AccountTest::toMapAndBackKeepsTheReferenceAccounts()
{
    const auto account = Account::fromMap(TestHelpers::createFakeAccountMapWithReferenceAccount());
    QVERIFY(account != nullptr);

    const auto referenceAccounts = account->referenceAccounts();
    QCOMPARE(referenceAccounts.size(), 1);
    QCOMPARE(referenceAccounts.at(0)->iban(), QStringLiteral("DE02120300000000202051"));
    QCOMPARE(referenceAccounts.at(0)->ownerName(), QStringLiteral("Erika Müller-Groß"));

    // Asked a second time. The list is still there, which it would not be if the
    // first call had released it. No release here either: the entries are shared
    // and go when the last holder does.
    const auto again = account->referenceAccounts();
    QCOMPARE(again.size(), 1);
}

/**
 * The property map carries the reference accounts, and it outlives the account it
 * was taken from. Nothing in this function releases them.
 *
 * Against a list of raw pointers this leaked one wrapper per entry, because the
 * only holder was a QVariant and a QVariant deletes nothing. The leak is not
 * visible as a failed assertion; it needs a run under AddressSanitizer.
 */
void AccountTest::toMapKeepsTheReferenceAccountsAfterTheAccountIsGone()
{
    QMap<QString, QVariant> map;

    {
        const auto account = Account::fromMap(
            TestHelpers::createFakeAccountMapWithReferenceAccount());
        QVERIFY(account != nullptr);

        map = account->toMap();
    }

    QVERIFY(map.value(QStringLiteral("refAccounts")).canConvert<ReferenceAccounts>());

    const auto referenceAccounts = qvariant_cast<ReferenceAccounts>(
        map.value(QStringLiteral("refAccounts")));

    QCOMPARE(referenceAccounts.size(), 1);
    QCOMPARE(referenceAccounts.at(0)->iban(), QStringLiteral("DE02120300000000202051"));
}

void AccountTest::isValidRejectsTheInvalidType()
{
    const auto account = TestHelpers::createFakeAccount(AB_AccountType_Invalid);

    QVERIFY(account != nullptr);
    QVERIFY(!account->isValid());
}

/**
 * The kind the library writes wherever the institution names one it does not
 * sort into a kind of its own, and over the unknown kind as well before a
 * caller ever sees the record. Such an account is held at a real bank, and
 * dropping it would keep the user from their own bookings.
 */
void AccountTest::isValidAcceptsAnAccountTheLibraryDidNotSort()
{
    const auto account = TestHelpers::createFakeAccount(AB_AccountType_Unspecified);

    QVERIFY(account != nullptr);
    QCOMPARE(account->type(), static_cast<qint32>(AB_AccountType_Unspecified));
    QVERIFY(account->isValid());
}

void AccountTest::isValidAcceptsAKnownType()
{
    const auto account = TestHelpers::createFakeAccount(AB_AccountType_Checking);

    QVERIFY(account != nullptr);
    QCOMPARE(account->type(), static_cast<qint32>(AB_AccountType_Checking));
    QVERIFY(account->isValid());
}

/**
 * Every branch of the switch answers with a translated name. A value the enum
 * does not know falls through to an empty string, which is how a caller tells the
 * two apart.
 */
void AccountTest::typeStringIsEmptyForATypeOutsideTheEnum()
{
    const auto known = TestHelpers::createFakeAccount(AB_AccountType_Savings);
    QVERIFY(known != nullptr);
    QVERIFY(!known->typeString().isEmpty());

    const auto unknown = TestHelpers::createFakeAccount(9999);
    QVERIFY(unknown != nullptr);
    QVERIFY(unknown->typeString().isEmpty());
}

void AccountTest::toStringNamesTheAccountAndTheBank()
{
    const auto account = TestHelpers::createFakeAccount(AB_AccountType_Bank);
    QVERIFY(account != nullptr);

    const QString result = account->toString();

    QVERIFY(result.contains(account->accountNumber()));
    QVERIFY(result.contains(account->bankName()));
    QVERIFY(result.contains(account->ownerName()));
    QVERIFY(result.contains(account->accountName()));
}

void AccountTest::itemTypeIsTheNameOfTheClass()
{
    const auto account = TestHelpers::createFakeAccount();
    QVERIFY(account != nullptr);

    QCOMPARE(account->itemType(), QStringLiteral("Account"));
}

} // namespace olbaflinx::core::banking::account::tests

QTEST_APPLESS_MAIN(olbaflinx::core::banking::account::tests::AccountTest)

#include "tst_account.moc"
