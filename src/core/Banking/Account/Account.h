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

#pragma once

#include "core/OlbaFlinxCore.h"

#include "core/Banking/Account/ReferenceAccount.h"
#include "core/Banking/BankingItem.h"

#include <aqbanking/types/account_spec.h>

#include <QtCore/QList>
#include <QtCore/QMetaType>
#include <QtCore/QtGlobal>

using namespace olbaflinx::core::banking;

namespace olbaflinx::core::banking::account {

typedef QList<ReferenceAccount *> ReferenceAccounts;

typedef AB_TRANSACTION_LIMITS TransactionLimits;
typedef AB_TRANSACTION_LIMITS_LIST TransactionLimitsList;
typedef AB_TRANSACTION_COMMAND TransactionCommand;

class OLBAFLINX_CORE_EXPORT Account : public BankingItem
{
public:
    explicit Account(const AB_ACCOUNT_SPEC *accountSpec = nullptr, double balance = 0.0);
    ~Account() override;

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
    [[nodiscard]] TransactionLimitsList *transactionLimits() const;
    [[nodiscard]] ReferenceAccounts referenceAccounts() const;
    [[nodiscard]] TransactionLimits *transactionLimitsForCommand(const TransactionCommand &cmd) const;

    [[nodiscard]] BankingItem *create(QMap<QString, QVariant> &map) const override;
    [[nodiscard]] bool isValid() const override;
    [[nodiscard]] QString toString() const override;
    [[nodiscard]] QMap<QString, QVariant> toMap() const override;
    [[nodiscard]] QString itemType() const override;

private:
    class Private;
    Private *d_ptr;
};

} // namespace olbaflinx::core::banking::account

Q_DECLARE_METATYPE(olbaflinx::core::banking::account::Account *)
