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

#include "core/Banking/Account/Account.h"
#include "core/Banking/BankingItem.h"

#include <QtCore/QAbstractListModel>

#include <memory>

namespace olbaflinx::ui::models {

/**
 * @brief Maps the accounts reported by core onto display roles.
 *
 * Ownership: the model holds the records it receives through setItems. That is
 * not a second copy of the truth, because Storage lets go of them once the
 * signal is emitted and holds none of them itself.
 */
class AccountTreeModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        UniqueIdRole = Qt::UserRole + 1,
        AccountNameRole,
        OwnerNameRole,
        BankNameRole,
        IbanRole,
        BicRole,
        AccountNumberRole,
        CurrencyRole,
        BalanceRole,
    };
    Q_ENUM(Role)

    explicit AccountTreeModel(QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

public Q_SLOTS:
    /**
     * @brief Takes over the reported records.
     *
     * Records that are not an account are skipped.
     */
    void setItems(const olbaflinx::core::banking::BankingItems &items);

private:
    QList<std::shared_ptr<olbaflinx::core::banking::account::Account>> m_accounts;
};

} // namespace olbaflinx::ui::models
