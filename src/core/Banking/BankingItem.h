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

#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QMetaType>
#include <QtCore/QString>
#include <QtCore/QVariant>

#include <memory>

namespace olbaflinx::core::banking {

/**
 * The common interface of every banking record.
 *
 * Ownership: instances are created through the static factory methods of the
 * derived classes and are held in a BankingItemPtr only.
 */
class OLBAFLINX_CORE_EXPORT BankingItem
{
public:
    explicit BankingItem() = default;
    virtual ~BankingItem() = default;

    [[nodiscard]] virtual bool isValid() const = 0;

    [[nodiscard]] virtual QString toString() const = 0;

    /**
     * The keys of the map are the column names the storage writes under.
     */
    [[nodiscard]] virtual QMap<QString, QVariant> toMap() const = 0;

    /**
     * Names the derived class, which is how a receiver of a mixed list tells
     * the records apart.
     */
    [[nodiscard]] virtual QString itemType() const = 0;
};

/**
 * Storage and Banking create the records and hand them on through a signal.
 * The shared pointer makes the transfer of ownership visible and lets a record
 * outlive its creator, which releases the list right after emitting.
 */
using BankingItemPtr = std::shared_ptr<BankingItem>;
using BankingItems = QList<BankingItemPtr>;

} // namespace olbaflinx::core::banking

Q_DECLARE_METATYPE(olbaflinx::core::banking::BankingItem *)
Q_DECLARE_METATYPE(const olbaflinx::core::banking::BankingItem *)
Q_DECLARE_METATYPE(olbaflinx::core::banking::BankingItems)
