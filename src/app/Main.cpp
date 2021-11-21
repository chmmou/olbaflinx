/**
 * Copyright (C) 2021, Alexander Saal <developer@olbaflinx.chm-projects.de>
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

#include <QtCore/QTranslator>

#include "app/App.h"
#include "core/SingleApplication/SingleApplication.h"

using namespace olbaflinx::app;

int main(int argc, char *argv[])
{
    SingleApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    SingleApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

#if QT_VERSION > QT_VERSION_CHECK(5, 14, 0)
    SingleApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
#endif

    SingleApplication::setApplicationName("OlbaFlinx");
    SingleApplication::setApplicationVersion("1.0.0");
    SingleApplication::setOrganizationName("de.chm-projects.olbaflinx");
    SingleApplication::setOrganizationDomain("https://olbaflinx.chm-projects.de");

    SingleApplication a(argc, argv);

    if (a.isSecondary()) {
        a.sendMessage(a.arguments().join(' ').toUtf8());
        a.exit(0);
    }

    App *app = new App();
    app->initialize();
    app->setVisible(true);

    const int result = a.exec();

    delete app;

    return result;
}

