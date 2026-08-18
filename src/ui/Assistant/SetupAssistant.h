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

#include "core/ApplicationInfo.h"
#include "core/Banking/BankingItem.h"

#include <QtWidgets/QWizard>

namespace olbaflinx::ui::assistant {

/**
 * Guides the first run: banking backend and account setup.
 *
 * Ownership: belongs to its parent widget. The ApplicationInfo is copied into
 * the pages, nothing is kept by reference.
 */
class SetupAssistant : public QWizard
{
    Q_OBJECT

public:
    /**
     * The application info is what the banking page sets itself up with.
     */
    explicit SetupAssistant(const olbaflinx::core::ApplicationInfo &applicationInfo,
                            QWidget *parent = nullptr,
                            Qt::WindowFlags flags = Qt::WindowFlags());
    ~SetupAssistant() override;

    /**
     * The accounts the user chose, as objects.
     *
     * Empty when the wizard was cancelled. Ownership is shared with the caller,
     * the same way itemsReceived hands over what it reports.
     *
     * OptionBankingPage::selectedAccountIds stays where it is. It answers the
     * question the wizard pages ask; this one answers the question the caller
     * asks.
     */
    [[nodiscard]] olbaflinx::core::banking::BankingItems selectedAccounts() const;

    /**
     * Every account the wizard put up for choice.
     *
     * Whoever stores the result needs both lists. An account in this one and not
     * in the chosen one was turned down; one in neither was never on offer, and
     * nothing about it may change.
     *
     * Empty when the wizard was cancelled, so that a cancelled run changes
     * nothing at all.
     */
    [[nodiscard]] olbaflinx::core::banking::BankingItems offeredAccounts() const;

private:
    class Private;
    Private *d_ptr = nullptr;
};

} // namespace olbaflinx::ui::assistant
