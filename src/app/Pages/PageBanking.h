/**
* Copyright (C) 2022, Alexander Saal <developer@olbaflinx.chm-projects.de>
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

#ifndef OLBAFLINX_PAGEBANKING_H
#define OLBAFLINX_PAGEBANKING_H

#include <QtCore/QScopedPointer>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QWidget>

namespace olbaflinx::app::pages {

class PageBasePrivate;
class PageBanking : public QWidget
{
    Q_OBJECT

public:
    explicit PageBanking(QWidget *parent = Q_NULLPTR);
    ~PageBanking() override;

    void initialize(QMainWindow *mainWindow);

private:
    friend class PageBasePrivate;
    QScopedPointer<PageBasePrivate> d_ptr;

    void initializeMenuBar();
    void initializeToolbar();
};

} // namespace pages

#endif //OLBAFLINX_PAGEBANKING_H