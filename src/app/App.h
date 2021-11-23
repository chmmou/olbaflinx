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

#ifndef OLBAFLINX_APP_H
#define OLBAFLINX_APP_H

#include <QtWidgets/QMainWindow>

#include "app/DataVault/DataVaultItem.h"

#include "ui_OlbaFlinx.h"

namespace olbaflinx::app
{

using namespace datavault;

class App: public QMainWindow, private Ui::UiOlbaFlinx
{

Q_OBJECT

public:
    explicit App(QWidget *parent = nullptr, Qt::WindowFlags flags = Qt::WindowFlags());
    ~App() override;

    void initialize();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void setupDataVault();
    void addDataVault(const QString &title, const QString &fileName);
    void createVaultInfoLabel();
    void createVaultInfoSpacerItems();
    void addVaultInfo();
    void removeVaultInfo();

private Q_SLOTS:
    void connectDataVaultForAdding();
    void connectDataVaultItemForOpen(const DataVaultItem *item);
    void connectDataVaultItemForDeletion(const DataVaultItem *item);
};

}

#endif // OLBAFLINX_APP_H
