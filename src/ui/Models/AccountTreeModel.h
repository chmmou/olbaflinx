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

#include <QtCore/QAbstractItemModel>

#include <memory>

namespace olbaflinx::ui::models {

/**
 * Maps the accounts reported by core onto two levels, bank and account.
 *
 * The upper level is no record of its own. It is formed from the bank name every
 * account carries, so two banks of that same name become one node and cannot be
 * told apart. A bank node answers every account role with an invalid QVariant,
 * which is how a selection recognises that no account was picked.
 *
 * Ownership: the model holds the records it receives through setItems. That is
 * not a second copy of the truth, because Storage lets go of them once the
 * signal is emitted and holds none of them itself.
 */
class AccountTreeModel final : public QAbstractItemModel
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

    [[nodiscard]] QModelIndex index(int row,
                                    int column,
                                    const QModelIndex &parent = QModelIndex()) const override;
    [[nodiscard]] QModelIndex parent(const QModelIndex &child) const override;
    [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    [[nodiscard]] int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    /**
     * The account an index stands for.
     *
     * The roles carry the fields of an account, not the account itself, and a
     * fetch needs the record: the banking layer fills its orders from the
     * description an account carries and no set of fields can stand in for it.
     *
     * A bank node and an invalid index answer with an empty pointer. The
     * account is shared with the model and stays valid for as long as the model
     * holds it, which the next setItems ends.
     */
    [[nodiscard]] std::shared_ptr<olbaflinx::core::banking::account::Account> accountAt(
        const QModelIndex &index) const;

public Q_SLOTS:
    /**
     * Takes over the reported records and groups them by bank.
     *
     * Records that are not an account are skipped, and so are accounts the user
     * has deselected. A bank whose accounts are all deselected leaves no node
     * behind.
     */
    void setItems(const olbaflinx::core::banking::BankingItems &items);

private:
    struct Bank
    {
        QString name;
        QList<std::shared_ptr<olbaflinx::core::banking::account::Account>> accounts;
    };

    QList<Bank> m_banks;
};

} // namespace olbaflinx::ui::models
