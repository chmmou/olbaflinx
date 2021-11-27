/**
 * Copyright (c) 2021, Alexander Saal <developer@olbaflinx.chm-projects.de>
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
#ifndef OLBAFLINX_SETUPASSISTANT_H
#define OLBAFLINX_SETUPASSISTANT_H

#include <QtCore/QList>
#include <QtWidgets/QWizard>

#include "ui_SetupAssistant.h"

namespace olbaflinx::app::banking::assistant
{

class SetupAssistant: public QWizard, private Ui::UiSetupAssistant
{
Q_OBJECT

public:
    explicit SetupAssistant(QWidget *parent = nullptr);
    ~SetupAssistant() override;

protected:
    void done(int result) override;
};

} // olbaflinx::app::banking

#endif // OLBAFLINX_SETUPASSISTANT_H
