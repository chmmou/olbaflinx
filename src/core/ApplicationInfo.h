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

#include "core/OlbaFlinxCore.h"

#include <QtCore/QString>

namespace olbaflinx::core {

/**
 * @brief Die Kenndaten der Anwendung, die core fuer Einstellungen und fuer die
 *  Anmeldung am Bankbackend braucht.
 *
 * Die Werte werden uebergeben, statt sie aus der laufenden Anwendungsinstanz zu
 * lesen. Nur so laesst sich core ohne QCoreApplication aufbauen und pruefen.
 * Eigentum: Wertetyp, kein QObject, wird kopiert.
 */
struct OLBAFLINX_CORE_EXPORT ApplicationInfo
{
    QString organization;
    QString name;
    QString version;
};

} // namespace olbaflinx::core
