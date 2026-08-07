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

/**
 * @brief The central area of the main window.
 *
 * Ownership: belongs to its parent widget, as every QWidget does. The window
 * passed to initialize() is only borrowed, it is not kept beyond the call.
 */
class AppCentralWidget : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief The two pages the central area carries.
     *
     * Storages is what stands there before a storage is open, Banking what comes
     * after it. The numbers are the ones the QStackedWidget counts by.
     */
    enum class Page { Storages = 0, Banking = 1 };
    Q_ENUM(Page)

    explicit AppCentralWidget(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
    ~AppCentralWidget() override;

    void initialize(QMainWindow *window);

    /**
     * @brief Shows one of the two pages. The other one stays built.
     *
     * Closing a storage drops the records of the models, not the widgets of the
     * page that is left.
     */
    void setPage(Page page);

    [[nodiscard]] Page page() const;

    /**
     * @brief Puts the overview of the storages onto the first page.
     *
     * The overview needs the storage of the application and is therefore built
     * by the window, not here. It becomes a child of this widget through the
     * layout of the page.
     *
     * @param overview The widget to show. It is taken over.
     */
    void setStorageOverview(QWidget *overview);

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
