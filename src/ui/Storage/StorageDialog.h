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
 * The overview of the storages that have been set up.
 *
 * Ownership: the storage is observed only. It belongs to whoever created the
 * dialog, and that owner closes and releases it.
 */
class StorageDialog : public QWidget
{
    Q_OBJECT

public:
    /**
     * The storage is owned elsewhere and has to outlive the dialog.
     */
    explicit StorageDialog(olbaflinx::core::storage::Storage *storage, QWidget *parent = nullptr);
    ~StorageDialog() override;

    /**
     * Builds the overview and remembers the window it reports to.
     *
     * The accounts of an opened storage go to that window and there is no
     * second way there, so a window of another kind, or none at all, leaves the
     * overview able to create a storage but not to open one. That is said once
     * and refused where it is asked for, rather than taken as a precondition
     * nobody checks.
     */
    void initialize(QMainWindow *window);

    /**
     * Reads the list of storages again and builds the overview from it.
     *
     * Entries whose file is gone are dropped from the list on the way.
     */
    void reload();

    /**
     * The name a new storage can be created under.
     *
     * A name that is already taken gets a number behind a hyphen, and the number
     * grows for as long as the name it forms is taken as well. The message that
     * announces the conflict and the call that creates the storage ask this same
     * function, so that the storage carries the name the user was shown.
     *
     * Answers with the name itself when it is free, otherwise with the first
     * free variant.
     */
    [[nodiscard]] QString availableName(const QString &name) const;

    /**
     * Creates a storage file for name and puts it into the overview.
     *
     * The file is written right away, not on the first time it is opened. An
     * entry without a file would disappear again the next time the overview is
     * built.
     *
     * The name has to be a free one, as availableName() answers it. True comes
     * back when the file was created and the list was written.
     */
    bool createStorage(const QString &name, const QString &password);

public Q_SLOTS:
    /**
     * Asks the user for a name and a password and creates a storage.
     *
     * Reachable from the window as well, because the menu carries the same
     * command as the button in the overview.
     */
    void addStorage();

Q_SIGNALS:
    /**
     * A storage was opened and its accounts are on their way.
     *
     * The window listens for this and turns to the page that shows them. The
     * overview itself does not switch pages; it does not own the window it sits
     * in.
     */
    void storageOpened();

    /**
     * Something the user needs to read, already worded for him.
     *
     * The overview is a page and has no status bar of its own. It says what
     * happened and lets the window decide where that goes. A signal rather than
     * a call into the window, because createStorage() is reachable without one.
     * The message names no path and no password.
     */
    void message(const QString &message);

protected:
    /**
     * Sizes that derive from the font are computed again when the font changes.
     * Set once at construction they would hold a measure of a font nobody uses
     * any more.
     */
    void changeEvent(QEvent *event) override;

private:
    class Private;
    Private *d_ptr = nullptr;
};

} // namespace olbaflinx::ui::storage
