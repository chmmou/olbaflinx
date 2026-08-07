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

#pragma once

#include "core/OlbaFlinxCore.h"

#include "core/Banking/Account/ReferenceAccount.h"
#include "core/Banking/BankingItem.h"

#include <aqbanking/types/account_spec.h>

#include <QtCore/QList>
#include <QtCore/QMetaType>
#include <QtCore/QtGlobal>

#include <memory>

using namespace olbaflinx::core::banking;

namespace olbaflinx::core::banking::account {

/**
 * The entries are shared, not owned by whoever asked for the list. A raw pointer
 * put the release on the caller, and there was no way to see that from the
 * signature: the same list also travelled through QVariant, where nobody was
 * left to do it.
 */
using ReferenceAccounts = QList<std::shared_ptr<ReferenceAccount>>;

typedef AB_TRANSACTION_LIMITS TransactionLimits;
typedef AB_TRANSACTION_LIMITS_LIST TransactionLimitsList;
typedef AB_TRANSACTION_COMMAND TransactionCommand;

/**
 * @brief An account reported by AqBanking.
 *
 * Ownership: the creator owns the instance. Accounts read from the database are
 * created through fromMap and handed on as a BankingItemPtr. The reference
 * accounts reported by referenceAccounts are shared with the caller.
 */
class OLBAFLINX_CORE_EXPORT Account : public BankingItem
{
public:
    explicit Account(const AB_ACCOUNT_SPEC *accountSpec = nullptr, double balance = 0.0);
    ~Account() override;

    /**
     * @brief Creates an account from the column values of a database row.
     *
     * @param map Column values, named after the binding names of the query.
     *
     * @return The new account, or an empty pointer if the map is empty.
     */
    [[nodiscard]] static std::shared_ptr<Account> fromMap(const QMap<QString, QVariant> &map);

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

    /**
     * @brief Whether the user keeps this account.
     *
     * Set by the wizard, not reported by the bank. An account nobody has decided
     * about counts as kept, and storing such an account leaves the state of an
     * already stored one where it is. That is what keeps a deselected account
     * out of sight when the wizard merely offers it again.
     */
    [[nodiscard]] bool isActive() const;
    void setActive(bool active);

    [[nodiscard]] TransactionLimitsList *transactionLimits() const;
    [[nodiscard]] ReferenceAccounts referenceAccounts() const;
    [[nodiscard]] TransactionLimits *transactionLimitsForCommand(const TransactionCommand &cmd) const;

    [[nodiscard]] bool isValid() const override;
    [[nodiscard]] QString toString() const override;
    [[nodiscard]] QMap<QString, QVariant> toMap() const override;
    [[nodiscard]] QString itemType() const override;

private:
    class Private;
    Private *d_ptr = nullptr;
};

} // namespace olbaflinx::core::banking::account

Q_DECLARE_METATYPE(olbaflinx::core::banking::account::Account *)

// The list travels through QVariant, in Account::toMap and out again in
// Account::fromMap. Without the declaration canConvert answers false and the
// reference accounts of an account are silently dropped on the way.
Q_DECLARE_METATYPE(olbaflinx::core::banking::account::ReferenceAccounts)
