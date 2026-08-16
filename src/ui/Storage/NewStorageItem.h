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

#include <QtWidgets/QWidget>

namespace olbaflinx::core::storage {
class Storage;
}

namespace olbaflinx::ui::storage {

/**
 * @brief One entry in the overview of storages.
 *
 * Ownership: the storage is observed only and belongs to its creator.
 */
class NewStorageItem : public QWidget
{
    Q_OBJECT

public:
    /**
     * @param storage Externally owned storage, has to outlive the entry.
     * @param parent Optional owner.
     * @param f Window flags.
     */
    explicit NewStorageItem(olbaflinx::core::storage::Storage *storage,
                            QWidget *parent = nullptr,
                            Qt::WindowFlags f = Qt::WindowFlags());
    ~NewStorageItem() override;

    /**
     * @brief Names the entry, on screen and towards assistive tools.
     *
     * The entry is a group without a label of its own, so the title is what
     * tells one apart from the next.
     */
    void setTitle(const QString &title);
    void setFileInfo(const QString &info);
    void setFilePath(const QString &filePath);

    [[nodiscard]] QString filePath() const;

Q_SIGNALS:
    void storageOpened(const QString &filePath, const QString &password);
    void storageDeleted(bool success, NewStorageItem *item, const QString &errorMessage);

    /**
     * @brief Something the user needs to read, already worded for him.
     *
     * An entry sits inside a page and has no status bar of its own. The overview
     * passes this on to the window, which is where such a sentence goes.
     *
     * @param message What the user gets to see. It names no path and no password.
     */
    void message(const QString &message);

protected Q_SLOTS:
    void showMenu();
    void openVault();

private Q_SLOTS:
    void showPasswordChangeDialog();
    void deleteStorage();
    void backupStorage();

private:
    class Private;
    Private *d_ptr = nullptr;
};

} // namespace olbaflinx::ui::storage
