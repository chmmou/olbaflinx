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

#include <QtWidgets/QMainWindow>
#include <QtWidgets/QWidget>

namespace olbaflinx::core::storage {
class Storage;
}

namespace olbaflinx::ui::storage {

/**
 * @brief The overview of the storages that have been set up.
 *
 * Ownership: the storage is observed only. It belongs to whoever created the
 * dialog, and that owner closes and releases it.
 */
class StorageDialog : public QWidget
{
    Q_OBJECT

public:
    /**
     * @param storage Externally owned storage, has to outlive the dialog.
     * @param parent Optional owner.
     */
    explicit StorageDialog(olbaflinx::core::storage::Storage *storage, QWidget *parent = nullptr);
    ~StorageDialog() override;

    void initialize(QMainWindow *window);

    /**
     * @brief Reads the list of storages again.
     */
    void reload();

protected:
    void moveEvent(QMoveEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    class Private;
    Private *d_ptr = nullptr;
};

} // namespace olbaflinx::ui::storage
