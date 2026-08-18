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

#include <QtCore/QList>

#include <QtWidgets/QWizardPage>

namespace olbaflinx::ui::assistant::pages {

/**
 * The wizard page that connects to the banking backend and lists the accounts
 * it finds.
 *
 * Ownership: belongs to the wizard it is added to. The Banking instance and the
 * gwenhywfar user interface it creates are owned by this page and released with
 * it, in that order: the backend detaches from the interface as it shuts down.
 */
class OptionBankingPage : public QWizardPage
{
    Q_OBJECT

public:
    explicit OptionBankingPage(QWidget *parent = nullptr);
    ~OptionBankingPage() override;

    /**
     * Sets up the connection to the banking backend and reads the accounts.
     *
     * The setup happens here and not in the constructor because uic creates the
     * page without arguments and the application details are only available
     * afterwards.
     */
    void initialize(const olbaflinx::core::ApplicationInfo &applicationInfo);
    [[nodiscard]] bool isComplete() const override;

    /**
     * Takes the accounts the page puts up for choice.
     *
     * Where they come from is not the concern of the page. initialize connects
     * the backend to this; a test hands them over directly.
     */
    void setAccounts(const olbaflinx::core::banking::BankingItems &accounts);

    [[nodiscard]] QList<quint32> selectedAccountIds() const;

    /**
     * The accounts the user chose, as objects.
     *
     * Ownership is shared with whoever asked, the same way itemsReceived hands
     * over what it reports.
     */
    [[nodiscard]] olbaflinx::core::banking::BankingItems selectedAccounts() const;

    /**
     * Every account the page put up for choice.
     *
     * Whoever stores the result needs both lists. An account in this one and not
     * in the chosen one was turned down; one in neither was never on offer, and
     * nothing about it may change.
     */
    [[nodiscard]] olbaflinx::core::banking::BankingItems offeredAccounts() const;

public Q_SLOTS:
    void showSetupDialog();

private:
    class Private;
    Private *d_ptr = nullptr;
};

} // namespace olbaflinx::ui::assistant::pages
