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

#include "core/Banking/Account/Account.h"

#include <aqbanking/account_type.h>

#include <QtCore/QObject>

#include <optional>

using namespace olbaflinx::core::banking::account;

class Account::Private
{
public:
    // A duplicate is made only of what the caller handed in. The fallback used to
    // build a spec and duplicate that one as well, so the structure it had just
    // created was never released.
    explicit Private(const AB_ACCOUNT_SPEC *accountSpec, double balance)
        : abBalance(balance)
        , abAccountSpec(accountSpec ? AB_AccountSpec_dup(accountSpec) : AB_AccountSpec_new())
    {}

    ~Private()
    {
        if (abAccountSpec != nullptr) {
            AB_AccountSpec_free(abAccountSpec);
        }
        abAccountSpec = nullptr;
    }

    double abBalance;
    AB_ACCOUNT_SPEC *abAccountSpec;

    // Not part of the account spec. The bank does not report it, the user
    // decides it in the wizard, and an account nobody has decided about counts
    // as kept.
    //
    // Empty is not the same as true. It says that this account carries no
    // statement about its state, and toMap leaves the property out for it, so
    // that writing such an account cannot overwrite the state a stored one
    // already has. That is what keeps a deselected account out of sight when
    // the wizard merely offers it again.
    std::optional<bool> active;
};

Account::Account(const AB_ACCOUNT_SPEC *accountSpec, double balance)
    : BankingItem()
    , d_ptr(new Private(accountSpec, balance))
{}

Account::~Account()
{
    delete d_ptr;
}

qint32 Account::type() const
{
    return AB_AccountSpec_GetType(d_ptr->abAccountSpec);
}

QString Account::typeString() const
{
    QString typeString = {};

    switch (type()) {
    case AB_AccountType_Invalid:
        typeString = QObject::tr("Invalid");
        break;
    case AB_AccountType_Unknown:
        typeString = QObject::tr("Unknown");
        break;
    case AB_AccountType_Bank:
        typeString = QObject::tr("Bank");
        break;
    case AB_AccountType_CreditCard:
        typeString = QObject::tr("Credit Card"); // Kreditkarte (Kreditkarten Konto?)
        break;
    case AB_AccountType_Checking:
        typeString = QObject::tr("Checking"); // Gehaltskonto?
        break;
    case AB_AccountType_Savings:
        typeString = QObject::tr("Savings"); // Sparkonto
        break;
    case AB_AccountType_Investment:
        typeString = QObject::tr("Investment"); // Anlagen
        break;
    case AB_AccountType_Cash:
        typeString = QObject::tr("Cash"); // Bargeld
        break;
    case AB_AccountType_MoneyMarket:
        typeString = QObject::tr("Money Market"); // Kapitalmarkt?
        break;
    case AB_AccountType_Credit:
        typeString = QObject::tr("Credit"); // Guthaben Konto?
        break;
    case AB_AccountType_Unspecified:
        typeString = QObject::tr("Unspecified");
        break;
    default:
        typeString.clear();
    }

    return typeString;
}

quint32 Account::uniqueId() const
{
    return AB_AccountSpec_GetUniqueId(d_ptr->abAccountSpec);
}

QString Account::backendName() const
{
    return QString::fromUtf8(AB_AccountSpec_GetBackendName(d_ptr->abAccountSpec));
}

QString Account::ownerName() const
{
    return QString::fromUtf8(AB_AccountSpec_GetOwnerName(d_ptr->abAccountSpec));
}

QString Account::accountName() const
{
    return QString::fromUtf8(AB_AccountSpec_GetAccountName(d_ptr->abAccountSpec));
}

QString Account::currency() const
{
    return QString::fromUtf8(AB_AccountSpec_GetCurrency(d_ptr->abAccountSpec));
}

QString Account::memo() const
{
    return QString::fromUtf8(AB_AccountSpec_GetMemo(d_ptr->abAccountSpec));
}

QString Account::iban() const
{
    return QString::fromUtf8(AB_AccountSpec_GetIban(d_ptr->abAccountSpec));
}

QString Account::bic() const
{
    return QString::fromUtf8(AB_AccountSpec_GetBic(d_ptr->abAccountSpec));
}

QString Account::country() const
{
    return QString::fromUtf8(AB_AccountSpec_GetCountry(d_ptr->abAccountSpec));
}

QString Account::bankCode() const
{
    return QString::fromUtf8(AB_AccountSpec_GetBankCode(d_ptr->abAccountSpec));
}

QString Account::bankName() const
{
    return QString::fromUtf8(AB_AccountSpec_GetBankName(d_ptr->abAccountSpec));
}

QString Account::branchId() const
{
    return QString::fromUtf8(AB_AccountSpec_GetBranchId(d_ptr->abAccountSpec));
}

QString Account::accountNumber() const
{
    return QString::fromUtf8(AB_AccountSpec_GetAccountNumber(d_ptr->abAccountSpec));
}

QString Account::subAccountNumber() const
{
    return QString::fromUtf8(AB_AccountSpec_GetSubAccountNumber(d_ptr->abAccountSpec));
}

double Account::balance() const
{
    return d_ptr->abBalance;
}

bool Account::isActive() const
{
    return d_ptr->active.value_or(true);
}

void Account::setActive(const bool active)
{
    d_ptr->active = active;
}

TransactionLimitsList *Account::transactionLimits() const
{
    return AB_AccountSpec_GetTransactionLimitsList(d_ptr->abAccountSpec);
}

ReferenceAccounts Account::referenceAccounts() const
{
    // The getter hands out the list the account spec holds, not a copy of it.
    // Releasing it here left the spec with a dangling list, and its destructor
    // then walked into an assertion inside gwenhywfar. Ownership of the list
    // stays with the spec; the wrappers below are shared with the caller, and
    // each of them duplicates the entry it was built from.
    const auto refAccList = AB_AccountSpec_GetRefAccountList(d_ptr->abAccountSpec);
    if (refAccList == nullptr || AB_ReferenceAccount_List_GetCount(refAccList) == 0) {
        return {};
    }

    auto refAccounts = ReferenceAccounts();
    auto refAcc = AB_ReferenceAccount_List_First(refAccList);

    while (refAcc) {
        refAccounts.append(std::make_shared<ReferenceAccount>(refAcc));
        refAcc = AB_ReferenceAccount_List_Next(refAcc);
    }

    return refAccounts;
}

TransactionLimits *Account::transactionLimitsForCommand(const TransactionCommand &cmd) const
{
    return AB_AccountSpec_GetTransactionLimitsForCommand(d_ptr->abAccountSpec, cmd);
}

std::shared_ptr<Account> Account::fromMap(const QMap<QString, QVariant> &map)
{
    if (map.isEmpty()) {
        return {};
    }

    auto accountSpec = AB_AccountSpec_new();

    const auto backendName = map.value(QStringLiteral("backend_name")).toString();
    const auto ownerName = map.value(QStringLiteral("owner_name")).toString();
    const auto accountName = map.value(QStringLiteral("account_name")).toString();
    const auto currency = map.value(QStringLiteral("currency")).toString();
    const auto memo = map.value(QStringLiteral("memo")).toString();
    const auto iban = map.value(QStringLiteral("iban")).toString();
    const auto bic = map.value(QStringLiteral("bic")).toString();
    const auto country = map.value(QStringLiteral("country")).toString();
    const auto bankCode = map.value(QStringLiteral("bank_code")).toString();
    const auto bankName = map.value(QStringLiteral("bank_name")).toString();
    const auto branchId = map.value(QStringLiteral("branch_id")).toString();
    const auto accountNumber = map.value(QStringLiteral("account_number")).toString();
    const auto subAccountNumber = map.value(QStringLiteral("sub_account_number")).toString();
    const auto balance = map.value(QStringLiteral("balance")).toDouble();

    auto refAccounts = ReferenceAccounts();

    if (map.value(QStringLiteral("refAccounts")).canConvert<ReferenceAccounts>()) {
        refAccounts = qvariant_cast<ReferenceAccounts>(map.value(QStringLiteral("refAccounts")));
    }

    AB_AccountSpec_SetType(accountSpec, map.value(QStringLiteral("type")).toInt());
    // The key used to read "uniqueId" while toMap and the column both write
    // "unique_id", so every account read back carried an id of zero. toUInt
    // keeps ids beyond the range of a signed int intact.
    AB_AccountSpec_SetUniqueId(accountSpec, map.value(QStringLiteral("unique_id")).toUInt());
    AB_AccountSpec_SetBackendName(accountSpec, backendName.toLocal8Bit().constData());
    AB_AccountSpec_SetOwnerName(accountSpec, ownerName.toLocal8Bit().constData());
    AB_AccountSpec_SetAccountName(accountSpec, accountName.toLocal8Bit().constData());
    AB_AccountSpec_SetCurrency(accountSpec, currency.toLocal8Bit().constData());
    AB_AccountSpec_SetMemo(accountSpec, memo.toLocal8Bit().constData());
    AB_AccountSpec_SetIban(accountSpec, iban.toLocal8Bit().constData());
    AB_AccountSpec_SetBic(accountSpec, bic.toLocal8Bit().constData());
    AB_AccountSpec_SetCountry(accountSpec, country.toLocal8Bit().constData());
    AB_AccountSpec_SetBankCode(accountSpec, bankCode.toLocal8Bit().constData());
    AB_AccountSpec_SetBankName(accountSpec, bankName.toLocal8Bit().constData());
    AB_AccountSpec_SetBranchId(accountSpec, branchId.toLocal8Bit().constData());
    AB_AccountSpec_SetAccountNumber(accountSpec, accountNumber.toLocal8Bit().constData());
    AB_AccountSpec_SetSubAccountNumber(accountSpec, subAccountNumber.toLocal8Bit().constData());

    for (const auto &refAccount : std::as_const(refAccounts)) {
        auto refAccMap = refAccount->toMap();

        const auto refAccIban = refAccMap[QStringLiteral("iban")].toString();
        const auto refAccBic = refAccMap[QStringLiteral("bic")].toString();
        const auto refAccAccountNumber = refAccMap[QStringLiteral("account_number")].toString();
        const auto refAccSubAccountNumber = refAccMap[QStringLiteral("sub_account_number")]
                                                .toString();
        const auto refAccCountry = refAccMap[QStringLiteral("country")].toString();
        const auto refAccBankCode = refAccMap[QStringLiteral("bank_code")].toString();
        const auto refAccOwnerName = refAccMap[QStringLiteral("owner_name")].toString();
        const auto refAccOwnerName2 = refAccMap[QStringLiteral("owner_name2")].toString();
        const auto refAccAccountName = refAccMap[QStringLiteral("account_name")].toString();
        const auto refAccAccountType = refAccMap[QStringLiteral("account_type")].toInt();

        AB_REFERENCE_ACCOUNT *abRefAccount = AB_ReferenceAccount_new();

        AB_ReferenceAccount_SetIban(abRefAccount, refAccIban.toLocal8Bit().constData());
        AB_ReferenceAccount_SetBic(abRefAccount, refAccBic.toLocal8Bit().constData());
        AB_ReferenceAccount_SetAccountNumber(abRefAccount,
                                             refAccAccountNumber.toLocal8Bit().constData());
        AB_ReferenceAccount_SetSubAccountNumber(abRefAccount,
                                                refAccSubAccountNumber.toLocal8Bit().constData());
        AB_ReferenceAccount_SetCountry(abRefAccount, refAccCountry.toLocal8Bit().constData());
        AB_ReferenceAccount_SetBankCode(abRefAccount, refAccBankCode.toLocal8Bit().constData());
        AB_ReferenceAccount_SetOwnerName(abRefAccount, refAccOwnerName.toLocal8Bit().constData());
        AB_ReferenceAccount_SetOwnerName2(abRefAccount, refAccOwnerName2.toLocal8Bit().constData());
        AB_ReferenceAccount_SetAccountName(abRefAccount,
                                           refAccAccountName.toLocal8Bit().constData());
        AB_ReferenceAccount_SetAccountType(abRefAccount, refAccAccountType);

        AB_AccountSpec_AddReferenceAccount(accountSpec, AB_ReferenceAccount_dup(abRefAccount));

        AB_ReferenceAccount_free(abRefAccount);
        abRefAccount = nullptr;
        refAccMap.clear();
    }

    auto account = std::make_shared<Account>(accountSpec, balance);

    // A row from a store written before the column existed carries no value, and
    // neither does an account the wizard has not decided about. It stays without
    // a statement rather than being given one it never had.
    if (map.contains(QStringLiteral("active"))) {
        account->setActive(map.value(QStringLiteral("active")).toBool());
    }

    AB_AccountSpec_free(accountSpec);

    return account;
}

bool Account::isValid() const
{
    return type() != AB_AccountType_Invalid && type() != AB_AccountType_Unspecified;
}

QString Account::toString() const
{
    return QObject::tr("Account %1 [%2] - %3 - %4")
        .arg(accountNumber(), bankName(), ownerName(), accountName());
}

QMap<QString, QVariant> Account::toMap() const
{
    QMap<QString, QVariant> map = {};

    map[QStringLiteral("type")] = type();
    map[QStringLiteral("unique_id")] = uniqueId();
    map[QStringLiteral("backend_name")] = backendName();
    map[QStringLiteral("owner_name")] = ownerName();
    map[QStringLiteral("account_name")] = accountName();
    map[QStringLiteral("currency")] = currency();
    map[QStringLiteral("memo")] = memo();
    map[QStringLiteral("iban")] = iban();
    map[QStringLiteral("bic")] = bic();
    map[QStringLiteral("country")] = country();
    map[QStringLiteral("bank_code")] = bankCode();
    map[QStringLiteral("bank_name")] = bankName();
    map[QStringLiteral("branch_id")] = branchId();
    map[QStringLiteral("account_number")] = accountNumber();
    map[QStringLiteral("sub_account_number")] = subAccountNumber();
    map[QStringLiteral("refAccounts")] = QVariant::fromValue(referenceAccounts());
    map[QStringLiteral("balance")] = balance();

    if (d_ptr->active.has_value()) {
        map[QStringLiteral("active")] = *d_ptr->active;
    }

    return map;
}

QString Account::itemType() const
{
    return QStringLiteral("Account");
}
