/**
* Copyright (c) 2022, Alexander Saal <developer@olbaflinx.chm-projects.de>
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
#ifndef OLBAFLINX_IMEXPORTASSISTANT_H
#define OLBAFLINX_IMEXPORTASSISTANT_H

#include <QtWidgets/QWizard>

#include "core/Container.h"
#include "ui_ImExportAssistant.h"

namespace olbaflinx::app::assistant {

using namespace olbaflinx::core;

class ImExportAssistant : public QWizard, private Ui::UiImExportAssistant
{
    Q_OBJECT

public:
    explicit ImExportAssistant(QWidget *parent = nullptr);
    ~ImExportAssistant() override;

    bool validateCurrentPage() override;
    void setAccountId(quint32 id);

protected:
    void done(int result) override;

private Q_SLOTS:
    void comboboxIndexChanged(int index);
    void openImExportFile();
    void imExportProgress(qreal progress);

private:
    ImExportProfileList m_imExportProfileList;
    QVector<int> m_metaTypeIds;
    quint32 m_accountId;
};

} // namespace olbaflinx::app::assistant

#endif //OLBAFLINX_IMEXPORTASSISTANT_H
