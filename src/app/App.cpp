/**
 * Copyright (C) 2021, Alexander Saal <developer@olbaflinx.chm-projects.de>
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

#include <QtCore/QFileInfo>
#include <QtGui/QKeySequence>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QSizePolicy>

#include "App.h"

#include "DataVault/DataVaultItem.h"
#include "DataVault/DataVaultDialog.h"

#include "core/Container.h"
#include "core/Storage/Storage.h"

using namespace olbaflinx::app;
using namespace olbaflinx::app::datavault;

using namespace olbaflinx::core;
using namespace olbaflinx::core::storage;

QLabel *vaultInfoLabel = nullptr;

QSpacerItem *scrollAreaSpacer = nullptr;

App::App(QWidget *parent, Qt::WindowFlags flags)
    : QMainWindow(parent, flags)
{
    setupUi(this);

    stackedWidgetMain->setCurrentIndex(0);

    setupDataVault();
}

App::~App() = default;

void App::setupDataVault()
{
    vaultInfoLabel = new QLabel();
    vaultInfoLabel->setTextFormat(Qt::RichText);
    vaultInfoLabel->setAlignment(Qt::AlignCenter);
    vaultInfoLabel->setText(
        tr(
            "<h1>Welcome to OlbaFlinx</h1>"
            "<p>Click the plus sign or type Ctrl+N to create a new data vault.</p>"
            "<p>You can create as many vaults as you like, each with its own password, e.g. for different user</p>"
        )
    );

    scrollAreaSpacer = new QSpacerItem(1, 1, QSizePolicy::Fixed, QSizePolicy::Expanding);

    scrollAreaDataVaults->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollAreaDataVaults->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollAreaDataVaults->setFrameShape(QScrollArea::NoFrame);
    scrollAreaDataVaults->setFrameShadow(QScrollArea::Plain);
    scrollAreaDataVaults->setWidgetResizable(true);

    scrollAreaDataVaultsContents->setLayout(new QVBoxLayout());

    pushButtonAddDataVaults->setShortcut(QKeySequence("Ctrl+N"));

    connect(
        pushButtonAddDataVaults,
        &QPushButton::clicked,
        [=]()
        {
            auto newDataVaultDlg = new DataVaultDialog(this);
            if (newDataVaultDlg->exec() == DataVaultDialog::Accepted) {

                const auto storageFile = QString("%1/%2.olbaflinx").arg(
                    Storage::instance()->storagePath(),
                    newDataVaultDlg->vaultName()
                );

                const auto storagePassword = newDataVaultDlg->vaultPassword();
                auto storageFiles = Storage::instance()->setting(
                    StorageSettingGroup,
                    StorageSettingGroupKey,
                    QStringList()
                ).toStringList();

                storageFiles << storageFile;

                Storage::instance()->storeSetting(
                    StorageSettingGroup,
                    storageFiles,
                    StorageSettingGroupKey
                );

                StorageUser storageUser;
                storageUser.setPassword(storagePassword);
                storageUser.setFilePath(storageFile);

                Storage::instance()->setUser(&storageUser);
                if (!Storage::instance()->isInitialized()) {
                    Storage::instance()->initialize();
                }

                scrollAreaDataVaultsContents->layout()->removeItem(scrollAreaSpacer);
                scrollAreaDataVaultsContents->layout()->removeWidget(vaultInfoLabel);

                addDataVault(newDataVaultDlg->vaultName(), storageFile);

            }
            newDataVaultDlg->deleteLater();
        }
    );

    auto storageFiles = Storage::instance()->setting(
        StorageSettingGroup,
        StorageSettingGroupKey,
        QStringList()
    ).toStringList();

    if (storageFiles.isEmpty()) {
        scrollAreaDataVaultsContents->layout()->removeItem(scrollAreaSpacer);
        scrollAreaDataVaultsContents->layout()->addWidget(vaultInfoLabel);
    }
    else {
        scrollAreaDataVaultsContents->layout()->removeItem(scrollAreaSpacer);
        scrollAreaDataVaultsContents->layout()->removeWidget(vaultInfoLabel);
    }

    for (const auto &file: qAsConst(storageFiles)) {
        addDataVault(QFileInfo(file).baseName(), file);
    }

    storageFiles.clear();

}

void App::addDataVault(const QString &title, const QString &fileName)
{
    auto dataVault = new DataVaultItem();
    dataVault->setVaultTitle(title);
    dataVault->setVaultFilePath(fileName);

    connect(
        dataVault,
        &DataVaultItem::vaultOpened,
        [=](const QString &filePath, const QString &password)
        {
            disconnect(Storage::instance(), &Storage::errorOccurred, nullptr, nullptr);
            disconnect(Storage::instance(), &Storage::userChanged, nullptr, nullptr);

            connect(
                Storage::instance(),
                &Storage::errorOccurred,
                [=](const QString &message, const Storage::ErrorType errorType)
                {
                    if (errorType == Storage::PasswordError) {
                        QMessageBox::critical(
                            this,
                            tr("Data Vault"),
                            message
                        );
                    }
                }
            );

            connect(
                Storage::instance(),
                &Storage::userChanged,
                [=](const StorageUser *)
                {
                    if (!Storage::instance()->isInitialized()) {
                        QMessageBox::critical(
                            this,
                            tr("Error"),
                            tr("Your data vault is corrupted and or not readable.")
                        );
                        return;
                    }

                    stackedWidgetMain->setCurrentIndex(1);
                }
            );

            StorageUser storageUser;
            storageUser.setPassword(password);
            storageUser.setFilePath(filePath);
            Storage::instance()->setUser(&storageUser);
        }
    );

    connect(
        dataVault,
        &DataVaultItem::vaultDeleted,
        [=](bool success, DataVaultItem *item)
        {
            if (success) {

                auto storageFiles = Storage::instance()->setting(
                    StorageSettingGroup,
                    StorageSettingGroupKey,
                    QStringList()
                ).toStringList();

                if (storageFiles.contains(item->vaultFilePath(), Qt::CaseSensitive)) {
                    storageFiles.removeOne(item->vaultFilePath());
                }

                Storage::instance()->storeSetting(
                    StorageSettingGroup,
                    storageFiles,
                    StorageSettingGroupKey
                );

                if (storageFiles.isEmpty()) {
                    scrollAreaDataVaultsContents->layout()->removeWidget(vaultInfoLabel);
                    scrollAreaDataVaultsContents->layout()->removeItem(scrollAreaSpacer);
                    scrollAreaDataVaultsContents->layout()->addWidget(vaultInfoLabel);
                }

                item->deleteLater();
                scrollAreaDataVaultsContents->update();
                storageFiles.clear();
            }
        }
    );

    scrollAreaDataVaultsContents->layout()->addWidget(dataVault);
    scrollAreaDataVaultsContents->layout()->addItem(scrollAreaSpacer);
}
