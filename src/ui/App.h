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

#include "core/ApplicationInfo.h"
#include "core/Banking/BankingItem.h"
#include "core/Error.h"

#include <QtWidgets/QMainWindow>
#include <QtWidgets/QPushButton>

using namespace olbaflinx::core::banking;

namespace olbaflinx::core::logger {
class Logger;
}

namespace olbaflinx::core::storage {
class Storage;
}

namespace olbaflinx::ui {

/**
 * The main window of the application.
 *
 * Ownership: Logger and Storage are observed only. Their lifetime encloses the
 * one of the window, and the creator releases them.
 */
class App : public QMainWindow
{
    Q_OBJECT

public:
    /**
     * Logger and storage are owned elsewhere and have to outlive the window.
     *
     * The application info is what the window signs on to a bank with. Left
     * out, no fetch comes about and the reason says so; a window that never
     * fetches needs none.
     */
    explicit App(core::logger::Logger *logger,
                 core::storage::Storage *storage,
                 core::ApplicationInfo applicationInfo = {},
                 QWidget *parent = nullptr,
                 const Qt::WindowFlags &flags = Qt::WindowFlags());
    ~App() override;

    void initialize();

    void setAccounts(const BankingItems &items);

    /**
     * Reads the accounts of the open storage again and shows what is
     * there now.
     *
     * For whoever wrote into the storage from outside the window, the wizard
     * above all. The window does not learn of such a write on its own, and the
     * tree would stand as it was until the storage is closed and opened again.
     *
     * The choice of the user and the transactions shown are kept. Does nothing
     * of consequence while no storage is open.
     */
    void refreshAccounts();

    /**
     * Turns an error from core into something the user can act on.
     *
     * The technical message goes to the log and never reaches the screen; the
     * status bar carries the short form. Nothing here is modal; none of these
     * errors blocks the window.
     */
    void showError(core::ErrorCode code, const QString &reason);

    /**
     * Puts a message the caller has already worded into the status bar.
     *
     * showError turns a code into a fixed sentence. This one carries what only
     * the caller knows, such as how many accounts of a run reached the storage.
     * Nothing here is modal. The message must name no account, no balance and
     * no amount.
     */
    void showMessage(const QString &message);

public Q_SLOTS:
    /**
     * Closes the open storage and returns to the overview.
     *
     * Everything the storage brought in goes with it: the window shows the first
     * page again and the models let go of their records. A record left behind
     * would show up under the next storage that is opened.
     *
     * Does nothing when no storage is open. The entry is reachable through its
     * shortcut before one ever was.
     */
    void closeStorage();

Q_SIGNALS:
    /**
     * The user asked for the setup wizard.
     *
     * The window does not know what a wizard needs to be built, so it asks
     * rather than builds. Whoever assembled the application answers, and the
     * accounts come back through setAccounts like any others.
     */
    void setupAssistantRequested();

protected:
    bool event(QEvent *event) override;
    void moveEvent(QMoveEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

    /**
     * Puts off closing while a fetch runs.
     *
     * The application has no way of its own to end a session; the abort runs
     * over the button of the progress dialog the banking layer brings. Ending
     * the process under a running session would reach into objects that are
     * already being taken down.
     */
    void closeEvent(QCloseEvent *event) override;

private:
    class Private;
    Private *d_ptr = nullptr;
};

} // namespace olbaflinx::ui
