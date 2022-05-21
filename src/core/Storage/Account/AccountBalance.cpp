/**
* Copyright (C) 2022, Alexander Saal <developer@olbaflinx.chm-projects.de>
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

#include <QtCore/QVariant>
#include <QtSql/QSqlField>
#include <QtSql/QSqlRecord>

#include <aqbanking/types/value.h>

#include "core/Utils.h"

#include "AccountBalance.h"

using namespace olbaflinx::core::storage::account;

AccountBalance::AccountBalance(const AB_BALANCE *abBalance)
    : m_balance(AB_Balance_dup(abBalance))
{}

AccountBalance::~AccountBalance()
{
    AB_Balance_free(m_balance);
}

QString AccountBalance::type() const
{
    return QString::fromUtf8(AB_Balance_Type_toString(AB_Balance_GetType(m_balance)));
}

QDate AccountBalance::date() const
{
    return Utils::gwenDateToQDate(AB_Balance_GetDate(m_balance));
}

QString AccountBalance::currency() const
{
    return QString::fromUtf8(AB_Value_GetCurrency(AB_Balance_GetValue(m_balance)));
}

double AccountBalance::balance() const
{
    return AB_Value_GetValueAsDouble(AB_Balance_GetValue(m_balance));
}

QSqlQuery AccountBalance::createInsertQuery(const quint32 &accountId, QSqlQuery &query) const
{
    query.prepare(StorageSqlAccountBalanceInsertQuery);
    query.bindValue(":account_id", accountId);
    query.bindValue(":date", date());
    query.bindValue(":value", date());
    query.bindValue(":type", type());
    query.bindValue(":currency", currency());

    return query;
}

QMap<QString, QVariant> AccountBalance::queryToMap(const QSqlQuery &query)
{
    QMap<QString, QVariant> map = QMap<QString, QVariant>();

    map["accountId"] = query.record().field("account_id").value();
    map["date"] = query.record().field("date").value();
    map["value"] = query.record().field("value").value();
    map["type"] = query.record().field("type").value();
    map["currency"] = query.record().field("currency").value();

    return map;
}

AccountBalance *AccountBalance::create(const QMap<QString, QVariant> &row)
{
    auto balance = AB_Balance_new();

    auto value = AB_Value_new();
    AB_Value_SetValueFromDouble(value, row["value"].toDouble());
    AB_Value_SetCurrency(value, row["currency"].toString().toLocal8Bit().constData());
    AB_Balance_SetValue(balance, AB_Value_dup(value));
    AB_Value_free(value);
    value = Q_NULLPTR;

    AB_Balance_SetDate(balance, Utils::qDateToGwenDate(row["value"].toDate()));

    const auto type = AB_Balance_Type_fromString(row["type"].toString().toLatin1().constData());
    AB_Balance_SetType(balance, type);

    auto accBalance = new AccountBalance(balance);

    AB_Balance_free(balance);

    return accBalance;
}
