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
 * The FinTS registration key of this product. It names the application to the
 * bank servers; it authenticates no user and grants access to no account, so it
 * is not a secret and losing it costs nothing but the identification.
 *
 * A key in the source is normally forbidden without qualification. This one is
 * kept there on purpose, because moving a value that identifies the build into
 * a build time variable would hide it without protecting anything. The value
 * has been public in this repository since it was first committed; taking it
 * out would not make it secret again.
 */
inline constexpr auto FinTsRegistrationKey = QLatin1StringView("3E1B97FF72A24783EC2215B12");

/**
 * The application details core needs for its settings and for signing on
 * to the banking backend.
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

    /**
     * The key the application signs on to a bank with, FinTsRegistrationKey
     * above. It is a field rather than a constant read where it is needed,
     * because a test signs on under a key of its own.
     *
     * An aggregate that names only the three fields above leaves this one empty
     * without a word from any compiler, and the application would then reach a
     * bank without identifying itself. Whoever builds one names all four.
     */
    QString registrationKey;
};

} // namespace olbaflinx::core
