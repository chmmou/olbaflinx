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

#include "ui/App.h"

#include "core/ApplicationInfo.h"
#include "core/Logger/Logger.h"
#include "core/Storage/Storage.h"
#include "ui/Assistant/SetupAssistant.h"
#include "ui/Storage/StorageDialog.h"

#include <QtWidgets/QApplication>

using namespace olbaflinx::core;
using namespace olbaflinx::core::logger;
using namespace olbaflinx::core::storage;

using namespace olbaflinx::ui;
using namespace olbaflinx::ui::storage;

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
    QApplication::setApplicationVersion(QStringLiteral(OLBAFLINX_VERSION));
    QApplication::setOrganizationName("de.chm-projects.olbaflinx");
    QApplication::setOrganizationDomain("https://olbaflinx.chm-projects.de");

    QApplication a(argc, argv);

    QObject::connect(&a, &QApplication::lastWindowClosed, &a, &QApplication::quit);

    const ApplicationInfo applicationInfo{QApplication::organizationName(),
                                          QApplication::applicationName(),
                                          QApplication::applicationVersion()};

    // Logger and Storage live on the stack of main. Their lifetime encloses the
    // one of every window, which leaves exactly one owner.
    Logger logger;
    Storage storage(applicationInfo);

    App app(&logger, &storage);
    app.initialize();
    app.show();

    StorageDialog storageDialog(&storage);
    storageDialog.initialize(&app);
    storageDialog.show();

    assistant::SetupAssistant setup(applicationInfo, &app);
    setup.exec();

    const int result = QApplication::exec();

    storage.close();

    return result;
}
