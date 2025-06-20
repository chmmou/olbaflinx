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

#ifndef OLBAFLINX_CORE_BANKING_ITEM_H
#define OLBAFLINX_CORE_BANKING_ITEM_H

#include "OlbaFlinxCore.h"

#include <QtCore/QMap>
#include <QtCore/QMetaType>
#include <QtCore/QString>
#include <QtCore/QVariant>

namespace olbaflinx::core::banking {

class OLBAFLINX_CORE_EXPORT BankingItem
{
public:
    explicit BankingItem() = default;
    virtual ~BankingItem() = default;

    /**
     * @brief Creates a new BankingItem object from the passed map for the corresponding class.
     *
     * @param map
     *
     * @return New BankingItem object from the corresponding class.
     */
    [[nodiscard]] virtual BankingItem *create(QMap<QString, QVariant> &map) const = 0;

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

} // namespace olbaflinx::core::banking

Q_DECLARE_METATYPE(olbaflinx::core::banking::BankingItem *)
Q_DECLARE_METATYPE(const olbaflinx::core::banking::BankingItem *)

#endif //OLBAFLINX_CORE_BANKING_ITEM_H
