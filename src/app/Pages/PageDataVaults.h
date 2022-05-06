/**
* Copyright (C) 2022, Alexander Saal <developer@olbaflinx.chm-projects.de>
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

#ifndef OLBAFLINX_PAGEDATAVAULTS_H
#define OLBAFLINX_PAGEDATAVAULTS_H

#include <QtWidgets/QLabel>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

#include "app/DataVault/DataVaultItem.h"

namespace olbaflinx::app::pages {

using namespace olbaflinx::app::datavault;

class PageBasePrivate;
class PageDataVaults : public QWidget
{
    Q_OBJECT

public:
    explicit PageDataVaults(QWidget *parent = Q_NULLPTR);
    ~PageDataVaults() override;

    void initialize(QMainWindow *mainWindow);

private Q_SLOTS:
    void addNewDataVault();

Q_SIGNALS:
    void vaultOpen();

private:
    void loadVaults();
    void addDataVault(const QString &title, const QString &fileName);
    void addVaultInfo();
    void removeVaultInfo();
    void createVaultInfoLabel();
    void createVaultInfoSpacerItems();

    void openDataVaultItem(const QString &filePath, const QString &password);
    void deleteDataVaultItem(bool success, DataVaultItem *item, const QString &errorMessage);

private:
    friend class PageBasePrivate;
    QScopedPointer<PageBasePrivate> d_ptr;

    QLabel *m_vaultInfoLabel;
    QSpacerItem *m_scrollAreaSpacerTop;
    QSpacerItem *m_scrollAreaSpacerBottom;
    QVBoxLayout *m_scrollAreaDataVaultsContentsLayout;
};

} // namespace olbaflinx::app::pages

#endif //OLBAFLINX_PAGEDATAVAULTS_H
