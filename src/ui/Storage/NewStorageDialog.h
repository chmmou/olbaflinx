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

namespace olbaflinx::core::storage {
class Storage;
}

namespace olbaflinx::ui::storage {

/**
 * @brief The dialog that asks for the details of a new storage file.
 *
 * It checks its own input and releases Ok only once name and pass phrase can
 * carry a storage, so that the caller never has to reject after the fact. What
 * was entered is read back with name() and password().
 *
 * Ownership: belongs to its parent widget. The storage is observed only and has
 * to outlive the dialog.
 */
class NewStorageDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @param storage Externally owned storage, asked for the pass phrase rule.
     * @param parent Optional owner.
     * @param f Optional window flags.
     */
    explicit NewStorageDialog(olbaflinx::core::storage::Storage *storage,
                              QWidget *parent = nullptr,
                              Qt::WindowFlags f = Qt::WindowFlags());
    ~NewStorageDialog() override;

    /**
     * @return The name that was entered, the storage file is named after it
     */
    [[nodiscard]] QString name() const;

    /**
     * @return The pass phrase that was entered
     */
    [[nodiscard]] QString password() const;

private:
    class Private;
    Private *d_ptr = nullptr;
};

} // namespace olbaflinx::ui::storage
