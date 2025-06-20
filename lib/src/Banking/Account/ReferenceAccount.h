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
#ifndef OLBAFLINX_CORE_ACCOUNT_REFERENCE_H
#define OLBAFLINX_CORE_ACCOUNT_REFERENCE_H

#include "OlbaFlinxCore.h"

#include "Banking/BankingItem.h"

#include <aqbanking/types/refaccount.h>

#include <QtCore/QMetaType>
#include <QtCore/QtGlobal>
#include <QtCore/QList>

using namespace olbaflinx::core::banking;

namespace olbaflinx::core::banking::account {

class OLBAFLINX_CORE_EXPORT ReferenceAccount : public BankingItem
{
public:
    explicit ReferenceAccount(const AB_REFERENCE_ACCOUNT *refAccount = nullptr);
    ~ReferenceAccount() override;

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

Q_DECLARE_METATYPE(olbaflinx::core::banking::account::ReferenceAccount *)

#endif //OLBAFLINX_CORE_ACCOUNT_REFERENCE_H
