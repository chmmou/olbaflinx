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
#ifndef OLBAFLINX_ACCOUNT_H
#define OLBAFLINX_ACCOUNT_H

#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QVariant>

#include <aqbanking/account_type.h>
#include <aqbanking/types/account_spec.h>
#include <aqbanking/types/transactionlimits.h>

namespace olbaflinx::core::storage::account
{

class Account
{
    typedef AB_TRANSACTION_LIMITS TransactionLimits;
    typedef AB_TRANSACTION_LIMITS_LIST TransactionLimitsList;

public:
    explicit Account(const AB_ACCOUNT_SPEC *accountSpec);
    ~Account();

    [[nodiscard]] const qint32 type() const;
    [[nodiscard]] const QString typeString() const;
    [[nodiscard]] const quint32 uniqueId() const;
    [[nodiscard]] const QString backendName() const;
    [[nodiscard]] const QString ownerName() const;
    [[nodiscard]] const QString accountName() const;
    [[nodiscard]] const QString currency() const;
    [[nodiscard]] const QString memo() const;
    [[nodiscard]] const QString iban() const;
    [[nodiscard]] const QString bic() const;
    [[nodiscard]] const QString country() const;
    [[nodiscard]] const QString bankCode() const;
    [[nodiscard]] const QString bankName() const;
    [[nodiscard]] const QString branchId() const;
    [[nodiscard]] const QString accountNumber() const;
    [[nodiscard]] const QString subAccountNumber() const;
    [[nodiscard]] const TransactionLimitsList *transactionLimitsList() const;
    [[nodiscard]] const TransactionLimits *transactionLimitsForCommand(
        const AB_TRANSACTION_COMMAND &cmd
    ) const;

    [[nodiscard]] static Account *create(const QMap<QString, QVariant> &row) const;

private:
    AB_ACCOUNT_SPEC *abAccountSpec;
};

}

Q_DECLARE_METATYPE(olbaflinx::core::storage::account::Account *)

#endif // OLBAFLINX_ACCOUNT_H
