/**
 * Copyright (C) 2021-2026, Alexander Saal <developer@olbaflinx.chm-projects.de>
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

#include <QtWidgets/QDialog>

namespace olbaflinx::ui::storage {

/**
 * @brief The dialog that asks for the details of a new storage file.
 *
 * Ownership: belongs to its parent widget. It holds the NewStorageItem that
 * carries the input fields and releases it with itself.
 */
class NewStorageDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NewStorageDialog(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
    ~NewStorageDialog() override;

private:
    class Private;
    Private *d_ptr = nullptr;
};

} // namespace olbaflinx::ui::storage
