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
#ifndef OLBAFLINX_APP_STORAGE_PAGE_H
#define OLBAFLINX_APP_STORAGE_PAGE_H

#include <QtWidgets/QMainWindow>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QWidget>

namespace olbaflinx::app {

class AppCentralWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AppCentralWidget(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
    ~AppCentralWidget() override;

    void initialize(QMainWindow *window);

    QTreeWidget *accountWidget() const;

private:
    class Private;
    Private *d_ptr;
};

} // namespace olbaflinx::app

#endif //OLBAFLINX_APP_STORAGE_PAGE_H
