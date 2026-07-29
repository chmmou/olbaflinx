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

#include <QtCore/QList>

#include <QtWidgets/QWizardPage>

namespace olbaflinx::ui::assistant::pages {

class OptionBankingPage : public QWizardPage
{
    Q_OBJECT

public:
    explicit OptionBankingPage(QWidget *parent = Q_NULLPTR);
    ~OptionBankingPage() override;

    /**
     * @brief Baut die Verbindung zum Bankbackend auf und liest die Konten.
     *
     * Der Aufbau erfolgt hier und nicht im Konstruktor, weil uic die Seite ohne
     * Argumente erzeugt und die Kenndaten der Anwendung erst danach vorliegen.
     */
    void initialize(const olbaflinx::core::ApplicationInfo &applicationInfo);
    bool isComplete() const override;

    QList<quint32> selectedAccountIds();

public Q_SLOTS:
    void showSetupDialog();

private:
    class Private;
    Private *d_ptr;
};

} // namespace olbaflinx::ui::assistant::pages
