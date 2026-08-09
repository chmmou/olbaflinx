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
#include <QtWidgets/QTreeView>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE
class QAbstractItemModel;
QT_END_NAMESPACE

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
     * @brief The tree of accounts, grouped by bank.
     *
     * @return The view. Never null once the widget is built.
     */
    [[nodiscard]] QTreeView *accountWidget() const;

    /**
     * @brief Hands the accounts to the view and takes over showing the notices.
     *
     * The view keeps no records of its own, so an empty tree and a tree that was
     * never filled look the same. This is where the difference is told: as long
     * as the model reports no row, the notice from setUpAccountNotice() stands in
     * its place.
     *
     * @param model Externally owned model, has to outlive this widget. Passing
     *  nullptr detaches the view.
     */
    void setAccountModel(QAbstractItemModel *model);

    /**
     * @brief Says that the accounts could not be read.
     *
     * A failed read is not an empty storage. What the view already shows stays
     * where it is; only a view that has nothing to show trades the notice about
     * the missing accounts for this one.
     *
     * @param message What the user gets to see. It names no file and no
     *  statement.
     */
    void showAccountsUnreadable(const QString &message);

private:
    class Private;
    Private *d_ptr = nullptr;
};

} // namespace olbaflinx::ui
