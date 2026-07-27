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

#include "ui/Storage/NewStorageDialog.h"

#include "ui_NewStorageDialog.h"

using namespace olbaflinx::ui::storage;

class NewStorageDialog::Private
{
public:
    explicit Private(NewStorageDialog *dialog)
        : ui(new Ui::UiNewStorageDialog)
    {
        ui->setupUi(dialog);
    }

    ~Private() { delete ui; }

    Ui::UiNewStorageDialog *ui;
};

NewStorageDialog::NewStorageDialog(QWidget *parent, Qt::WindowFlags f)
    : QDialog(parent, f)
    , d_ptr(new Private(this))
{
}

NewStorageDialog::~NewStorageDialog()
{
    delete d_ptr;
}
