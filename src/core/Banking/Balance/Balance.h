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

#include "core/Banking/BankingItem.h"

#include <aqbanking/types/balance.h>

#include <QtCore/QDate>
#include <QtCore/QMetaType>

using namespace olbaflinx::core::banking;

namespace olbaflinx::core::banking::balance {

typedef AB_BALANCE_TYPE BalanceType;

/**
 * @brief The balance of an account, as the bank reports it.
 *
 * Carries the four values the balance table holds - amount, date, type and
 * currency - plus the id the banking backend keeps the account under. The
 * account itself carries the amount alone, which is not enough to choose
 * between the balances a bank sends: that choice goes by the type.
 *
 * Ownership: the creator owns the instance and hands it on as a BankingItemPtr.
 * The AB_BALANCE handed to the constructor stays with its caller, a copy of it
 * is kept here.
 */
class OLBAFLINX_CORE_EXPORT Balance : public BankingItem
{
public:
    explicit Balance(quint32 uniqueAccountId = 0, const AB_BALANCE *balance = nullptr);
    ~Balance() override;

    // The instance owns a C structure and frees it. A copy would hand the same
    // pointer to two destructors, so the compiler generated ones are withdrawn
    // rather than left to be called by accident.
    Balance(const Balance &) = delete;
    Balance &operator=(const Balance &) = delete;
    Balance(Balance &&) = delete;
    Balance &operator=(Balance &&) = delete;

    /**
     * @brief The account this balance belongs to, as the banking backend keeps it.
     *
     * Not the row id of the stored account. The storage translates the one into
     * the other when it writes.
     */
    [[nodiscard]] quint32 uniqueAccountId() const;

    [[nodiscard]] QDate date() const;
    [[nodiscard]] qreal value() const;
    [[nodiscard]] QString currency() const;
    [[nodiscard]] BalanceType type() const;

    [[nodiscard]] bool isValid() const override;
    [[nodiscard]] QString toString() const override;
    [[nodiscard]] QMap<QString, QVariant> toMap() const override;
    [[nodiscard]] QString itemType() const override;

private:
    class Private;
    Private *d_ptr = nullptr;
};

} // namespace olbaflinx::core::banking::balance

Q_DECLARE_METATYPE(olbaflinx::core::banking::balance::Balance *)
