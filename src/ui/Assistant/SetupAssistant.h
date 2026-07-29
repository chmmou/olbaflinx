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

#include "core/ApplicationInfo.h"

#include <QtWidgets/QWizard>

namespace olbaflinx::ui::assistant {

class SetupAssistant : public QWizard
{
    Q_OBJECT

public:
    /**
     * @param applicationInfo Kenndaten, die die Bankseite zum Aufbau braucht.
     * @param parent Optionaler Eigentuemer.
     * @param flags Fensterflaggen.
     */
    explicit SetupAssistant(const olbaflinx::core::ApplicationInfo &applicationInfo,
                            QWidget *parent = Q_NULLPTR,
                            Qt::WindowFlags flags = Qt::WindowFlags());
    ~SetupAssistant() override;

private:
    class Private;
    Private *d_ptr;
};

} // namespace olbaflinx::ui::assistant
