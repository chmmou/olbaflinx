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

#include "core/Storage/Storage.h"

#include "App.h"

using namespace olbaflinx::app;
using namespace olbaflinx::core::storage;

App::App(QWidget *parent, Qt::WindowFlags flags)
    : QMainWindow(parent, flags)
{
    setupUi(this);

    StorageUser user;
    user.filePath = "/home/asaal/olbaflinx.storage";
    user.password = "äöü'tW5@tEV8ź\4eTyC.Q$s;/`kuzk-QC`V";

    Storage::instance()->setUser(&user);
    Storage::instance()->initialize();
    Storage::instance()->isInitialized();
    Storage::instance()->checkIntegrity();
    Storage::instance()->compress();

}

App::~App() = default;
