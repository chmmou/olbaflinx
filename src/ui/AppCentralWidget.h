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
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QWidget>

namespace olbaflinx::ui {

class AppCentralWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AppCentralWidget(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
    ~AppCentralWidget() override;

    void initialize(QMainWindow *window);

    /**
     * @brief The tree of accounts in the left dock.
     *
     * Answers with nullptr. The dock it belongs to is not built yet; the code
     * that would fill it is commented out in App. Kept rather than removed so
     * that the place it is meant to take stays visible, but every caller has to
     * expect nothing back until the dock exists. It is const while it hands out
     * nothing; a widget meant to be worked on afterwards would not be.
     *
     * @return nullptr.
     */
    [[nodiscard]] QTreeWidget *accountWidget() const;

private:
    class Private;
    Private *d_ptr = nullptr;
};

} // namespace olbaflinx::ui
