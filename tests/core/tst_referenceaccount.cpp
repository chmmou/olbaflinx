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

#include "core/Banking/Account/ReferenceAccount.h"

#include <QtTest/QtTest>

#include <memory>

using namespace olbaflinx::core::banking::account;

namespace olbaflinx::core::banking::account::tests {

class ReferenceAccountTest final : public QObject
{
    Q_OBJECT

private:
    /**
     * Builds a reference account with every field set. The constructor duplicates
     * what it is given, so the handle is released again right away.
     */
    static std::shared_ptr<ReferenceAccount> createFilledReferenceAccount()
    {
        auto abReferenceAccount = AB_ReferenceAccount_new();

        AB_ReferenceAccount_SetAccountType(abReferenceAccount, 1);
        AB_ReferenceAccount_SetOwnerName(abReferenceAccount, "Erika Müller-Groß");
        AB_ReferenceAccount_SetOwnerName2(abReferenceAccount, "Max Mustermann");
        AB_ReferenceAccount_SetAccountName(abReferenceAccount, "Girokonto");
        AB_ReferenceAccount_SetIban(abReferenceAccount, "DE02120300000000202051");
        AB_ReferenceAccount_SetBic(abReferenceAccount, "BYLADEM1001");
        AB_ReferenceAccount_SetCountry(abReferenceAccount, "de");
        AB_ReferenceAccount_SetBankCode(abReferenceAccount, "12030000");
        AB_ReferenceAccount_SetAccountNumber(abReferenceAccount, "0000202051");
        AB_ReferenceAccount_SetSubAccountNumber(abReferenceAccount, "01");

        auto referenceAccount = std::make_shared<ReferenceAccount>(abReferenceAccount);
        AB_ReferenceAccount_free(abReferenceAccount);

        return referenceAccount;
    }

private Q_SLOTS:
    void everyFieldSurvivesTheConstructor();
    void nonAsciiOwnerNameSurvivesTheConstructor();
    void isValidCannotRejectAnyAccountType();
    void isValidAcceptsTheDefaultConstructedAccount();
    void fromMapReturnsAnEmptyPointerForAnEmptyMap();
    void toMapCarriesEveryFieldOfTheAccount();
    void itemTypeIsTheNameOfTheClass();
};

void ReferenceAccountTest::everyFieldSurvivesTheConstructor()
{
    const auto referenceAccount = createFilledReferenceAccount();

    QCOMPARE(referenceAccount->accountType(), 1);
    QCOMPARE(referenceAccount->ownerName2(), QStringLiteral("Max Mustermann"));
    QCOMPARE(referenceAccount->accountName(), QStringLiteral("Girokonto"));
    QCOMPARE(referenceAccount->iban(), QStringLiteral("DE02120300000000202051"));
    QCOMPARE(referenceAccount->bic(), QStringLiteral("BYLADEM1001"));
    QCOMPARE(referenceAccount->country(), QStringLiteral("de"));
    QCOMPARE(referenceAccount->bankCode(), QStringLiteral("12030000"));
    QCOMPARE(referenceAccount->accountNumber(), QStringLiteral("0000202051"));
    QCOMPARE(referenceAccount->subAccountNumber(), QStringLiteral("01"));
}

void ReferenceAccountTest::nonAsciiOwnerNameSurvivesTheConstructor()
{
    const auto referenceAccount = createFilledReferenceAccount();

    QCOMPARE(referenceAccount->ownerName(), QStringLiteral("Erika Müller-Groß"));
}

/**
 * isValid compares the account type against -1, but AqBanking holds that type in
 * a uint8_t. The value never reaches the comparison: -1 arrives as 255. isValid
 * therefore answers true for every type it can be given, and the failure case it
 * was written for does not exist.
 *
 * This test states what the class does today, not what it should do. It fails as
 * soon as the comparison is corrected, which is the point at which it has to be
 * rewritten into the rejection case.
 */
void ReferenceAccountTest::isValidCannotRejectAnyAccountType()
{
    auto abReferenceAccount = AB_ReferenceAccount_new();
    AB_ReferenceAccount_SetAccountType(abReferenceAccount, static_cast<uint8_t>(-1));

    const ReferenceAccount referenceAccount(abReferenceAccount);
    AB_ReferenceAccount_free(abReferenceAccount);

    QCOMPARE(referenceAccount.accountType(), 255);
    QVERIFY(referenceAccount.isValid());
}

void ReferenceAccountTest::isValidAcceptsTheDefaultConstructedAccount()
{
    const ReferenceAccount referenceAccount;

    QVERIFY(referenceAccount.isValid());
}

void ReferenceAccountTest::fromMapReturnsAnEmptyPointerForAnEmptyMap()
{
    const auto referenceAccount = ReferenceAccount::fromMap({});

    QVERIFY(referenceAccount == nullptr);
}

void ReferenceAccountTest::toMapCarriesEveryFieldOfTheAccount()
{
    const auto referenceAccount = createFilledReferenceAccount();

    const auto map = referenceAccount->toMap();

    QCOMPARE(map.value(QStringLiteral("account_type")).toInt(), referenceAccount->accountType());
    QCOMPARE(map.value(QStringLiteral("owner_name")).toString(), referenceAccount->ownerName());
    QCOMPARE(map.value(QStringLiteral("owner_name2")).toString(), referenceAccount->ownerName2());
    QCOMPARE(map.value(QStringLiteral("account_name")).toString(), referenceAccount->accountName());
    QCOMPARE(map.value(QStringLiteral("iban")).toString(), referenceAccount->iban());
    QCOMPARE(map.value(QStringLiteral("bic")).toString(), referenceAccount->bic());
    QCOMPARE(map.value(QStringLiteral("country")).toString(), referenceAccount->country());
    QCOMPARE(map.value(QStringLiteral("bank_code")).toString(), referenceAccount->bankCode());
    QCOMPARE(map.value(QStringLiteral("account_number")).toString(),
             referenceAccount->accountNumber());
    QCOMPARE(map.value(QStringLiteral("sub_account_number")).toString(),
             referenceAccount->subAccountNumber());
}

void ReferenceAccountTest::itemTypeIsTheNameOfTheClass()
{
    const ReferenceAccount referenceAccount;

    QCOMPARE(referenceAccount.itemType(), QStringLiteral("ReferenceAccount"));
}

} // namespace olbaflinx::core::banking::account::tests

QTEST_APPLESS_MAIN(olbaflinx::core::banking::account::tests::ReferenceAccountTest)

#include "tst_referenceaccount.moc"
