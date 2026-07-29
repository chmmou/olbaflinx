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

#include "core/Banking/BankingItem.h"
#include "core/Banking/Transaction/Transaction.h"

#include <QtCore/QAbstractListModel>

#include <memory>

namespace olbaflinx::ui::models {

/**
 * @brief Bildet die von core gemeldeten Umsaetze auf Anzeigerollen ab.
 *
 * Eigentum: Das Modell haelt die Datensaetze, die es ueber setItems erhaelt.
 * Das ist keine zweite Kopie der Wahrheit im Sinne von QT-ARCH-022, denn
 * Storage gibt sie nach dem Signal aus der Hand und haelt sie selbst nicht.
 */
class TransactionListModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        UniqueIdRole = Qt::UserRole + 1,
        DateRole,
        ValutaDateRole,
        ValueRole,
        CurrencyRole,
        PurposeRole,
        RemoteNameRole,
        RemoteIbanRole,
        CategoryRole,
    };
    Q_ENUM(Role)

    explicit TransactionListModel(QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

public Q_SLOTS:
    /**
     * @brief Uebernimmt die gemeldeten Datensaetze.
     *
     * Datensaetze, die kein Umsatz sind, werden uebergangen.
     */
    void setItems(const olbaflinx::core::banking::BankingItems &items);

private:
    QList<std::shared_ptr<olbaflinx::core::banking::transaction::Transaction>> m_transactions;
};

} // namespace olbaflinx::ui::models
