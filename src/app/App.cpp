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
#include <QtCore/QDebug>
#include <QtCore/QFileInfo>
#include <QtGui/QKeySequence>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QSizePolicy>

#include "App.h"

#include "app/DataVault/DataVaultItem.h"
#include "app/DataVault/DataVaultDialog.h"
#include "app/Banking/Assistant/SetupAssistant.h"

#include "core/Container.h"
#include "core/Banking/Banking.h"
#include "core/Storage/Storage.h"
#include "core/SingleApplication/SingleApplication.h"

using namespace olbaflinx::app;
using namespace olbaflinx::app::banking;
using namespace olbaflinx::app::banking::assistant;
using namespace olbaflinx::app::datavault;

using namespace olbaflinx::core;
using namespace olbaflinx::core::banking;
using namespace olbaflinx::core::storage;

QLabel *vaultInfoLabel = nullptr;
QSpacerItem *scrollAreaSpacerTop = nullptr;
QSpacerItem *scrollAreaSpacerBottom = nullptr;
QVBoxLayout *scrollAreaDataVaultsContentsLayout = nullptr;

App::App(QWidget *parent, Qt::WindowFlags flags)
    : QMainWindow(parent, flags)
{
    setupUi(this);
}

App::~App() = default;

void App::initialize()
{
    stackedWidgetMain->setCurrentIndex(0);

    bool initialized = Banking::instance()->initialize(
        SingleApplication::applicationName(),
        "3E1B97FF72A24783EC2215B12",
        SingleApplication::applicationVersion()
    );

    if (!initialized) {
        QMessageBox::critical(
            this,
            SingleApplication::applicationName(),
            tr("Banking backend can't be initialized"));
    }

    setupDataVault();
}

void App::closeEvent(QCloseEvent *event)
{
    Banking::instance()->deInitialize();
    QMainWindow::closeEvent(event);
}

void App::setupDataVault()
{
    createVaultInfoLabel();
    createVaultInfoSpacerItems();

    scrollAreaDataVaultsContentsLayout = new QVBoxLayout(scrollAreaDataVaultsContents);

    scrollAreaDataVaults->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollAreaDataVaults->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollAreaDataVaults->setFrameShape(QScrollArea::NoFrame);
    scrollAreaDataVaults->setFrameShadow(QScrollArea::Plain);

    pushButtonAddDataVaults->setShortcut(QKeySequence("Ctrl+N"));

    connect(
        pushButtonAddDataVaults,
        &QPushButton::clicked,
        this,
        &App::connectDataVaultForAdding
    );

    auto storageFiles = Storage::instance()->setting(
        StorageSettingGroup,
        StorageSettingGroupKey,
        QStringList()
    ).toStringList();

    if (storageFiles.isEmpty()) {
        removeVaultInfo();
        addVaultInfo();
    }
    else {
        removeVaultInfo();

        for (const auto &file: qAsConst(storageFiles)) {
            addDataVault(QFileInfo(file).baseName(), file);
        }

        scrollAreaDataVaultsContentsLayout->addItem(scrollAreaSpacerBottom);
        scrollAreaDataVaultsContentsLayout->update();

        storageFiles.clear();
    }
}

void App::addDataVault(const QString &title, const QString &fileName)
{
    auto dataVaultItem = new DataVaultItem();
    dataVaultItem->setVaultTitle(title);
    dataVaultItem->setVaultFilePath(fileName);

    QFileInfo fi(fileName);
    const QString vaultItemCreationDateTimeString = fi.lastModified().toString("dd.MM.yyyy hh:mm");
    const QString vaultItemMessageString = tr("Created on %1").arg(vaultItemCreationDateTimeString);

    dataVaultItem->setVaultFileInfo(vaultItemMessageString);

    connectDataVaultItemForOpen(dataVaultItem);
    connectDataVaultItemForDeletion(dataVaultItem);

    scrollAreaDataVaultsContentsLayout->addWidget(dataVaultItem);
}

void App::createVaultInfoLabel()
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
}

void App::createVaultInfoSpacerItems()
{
    scrollAreaSpacerTop = new QSpacerItem(1, 1, QSizePolicy::Fixed, QSizePolicy::Expanding);
    scrollAreaSpacerBottom = new QSpacerItem(1, 1, QSizePolicy::Fixed, QSizePolicy::Expanding);
}

void App::addVaultInfo()
{
    scrollAreaDataVaultsContentsLayout->addItem(scrollAreaSpacerTop);
    scrollAreaDataVaultsContentsLayout->addWidget(vaultInfoLabel);
    scrollAreaDataVaultsContentsLayout->addItem(scrollAreaSpacerBottom);
    scrollAreaDataVaultsContentsLayout->update();
    scrollAreaDataVaultsContents->update();
    scrollAreaDataVaults->update();
}

void App::removeVaultInfo()
{
    int indexOf = scrollAreaDataVaultsContentsLayout->indexOf(scrollAreaSpacerTop);
    if (indexOf >= 0) {
        auto item = scrollAreaDataVaultsContentsLayout->takeAt(indexOf);
        delete item;

        scrollAreaSpacerTop = new QSpacerItem(1, 1, QSizePolicy::Fixed, QSizePolicy::Expanding);
    }

    indexOf = scrollAreaDataVaultsContentsLayout->indexOf(vaultInfoLabel);
    if (indexOf >= 0) {
        auto item = scrollAreaDataVaultsContentsLayout->takeAt(indexOf);
        delete item->widget();
        delete item;

        createVaultInfoLabel();
    }

    indexOf = scrollAreaDataVaultsContentsLayout->indexOf(scrollAreaSpacerBottom);
    if (indexOf >= 0) {
        auto item = scrollAreaDataVaultsContentsLayout->takeAt(indexOf);
        delete item;

        scrollAreaSpacerBottom = new QSpacerItem(1, 1, QSizePolicy::Fixed, QSizePolicy::Expanding);
    }

    scrollAreaDataVaultsContentsLayout->update();
    scrollAreaDataVaultsContents->update();
    scrollAreaDataVaults->update();
}

void App::connectDataVaultForAdding()
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

        disconnect(Storage::instance(), &Storage::userChanged, nullptr, nullptr);
        connect(
            Storage::instance(),
            &Storage::userChanged,
            [newDataVaultDlg, this](const StorageUser *user)
            {
                if (!Storage::instance()->isInitialized()) {
                    Storage::instance()->initialize();
                }

                removeVaultInfo();

                addDataVault(newDataVaultDlg->vaultName(), user->filePath());

                scrollAreaDataVaultsContentsLayout->addItem(scrollAreaSpacerBottom);
                scrollAreaDataVaultsContentsLayout->update();
                scrollAreaDataVaultsContents->update();
                scrollAreaDataVaults->update();
            }
        );

        StorageUser storageUser;
        storageUser.setPassword(storagePassword);
        storageUser.setFilePath(storageFile);
        Storage::instance()->setUser(&storageUser);
    }
    newDataVaultDlg->deleteLater();
}

void App::connectDataVaultItemForOpen(const DataVaultItem *item)
{
    disconnect(item, &DataVaultItem::vaultOpened, nullptr, nullptr);
    connect(
        item,
        &DataVaultItem::vaultOpened,
        [=](const QString &filePath, const QString &password)
        {
            disconnect(Storage::instance(), &Storage::errorOccurred, nullptr, nullptr);
            disconnect(Storage::instance(), &Storage::userChanged, nullptr, nullptr);
            disconnect(Storage::instance(), &Storage::accountReceived, nullptr, nullptr);

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

                    connect(
                        Storage::instance(),
                        &Storage::accountReceived,
                        [=](const AccountList &accounts)
                        {
                            if (!accounts.isEmpty()) {
                                treeWidgetAccounts->setAccounts(accounts);
                                return;
                            }

                            const auto setupAssistant = new SetupAssistant();
                            setupAssistant->exec();
                            setupAssistant->deleteLater();
                        }
                    );

                    stackedWidgetMain->setCurrentIndex(1);
                    Storage::instance()->receiveAccounts();
                }
            );

            StorageUser storageUser;
            storageUser.setPassword(password);
            storageUser.setFilePath(filePath);
            Storage::instance()->setUser(&storageUser);
        }
    );
}

void App::connectDataVaultItemForDeletion(const DataVaultItem *item)
{
    disconnect(item, &DataVaultItem::vaultDeleted, nullptr, nullptr);
    connect(
        item,
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
                    removeVaultInfo();
                    addVaultInfo();
                }

                scrollAreaDataVaultsContentsLayout->removeWidget(item);
                item->deleteLater();
                storageFiles.clear();
            }
        }
    );
}
