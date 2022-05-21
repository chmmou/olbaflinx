/**
 * Original code from
 * https://github.com/sthlm58/QtMaterialDesignIcons
 *
 * Copyright (C) 2021-2022, Alexander Saal <developer@olbaflinx.chm-projects.de>
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

#ifndef OLBAFLINX_MATERIALDESIGN_H
#define OLBAFLINX_MATERIALDESIGN_H

#include <QtCore/QSize>
#include <QtCore/QString>
#include <QtGui/QColor>
#include <QtGui/QPixmap>

namespace olbaflinx::core::material::design {

class MaterialDesign
{
public:
    static QPixmap icon(const QString &name,
                        const QSize &size = QSize(24, 24),
                        const QColor &color = Qt::darkGray);
};

} // namespace olbaflinx::core::material::design

#endif //OLBAFLINX_MATERIALDESIGN_H
