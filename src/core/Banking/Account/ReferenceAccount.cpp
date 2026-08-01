/**
 * Copyright (C) 2022-2026, Alexander Saal <developer@olbaflinx.chm-projects.de>
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

#include <aqbanking/account_type.h>

#include <QtCore/QObject>

using namespace olbaflinx::core::banking::account;

class ReferenceAccount::Private
{
public:
    explicit Private(const AB_REFERENCE_ACCOUNT *refAccount)
        : abRefAccount(AB_ReferenceAccount_dup(refAccount))
    {}

    ~Private()
    {
        if (abRefAccount != nullptr) {
            AB_ReferenceAccount_free(abRefAccount);
        }
        abRefAccount = nullptr;
    }

    AB_REFERENCE_ACCOUNT *abRefAccount;
};

ReferenceAccount::ReferenceAccount(const AB_REFERENCE_ACCOUNT *refAccount)
    : d_ptr(new Private(refAccount ?: AB_ReferenceAccount_new()))
{}

ReferenceAccount::~ReferenceAccount()
{
    delete d_ptr;
}

qint32 ReferenceAccount::accountType() const
{
    return AB_ReferenceAccount_GetAccountType(d_ptr->abRefAccount);
}

QString ReferenceAccount::ownerName() const
{
    return QString::fromUtf8(AB_ReferenceAccount_GetOwnerName(d_ptr->abRefAccount));
}

QString ReferenceAccount::ownerName2() const
{
    return QString::fromUtf8(AB_ReferenceAccount_GetOwnerName2(d_ptr->abRefAccount));
}

QString ReferenceAccount::accountName() const
{
    return QString::fromUtf8(AB_ReferenceAccount_GetAccountName(d_ptr->abRefAccount));
}

QString ReferenceAccount::iban() const
{
    return QString::fromUtf8(AB_ReferenceAccount_GetIban(d_ptr->abRefAccount));
}

QString ReferenceAccount::bic() const
{
    return QString::fromUtf8(AB_ReferenceAccount_GetBic(d_ptr->abRefAccount));
}

QString ReferenceAccount::country() const
{
    return QString::fromUtf8(AB_ReferenceAccount_GetCountry(d_ptr->abRefAccount));
}

QString ReferenceAccount::bankCode() const
{
    return QString::fromUtf8(AB_ReferenceAccount_GetBankCode(d_ptr->abRefAccount));
}

QString ReferenceAccount::accountNumber() const
{
    return QString::fromUtf8(AB_ReferenceAccount_GetAccountNumber(d_ptr->abRefAccount));
}

QString ReferenceAccount::subAccountNumber() const
{
    return QString::fromUtf8(AB_ReferenceAccount_GetSubAccountNumber(d_ptr->abRefAccount));
}

std::shared_ptr<ReferenceAccount> ReferenceAccount::fromMap(const QMap<QString, QVariant> &map)
{
    return std::shared_ptr<ReferenceAccount>(create(map));
}

ReferenceAccount *ReferenceAccount::create(const QMap<QString, QVariant> &map)
{
    if (map.isEmpty()) {
        return nullptr;
    }

    // The values used to be dropped and an empty account handed back, so every
    // reference account read from the database was blank.
    AB_REFERENCE_ACCOUNT *abRefAccount = AB_ReferenceAccount_new();

    const auto ownerName = map.value(QStringLiteral("owner_name")).toString().toUtf8();
    const auto ownerName2 = map.value(QStringLiteral("owner_name2")).toString().toUtf8();
    const auto accountName = map.value(QStringLiteral("account_name")).toString().toUtf8();
    const auto iban = map.value(QStringLiteral("iban")).toString().toUtf8();
    const auto bic = map.value(QStringLiteral("bic")).toString().toUtf8();
    const auto country = map.value(QStringLiteral("country")).toString().toUtf8();
    const auto bankCode = map.value(QStringLiteral("bank_code")).toString().toUtf8();
    const auto accountNumber = map.value(QStringLiteral("account_number")).toString().toUtf8();
    const auto subAccountNumber = map.value(QStringLiteral("sub_account_number"))
                                      .toString()
                                      .toUtf8();

    AB_ReferenceAccount_SetAccountType(abRefAccount,
                                       static_cast<uint8_t>(
                                           map.value(QStringLiteral("account_type")).toUInt()));
    AB_ReferenceAccount_SetOwnerName(abRefAccount, ownerName.constData());
    AB_ReferenceAccount_SetOwnerName2(abRefAccount, ownerName2.constData());
    AB_ReferenceAccount_SetAccountName(abRefAccount, accountName.constData());
    AB_ReferenceAccount_SetIban(abRefAccount, iban.constData());
    AB_ReferenceAccount_SetBic(abRefAccount, bic.constData());
    AB_ReferenceAccount_SetCountry(abRefAccount, country.constData());
    AB_ReferenceAccount_SetBankCode(abRefAccount, bankCode.constData());
    AB_ReferenceAccount_SetAccountNumber(abRefAccount, accountNumber.constData());
    AB_ReferenceAccount_SetSubAccountNumber(abRefAccount, subAccountNumber.constData());

    // The constructor duplicates what it is handed, so the structure built here
    // is freed again right after.
    auto *referenceAccount = new ReferenceAccount(abRefAccount);

    AB_ReferenceAccount_free(abRefAccount);

    return referenceAccount;
}

bool ReferenceAccount::isValid() const
{
    // AqBanking keeps the type in a uint8_t, so the comparison against
    // AB_AccountType_Invalid, which is -1, could never be true. Unknown is the
    // value the library uses for a type it has not been told.
    return accountType() != AB_AccountType_Unknown;
}

QString ReferenceAccount::toString() const
{
    return QObject::tr("Reference account %1 [%2] - %3")
        .arg(accountNumber().isEmpty() ? iban() : accountNumber(), bankCode(), ownerName());
}

QMap<QString, QVariant> ReferenceAccount::toMap() const
{
    QMap<QString, QVariant> map = {};

    map[QStringLiteral("iban")] = iban();
    map[QStringLiteral("bic")] = bic();
    map[QStringLiteral("account_number")] = accountNumber();
    map[QStringLiteral("sub_account_number")] = subAccountNumber();
    map[QStringLiteral("country")] = country();
    map[QStringLiteral("bank_code")] = bankCode();
    map[QStringLiteral("owner_name")] = ownerName();
    map[QStringLiteral("owner_name2")] = ownerName2();
    map[QStringLiteral("account_name")] = accountName();
    map[QStringLiteral("account_type")] = accountType();

    return map;
}

QString ReferenceAccount::itemType() const
{
    return QStringLiteral("ReferenceAccount");
}
