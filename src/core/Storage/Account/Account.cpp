/**
 * Copyright (c) 2021, Alexander Saal <alexander.saal@chm-projects.de>
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

#include <QtCore/QMetaType>

#include "Account.h"

using namespace olbaflinx::core::storage::account;

int metaDataTypeId = 0;

Account::Account(const AB_ACCOUNT_SPEC *accountSpec)
    : abAccountSpec(AB_AccountSpec_dup(accountSpec))
{
    metaDataTypeId = qRegisterMetaType<olbaflinx::core::storage::account::Account *>();
}
Account::~Account()
{
    QMetaType::unregisterType(metaDataTypeId);

    if (abAccountSpec != nullptr) {
        AB_AccountSpec_free(abAccountSpec);
    }
    abAccountSpec = nullptr;
}

const qint32 Account::type() const
{
    return AB_AccountSpec_GetType(abAccountSpec);
}

const QString Account::typeString() const
{
    QString typeString;
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
            typeString = "";
    }

    return typeString;
}

const quint32 Account::uniqueId() const
{
    return AB_AccountSpec_GetUniqueId(abAccountSpec);
}

const QString Account::backendName() const
{
    return QString::fromUtf8(AB_AccountSpec_GetBackendName(abAccountSpec));
}

const QString Account::ownerName() const
{
    return QString::fromUtf8(AB_AccountSpec_GetOwnerName(abAccountSpec));
}

const QString Account::accountName() const
{
    return QString::fromUtf8(AB_AccountSpec_GetAccountName(abAccountSpec));
}

const QString Account::currency() const
{
    return QString::fromUtf8(AB_AccountSpec_GetCurrency(abAccountSpec));
}

const QString Account::memo() const
{
    return QString::fromUtf8(AB_AccountSpec_GetMemo(abAccountSpec));
}

const QString Account::iban() const
{
    return QString::fromUtf8(AB_AccountSpec_GetIban(abAccountSpec));
}

const QString Account::bic() const
{
    return QString::fromUtf8(AB_AccountSpec_GetBic(abAccountSpec));
}

const QString Account::country() const
{
    return QString::fromUtf8(AB_AccountSpec_GetCountry(abAccountSpec));
}

const QString Account::bankCode() const
{
    return QString::fromUtf8(AB_AccountSpec_GetBankCode(abAccountSpec));
}

const QString Account::bankName() const
{
    return QString::fromUtf8(AB_AccountSpec_GetBankName(abAccountSpec));
}

const QString Account::branchId() const
{
    return QString::fromUtf8(AB_AccountSpec_GetBranchId(abAccountSpec));
}

const QString Account::accountNumber() const
{
    return QString::fromUtf8(AB_AccountSpec_GetAccountNumber(abAccountSpec));
}

const QString Account::subAccountNumber() const
{
    return QString::fromUtf8(AB_AccountSpec_GetSubAccountNumber(abAccountSpec));
}

const Account::TransactionLimitsList *Account::transactionLimitsList() const
{
    return AB_AccountSpec_GetTransactionLimitsList(abAccountSpec);
}

const Account::TransactionLimits *Account::transactionLimitsForCommand(
    const AB_TRANSACTION_COMMAND &cmd
) const
{
    return AB_AccountSpec_GetTransactionLimitsForCommand(abAccountSpec, cmd);
}

Account *Account::create(const QMap<QString, QVariant> &row) const
{
    auto accountSpec = AB_AccountSpec_new();

    AB_AccountSpec_SetType(accountSpec, row["type"].toInt());
    AB_AccountSpec_SetUniqueId(accountSpec, row["unique_id"].toInt());
    AB_AccountSpec_SetBackendName(
        accountSpec,
        row["backend_name"].toString().toLocal8Bit().constData()
    );
    AB_AccountSpec_SetOwnerName(
        accountSpec,
        row["owner_name"].toString().toLocal8Bit().constData()
    );
    AB_AccountSpec_SetAccountName(
        accountSpec,
        row["account_name"].toString().toLocal8Bit().constData()
    );
    AB_AccountSpec_SetCurrency(
        accountSpec,
        row["currency"].toString().toLocal8Bit().constData()
    );
    AB_AccountSpec_SetMemo(
        accountSpec,
        row["memo"].toString().toLocal8Bit().constData()
    );
    AB_AccountSpec_SetIban(
        accountSpec,
        row["iban"].toString().toLocal8Bit().constData()
    );
    AB_AccountSpec_SetBic(
        accountSpec,
        row["bic"].toString().toLocal8Bit().constData()
    );
    AB_AccountSpec_SetCountry(
        accountSpec,
        row["country"].toString().toLocal8Bit().constData()
    );
    AB_AccountSpec_SetBankCode(
        accountSpec,
        row["bank_code"].toString().toLocal8Bit().constData()
    );
    AB_AccountSpec_SetBankName(
        accountSpec,
        row["bank_name"].toString().toLocal8Bit().constData()
    );
    AB_AccountSpec_SetBranchId(
        accountSpec,
        row["branch_id"].toString().toLocal8Bit().constData()
    );
    AB_AccountSpec_SetAccountNumber(
        accountSpec,
        row["account_number"].toString().toLocal8Bit().constData()
    );
    AB_AccountSpec_SetSubAccountNumber(
        accountSpec,
        row["sub_account_number"].toString().toLocal8Bit().constData()
    );

    const auto account = new Account(accountSpec);

    AB_AccountSpec_free(accountSpec);

    return account;
}

