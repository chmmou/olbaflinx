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
#ifndef OLBAFLINX_APP_BANKING_PAGE_H
#define OLBAFLINX_APP_BANKING_PAGE_H

#include <QtCore/QList>

#include <QtWidgets/QWizardPage>

namespace olbaflinx::app::assistant::pages {

class OptionBankingPage : public QWizardPage
{
    Q_OBJECT

public:
    explicit OptionBankingPage(QWidget *parent = Q_NULLPTR);
    ~OptionBankingPage() override;

    void initialize();
    bool isComplete() const override;

    QList<quint32> selectedAccountIds();

public Q_SLOTS:
    void showSetupDialog();

private:
    class Private;
    Private *d_ptr;
};

} // namespace olbaflinx::app::assistant::pages

#endif //OLBAFLINX_APP_BANKING_PAGE_H
