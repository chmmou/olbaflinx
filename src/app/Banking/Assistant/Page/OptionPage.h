/**
 * Copyright (C) 2021, Alexander Saal <developer@olbaflinx.chm-projects.de>
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

#ifndef OLBAFLINX_OPTIONPAGE_H
#define OLBAFLINX_OPTIONPAGE_H

#include <QtWidgets/QWizardPage>

#include "ui_OptionPage.h"

#include "core/Container.h"

using namespace olbaflinx::core;

namespace olbaflinx::app::banking::assistant::page
{
class OptionPage: public QWizardPage, private Ui::UiOptionPage
{
Q_OBJECT

public:
    explicit OptionPage(QWidget *parent = nullptr);
    ~OptionPage() override;

    void initialize();
    bool isComplete() const override;

    [[nodiscard]] AccountIds selectedAccountIds() const;

private Q_SLOTS:
    void checkForAccounts(const AccountList &accounts);
    void setupAccounts(const AccountList &accounts);

public Q_SLOTS:
    void showSetupDialog();
};
}

#endif // OLBAFLINX_OPTIONPAGE_H
