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

#ifndef OLBAFLINX_ACCOUNTBALANCE_H
#define OLBAFLINX_ACCOUNTBALANCE_H

#include <QtCore/QDate>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QVariant>

#include <QtSql/QSqlQuery>

#include <aqbanking/types/balance.h>

namespace olbaflinx::core::storage::account {

class AccountBalance
{
public:
    explicit AccountBalance(const AB_BALANCE *abBalance = Q_NULLPTR);
    ~AccountBalance();

    [[nodiscard]] QString type() const;
    [[nodiscard]] QDate date() const;
    [[nodiscard]] QString currency() const;
    [[nodiscard]] double balance() const;

    [[nodiscard]] QSqlQuery createInsertQuery(const quint32 &accountId, QSqlQuery &query) const;
    [[nodiscard]] static QMap<QString, QVariant> queryToMap(const QSqlQuery &query);
    [[nodiscard]] static AccountBalance *create(const QMap<QString, QVariant> &row);

private:
    AB_BALANCE *m_balance;
};

} // namespace olbaflinx::core::storage::account

Q_DECLARE_METATYPE(olbaflinx::core::storage::account::AccountBalance *)
Q_DECLARE_METATYPE(const olbaflinx::core::storage::account::AccountBalance *)

#endif //OLBAFLINX_ACCOUNTBALANCE_H
