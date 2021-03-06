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
#ifndef OLBAFLINX_ACCOUNT_H
#define OLBAFLINX_ACCOUNT_H

#include <QtCore/qglobal.h>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QVariant>
#include <QtSql/QSqlQuery>

#include <aqbanking/account_type.h>
#include <aqbanking/types/account_spec.h>
#include <aqbanking/types/transactionlimits.h>

namespace olbaflinx::core::storage::account {

typedef AB_TRANSACTION_LIMITS TransactionLimits;
typedef AB_TRANSACTION_LIMITS_LIST TransactionLimitsList;

class Account
{
public:
    explicit Account(const AB_ACCOUNT_SPEC *accountSpec = Q_NULLPTR, double balance = 0.0);
    ~Account();

    [[nodiscard]] qint32 type() const;
    [[nodiscard]] QString typeString() const;
    [[nodiscard]] quint32 uniqueId() const;
    [[nodiscard]] QString backendName() const;
    [[nodiscard]] QString ownerName() const;
    [[nodiscard]] QString accountName() const;
    [[nodiscard]] QString currency() const;
    [[nodiscard]] QString memo() const;
    [[nodiscard]] QString iban() const;
    [[nodiscard]] QString bic() const;
    [[nodiscard]] QString country() const;
    [[nodiscard]] QString bankCode() const;
    [[nodiscard]] QString bankName() const;
    [[nodiscard]] QString branchId() const;
    [[nodiscard]] QString accountNumber() const;
    [[nodiscard]] QString subAccountNumber() const;
    [[nodiscard]] double balance() const;
    [[nodiscard]] TransactionLimitsList *transactionLimitsList() const;
    [[nodiscard]] TransactionLimits *transactionLimitsForCommand(const AB_TRANSACTION_COMMAND &cmd) const;

    [[nodiscard]] bool isValid() const;
    [[nodiscard]] QString toString() const;

    [[nodiscard]] QSqlQuery createInsertQuery(QSqlQuery &query) const;
    [[nodiscard]] static QMap<QString, QVariant> queryToMap(const QSqlQuery &query);
    [[nodiscard]] static Account *create(const QMap<QString, QVariant> &row);

private:
    AB_ACCOUNT_SPEC *abAccountSpec;
    double m_balance;
};

} // namespace olbaflinx::core::storage::account

Q_DECLARE_METATYPE(olbaflinx::core::storage::account::Account *)
Q_DECLARE_METATYPE(const olbaflinx::core::storage::account::Account *)

#endif // OLBAFLINX_ACCOUNT_H
