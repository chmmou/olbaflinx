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

#ifndef OLBAFLINX_DATAVAULTDIALOG_H
#define OLBAFLINX_DATAVAULTDIALOG_H

#include <QtWidgets/QDialog>

#include "ui_DataVaultDialog.h"

namespace olbaflinx::app::datavault
{

class DataVaultDialog: public QDialog, private Ui::UiDataVaultDialog
{

Q_OBJECT

public:
    explicit DataVaultDialog(QWidget *parent = nullptr);
    ~DataVaultDialog() override;

    QString vaultName() const;
    QString vaultPassword() const;

public Q_SLOTS:
    void accept() override;

};

}

#endif // OLBAFLINX_DATAVAULTDIALOG_H
