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
     * @brief Reads the list of storages again and builds the overview from it.
     *
     * Entries whose file is gone are dropped from the list on the way.
     */
    void reload();

    /**
     * @brief The name a new storage can be created under.
     *
     * A name that is already taken gets a number behind a hyphen, and the number
     * grows for as long as the name it forms is taken as well. The message that
     * announces the conflict and the call that creates the storage ask this same
     * function, so that the storage carries the name the user was shown.
     *
     * @param name The name that was entered
     * @return name itself when it is free, otherwise the first free variant
     */
    [[nodiscard]] QString availableName(const QString &name) const;

    /**
     * @brief Creates a storage file for name and puts it into the overview.
     *
     * The file is written right away, not on the first time it is opened. An
     * entry without a file would disappear again the next time the overview is
     * built.
     *
     * @param name A free name, as availableName() answers it
     * @param password The pass phrase the file is encrypted with
     * @return true when the file was created and the list was written
     */
    bool createStorage(const QString &name, const QString &password);

protected:
    /**
     * Sizes that derive from the font are computed again when the font changes.
     * They used to be set once at construction and never revisited.
     */
    void changeEvent(QEvent *event) override;

private:
    class Private;
    Private *d_ptr = nullptr;
};

} // namespace olbaflinx::ui::storage
