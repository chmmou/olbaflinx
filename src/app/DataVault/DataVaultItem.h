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

#ifndef OLBAFLINX_DATAVAULTITEM_H
#define OLBAFLINX_DATAVAULTITEM_H

#include <QtWidgets/QWidget>

#include "ui_DataVaultItem.h"

namespace olbaflinx::app::datavault
{

class DataVaultItem: public QWidget, private Ui::UiDataVaultItem
{

Q_OBJECT

public:
    explicit DataVaultItem(QWidget *parent = nullptr);
    ~DataVaultItem() override;

    void setVaultTitle(const QString &title) const;
    void setVaultFilePath(const QString &filePath) const;
    QString vaultFilePath() const;

Q_SIGNALS:
    void vaultOpened(const QString &filePath, const QString &password);
    void vaultDeleted(bool success, DataVaultItem *item, const QString &errorMessage);

protected Q_SLOTS:
    void showMenu();
    void openVault();

private Q_SLOTS:
    void showPasswordChangeDialog();
    void deleteDataVault();

};

}

#endif // OLBAFLINX_DATAVAULTITEM_H
