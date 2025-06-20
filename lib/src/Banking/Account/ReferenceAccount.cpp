/**
 * Copyright (C) 2022-2025, Alexander Saal <developer@olbaflinx.chm-projects.de>
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

#include "ReferenceAccount.h"

using namespace olbaflinx::core::banking::account;

class ReferenceAccount::Private
{
public:
    explicit Private(const AB_REFERENCE_ACCOUNT *refAccount)
        : abRefAccount(AB_ReferenceAccount_dup(refAccount))
    { }

    ~Private()
    {
        if (abRefAccount != Q_NULLPTR) {
            AB_ReferenceAccount_free(abRefAccount);
        }
        abRefAccount = Q_NULLPTR;
    }

    AB_REFERENCE_ACCOUNT *abRefAccount;
};

ReferenceAccount::ReferenceAccount(const AB_REFERENCE_ACCOUNT *refAccount)
    : d_ptr(new Private(refAccount ?: AB_ReferenceAccount_new()))
{ }

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

BankingItem *ReferenceAccount::create(QMap<QString, QVariant> &map) const
{
    if (map.isEmpty()) {
        return Q_NULLPTR;
    }

    return new ReferenceAccount();
}

bool ReferenceAccount::isValid() const
{
    return accountType() != -1;
}

QString ReferenceAccount::toString() const
{
    return {};
}

QMap<QString, QVariant> ReferenceAccount::toMap() const
{
    QMap<QString, QVariant> map = {};

    map[":iban"] = iban();
    map[":bic"] = bic();
    map[":account_number"] = accountNumber();
    map[":sub_account_number"] = subAccountNumber();
    map[":country"] = country();
    map[":bank_code"] = bankCode();
    map[":owner_name"] = ownerName();
    map[":owner_name2"] = ownerName2();
    map[":account_name"] = accountName();
    map[":account_type"] = accountType();

    return map;
}

QString ReferenceAccount::itemType() const
{
    return {"ReferenceAccount"};
}
