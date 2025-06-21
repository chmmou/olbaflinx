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

#include "App.h"

#include <QtWidgets/QApplication>

#include "SetupAssistant.h"
#include "Storage/StorageDialog.h"

using namespace olbaflinx::app;
using namespace olbaflinx::app::storage;

int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

#if QT_VERSION > QT_VERSION_CHECK(5, 14, 0)
    QApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
#endif

    QApplication::setApplicationName("OlbaFlinx");
    QApplication::setApplicationVersion("1.0");
    QApplication::setOrganizationName("de.chm-projects.olbaflinx");
    QApplication::setOrganizationDomain("https://olbaflinx.chm-projects.de");

    const QApplication a(argc, argv);

    QObject::connect(&a, &QApplication::lastWindowClosed, &a, &QApplication::quit);

    const auto app = new App();
    app->initialize(&a);
    //app->show();

    const auto storageDialog = new StorageDialog;
    storageDialog->initialize(app);
    storageDialog->show();

    /*const auto setup = new assistant::SetupAssistant(app);
    setup->exec();*/

    const int result = a.exec();

    delete storageDialog;
    delete app;

    return result;
}
