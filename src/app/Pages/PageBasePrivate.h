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

#ifndef OLBAFLINX_PAGEBASEPRIVATE_H
#define OLBAFLINX_PAGEBASEPRIVATE_H

#include "app/App.h"
#include <QtWidgets/QMainWindow>

namespace olbaflinx::app::pages
{
class PageBasePrivate
{
public:
    inline explicit PageBasePrivate()
        : m_app(Q_NULLPTR)
        , m_mainWindow(Q_NULLPTR)
    { }

    inline ~PageBasePrivate()
    {
        m_app = Q_NULLPTR;
        m_mainWindow = Q_NULLPTR;
    }

    inline App *app()
    {
        if (m_app == Q_NULLPTR) {
            m_app = qobject_cast<App *>(m_mainWindow);
        }
        return m_app;
    }

    inline void setMainWindow(QMainWindow *mainWindow) { m_mainWindow = mainWindow; }

private:
    App *m_app;
    QMainWindow *m_mainWindow;
};

} // namespace olbaflinx::app::pages

#endif //OLBAFLINX_PAGEBASEPRIVATE_H
