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

#include "ui/Logging.h"

// Same as in core: debug output stays off until QT_LOGGING_RULES turns it on.
Q_LOGGING_CATEGORY(lcUi, "olbaflinx.ui", QtInfoMsg)
Q_LOGGING_CATEGORY(lcUiStorage, "olbaflinx.ui.storage", QtInfoMsg)
Q_LOGGING_CATEGORY(lcUiThemes, "olbaflinx.ui.themes", QtInfoMsg)
