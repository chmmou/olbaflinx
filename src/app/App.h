/**
 * Copyright (C) 2021, Alexander Saal <alexander.saal@chm-projects.de>
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

#ifndef OLBAFLINX_APP_H
#define OLBAFLINX_APP_H

#include <QtWidgets/QMainWindow>
#include "ui_OlbaFlinx.h"

namespace olbaflinx::app
{

class App: public QMainWindow, private Ui::UiOlbaFlinx
{

Q_OBJECT

public:
    explicit App(QWidget *parent = nullptr, Qt::WindowFlags flags = Qt::WindowFlags());
    ~App() override;

};

}

#endif // OLBAFLINX_APP_H
