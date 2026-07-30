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

#include "core/Banking/BankingItem.h"

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
 * @brief The main window of the application.
 *
 * Ownership: Logger and Storage are observed only. Their lifetime encloses the
 * one of the window, and the creator releases them.
 */
class App : public QMainWindow
{
    Q_OBJECT

public:
    /**
     * @param logger Externally owned logger, has to outlive the window.
     * @param storage Externally owned storage, has to outlive the window.
     * @param parent Optional owner.
     * @param flags Window flags.
     */
    explicit App(core::logger::Logger *logger,
                 core::storage::Storage *storage,
                 QWidget *parent = nullptr,
                 const Qt::WindowFlags &flags = Qt::WindowFlags());
    ~App() override;

    void initialize();

    void setAccounts(const BankingItems &items);

protected:
    bool event(QEvent *event) override;
    void moveEvent(QMoveEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    class Private;
    Private *d_ptr = nullptr; ///< private data (pimpl)
};

} // namespace olbaflinx::ui
