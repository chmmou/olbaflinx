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

#include "core/Error.h"

#include <QtCore/QString>

namespace olbaflinx::ui {

/**
 * Turns an error code from core into a sentence the user can act on.
 *
 * The technical message that comes with the code belongs in the log. It can name
 * a file path or an SQL statement, neither of which is of any use on screen.
 *
 * What comes back says what failed and what the user can do about it.
 */
[[nodiscard]] QString userMessage(core::ErrorCode code);

} // namespace olbaflinx::ui
