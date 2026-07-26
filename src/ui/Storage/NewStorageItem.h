/**
 * Copyright (C) 2022-2025, Alexander Saal <developer@olbaflinx.chm-projects.de>
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

namespace olbaflinx::ui::storage {

class NewStorageItem : public QWidget
{
    Q_OBJECT

public:
    explicit NewStorageItem(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
    ~NewStorageItem() override;

    void setTitle(const QString &title) const;
    void setFileInfo(const QString &info) const;
    void setFilePath(const QString &filePath) const;

    QString filePath() const;

Q_SIGNALS:
    void storageOpened(const QString &filePath, const QString &password);
    void storageDeleted(bool success, NewStorageItem *item, const QString &errorMessage);

protected Q_SLOTS:
    void showMenu();
    void openVault();

private Q_SLOTS:
    void showPasswordChangeDialog();
    void deleteStorage();
    void backupStorage();
    void aboutStorage();

private:
    class Private;
    Private *d_ptr;
};

} // namespace olbaflinx::ui::storage
