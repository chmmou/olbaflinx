/**
 * Copyright (C) 2021, Alexander Saal <developer@olbaflinx.chm-projects.de>
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

#include <QtCore/QObject>
#include <QtSql/QSqlField>
#include <QtSql/QSqlRecord>

#include "Account.h"
#include "core/Container.h"

using namespace olbaflinx::core::storage::account;

Account::Account(const AB_ACCOUNT_SPEC *accountSpec, double balance)
    : abAccountSpec(AB_AccountSpec_dup(accountSpec))
    , m_balance(balance)
{}

Account::~Account()
{
    if (abAccountSpec != nullptr) {
        AB_AccountSpec_free(abAccountSpec);
    }
    abAccountSpec = nullptr;
}

qint32 Account::type() const
{
    return AB_AccountSpec_GetType(abAccountSpec);
}

QString Account::typeString() const
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

quint32 Account::uniqueId() const
{
    return AB_AccountSpec_GetUniqueId(abAccountSpec);
}

QString Account::backendName() const
{
    return QString::fromUtf8(AB_AccountSpec_GetBackendName(abAccountSpec));
}

QString Account::ownerName() const
{
    return QString::fromUtf8(AB_AccountSpec_GetOwnerName(abAccountSpec));
}

QString Account::accountName() const
{
    return QString::fromUtf8(AB_AccountSpec_GetAccountName(abAccountSpec));
}

QString Account::currency() const
{
    return QString::fromUtf8(AB_AccountSpec_GetCurrency(abAccountSpec));
}

QString Account::memo() const
{
    return QString::fromUtf8(AB_AccountSpec_GetMemo(abAccountSpec));
}

QString Account::iban() const
{
    return QString::fromUtf8(AB_AccountSpec_GetIban(abAccountSpec));
}

QString Account::bic() const
{
    return QString::fromUtf8(AB_AccountSpec_GetBic(abAccountSpec));
}

QString Account::country() const
{
    return QString::fromUtf8(AB_AccountSpec_GetCountry(abAccountSpec));
}

QString Account::bankCode() const
{
    return QString::fromUtf8(AB_AccountSpec_GetBankCode(abAccountSpec));
}

QString Account::bankName() const
{
    return QString::fromUtf8(AB_AccountSpec_GetBankName(abAccountSpec));
}

QString Account::branchId() const
{
    return QString::fromUtf8(AB_AccountSpec_GetBranchId(abAccountSpec));
}

QString Account::accountNumber() const
{
    return QString::fromUtf8(AB_AccountSpec_GetAccountNumber(abAccountSpec));
}

QString Account::subAccountNumber() const
{
    return QString::fromUtf8(AB_AccountSpec_GetSubAccountNumber(abAccountSpec));
}

double Account::balance() const
{
    return m_balance;
}

TransactionLimitsList *Account::transactionLimitsList() const
{
    return AB_AccountSpec_GetTransactionLimitsList(abAccountSpec);
}

TransactionLimits *Account::transactionLimitsForCommand(const AB_TRANSACTION_COMMAND &cmd) const
{
    return AB_AccountSpec_GetTransactionLimitsForCommand(abAccountSpec, cmd);
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

QSqlQuery Account::createInsertQuery(QSqlQuery &query) const
{
    query.prepare(StorageSqlAccountInsertQuery);
    query.bindValue(":type", type());
    query.bindValue(":unique_id", uniqueId());
    query.bindValue(":backend_name", backendName());
    query.bindValue(":owner_name", ownerName());
    query.bindValue(":account_name", accountName());
    query.bindValue(":currency", currency());
    query.bindValue(":memo", memo());
    query.bindValue(":iban", iban());
    query.bindValue(":bic", bic());
    query.bindValue(":country", country());
    query.bindValue(":bank_code", bankCode());
    query.bindValue(":bank_name", bankName());
    query.bindValue(":branch_id", branchId());
    query.bindValue(":account_number", accountNumber());
    query.bindValue(":sub_account_number", subAccountNumber());

    return query;
}

QMap<QString, QVariant> Account::queryToMap(const QSqlQuery &query)
{
    QMap<QString, QVariant> map = {};

    map["type"] = query.record().field("type").value();
    map["uniqueId"] = query.record().field("unique_id").value();
    map["backendName"] = query.record().field("backend_name").value();
    map["ownerName"] = query.record().field("owner_name").value();
    map["accountName"] = query.record().field("account_name").value();
    map["currency"] = query.record().field("currency").value();
    map["memo"] = query.record().field("memo").value();
    map["iban"] = query.record().field("iban").value();
    map["bic"] = query.record().field("bic").value();
    map["country"] = query.record().field("country").value();
    map["bankCode"] = query.record().field("bank_code").value();
    map["bankName"] = query.record().field("bank_name").value();
    map["branchId"] = query.record().field("branch_id").value();
    map["accountNumber"] = query.record().field("account_number").value();
    map["subAccountNumber"] = query.record().field("sub_account_number").value();
    map["balance"] = query.record().field("balance").value();

    return map;
}

Account *Account::create(const QMap<QString, QVariant> &row)
{
    auto accountSpec = AB_AccountSpec_new();

    const QString backendName = row["backendName"].toString();
    const QString ownerName = row["ownerName"].toString();
    const QString accountName = row["accountName"].toString();
    const QString currency = row["currency"].toString();
    const QString memo = row["memo"].toString();
    const QString iban = row["iban"].toString();
    const QString bic = row["bic"].toString();
    const QString country = row["country"].toString();
    const QString bankCode = row["bankCode"].toString();
    const QString bankName = row["bankName"].toString();
    const QString branchId = row["branchId"].toString();
    const QString accountNumber = row["accountNumber"].toString();
    const QString subAccountNumber = row["subAccountNumber"].toString();
    const double balance = row["balance"].toDouble();

    AB_AccountSpec_SetType(accountSpec, row["type"].toInt());
    AB_AccountSpec_SetUniqueId(accountSpec, row["uniqueId"].toInt());
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

    const auto account = new Account(accountSpec, balance);

    AB_AccountSpec_free(accountSpec);

    return account;
}
