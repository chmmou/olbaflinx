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
 * @brief The application details core needs for its settings and for signing on
 *  to the banking backend.
 *
 * The values are passed in instead of being read from the running application
 * instance. Only that way can core be built and tested without a
 * QCoreApplication. Ownership: a value type, not a QObject, it is copied.
 */
struct OLBAFLINX_CORE_EXPORT ApplicationInfo
{
    QString organization;
    QString name;
    QString version;
};

} // namespace olbaflinx::core
