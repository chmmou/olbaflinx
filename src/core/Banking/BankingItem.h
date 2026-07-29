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
 * @brief The common interface of every banking record.
 *
 * Ownership: instances are created through the static factory methods of the
 * derived classes and are held in a BankingItemPtr only.
 */
class OLBAFLINX_CORE_EXPORT BankingItem
{
public:
    explicit BankingItem() = default;
    virtual ~BankingItem() = default;

    /**
     * @brief Checks whether the corresponding class is valid
     *
     * @return true if we have a valid corresponding class; otherwise false.
     */
    [[nodiscard]] virtual bool isValid() const = 0;

    /**
     * @brief Presents the corresponding class as a string.
     *
     * @return Corresponding class as a string.
     */
    [[nodiscard]] virtual QString toString() const = 0;

    /**
     * @brief Converts the corresponding class into a QMap.
     *
     * @return A corresponding class as a QMap
     */
    [[nodiscard]] virtual QMap<QString, QVariant> toMap() const = 0;

    /**
     * @brief Corresponding class type.
     *
     * @return Corresponding class type.
     */
    [[nodiscard]] virtual QString itemType() const = 0;
};

/**
 * Storage and Banking create the records and hand them on through a signal. A
 * shared pointer makes the transfer of ownership visible and survives the
 * creator. A raw pointer did not: the creator released the list right after
 * emitting the signal.
 */
using BankingItemPtr = std::shared_ptr<BankingItem>;
using BankingItems = QList<BankingItemPtr>;

} // namespace olbaflinx::core::banking

Q_DECLARE_METATYPE(olbaflinx::core::banking::BankingItem *)
Q_DECLARE_METATYPE(const olbaflinx::core::banking::BankingItem *)
Q_DECLARE_METATYPE(olbaflinx::core::banking::BankingItems)
