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

#include <aqbanking/types/refaccount.h>

#include <QtCore/QList>
#include <QtCore/QMetaType>
#include <QtCore/QtGlobal>

using namespace olbaflinx::core::banking;

namespace olbaflinx::core::banking::account {

/**
 * A reference account held with an account.
 *
 * Ownership: the creator owns the instance. Reference accounts read from the
 * database are created through fromMap and handed on as a BankingItemPtr.
 */
class OLBAFLINX_CORE_EXPORT ReferenceAccount : public BankingItem
{
public:
    explicit ReferenceAccount(const AB_REFERENCE_ACCOUNT *refAccount = nullptr);
    ~ReferenceAccount() override;

    // The instance owns a C structure and frees it. A copy would hand the same
    // pointer to two destructors, so the compiler generated ones are withdrawn
    // rather than left to be called by accident.
    ReferenceAccount(const ReferenceAccount &) = delete;
    ReferenceAccount &operator=(const ReferenceAccount &) = delete;
    ReferenceAccount(ReferenceAccount &&) = delete;
    ReferenceAccount &operator=(ReferenceAccount &&) = delete;

    /**
     * Creates a reference account from the column values of a database row. An
     * empty map answers with an empty pointer.
     */
    [[nodiscard]] static std::shared_ptr<ReferenceAccount> fromMap(
        const QMap<QString, QVariant> &map);

    [[nodiscard]] qint32 accountType() const;
    [[nodiscard]] QString ownerName() const;
    [[nodiscard]] QString ownerName2() const;
    [[nodiscard]] QString accountName() const;
    [[nodiscard]] QString iban() const;
    [[nodiscard]] QString bic() const;
    [[nodiscard]] QString country() const;
    [[nodiscard]] QString bankCode() const;
    [[nodiscard]] QString accountNumber() const;
    [[nodiscard]] QString subAccountNumber() const;

    [[nodiscard]] bool isValid() const override;
    [[nodiscard]] QString toString() const override;
    [[nodiscard]] QMap<QString, QVariant> toMap() const override;
    [[nodiscard]] QString itemType() const override;

private:
    class Private;
    Private *d_ptr = nullptr;
};

} // namespace olbaflinx::core::banking::account

Q_DECLARE_METATYPE(olbaflinx::core::banking::account::ReferenceAccount *)
