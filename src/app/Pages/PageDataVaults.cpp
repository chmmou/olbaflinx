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

#include <QtConcurrent/QtConcurrent>
#include <QtCore/QDate>
#include <QtCore/QFileInfo>
#include <QtWidgets/QMessageBox>

#include "app/App.h"
#include "app/Banking/Assistant/SetupAssistant.h"
#include "app/DataVault/DataVaultDialog.h"

#include "core/Container.h"
#include "core/Storage/VaultStorage.h"

#include "PageBasePrivate.h"
#include "PageDataVaults.h"

using namespace olbaflinx::core::storage;
using namespace olbaflinx::app;
using namespace olbaflinx::app::pages;
using namespace olbaflinx::app::banking::assistant;

PageDataVaults::PageDataVaults(QWidget *parent)
    : QWidget(parent)
    , d_ptr(new PageBasePrivate())
    , m_vaultInfoLabel(Q_NULLPTR)
    , m_scrollAreaSpacerTop(Q_NULLPTR)
    , m_scrollAreaSpacerBottom(Q_NULLPTR)
    , m_scrollAreaDataVaultsContentsLayout(Q_NULLPTR)
{}

PageDataVaults::~PageDataVaults() = default;

void PageDataVaults::initialize(QMainWindow *mainWindow)
{
    d_ptr->setMainWindow(mainWindow);

    createVaultInfoLabel();
    createVaultInfoSpacerItems();

    const auto app = d_ptr->app();
    m_scrollAreaDataVaultsContentsLayout = new QVBoxLayout(app->scrollAreaDataVaultsContents);

    app->scrollAreaDataVaults->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    app->scrollAreaDataVaults->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    app->scrollAreaDataVaults->setFrameShape(QScrollArea::NoFrame);
    app->scrollAreaDataVaults->setFrameShadow(QScrollArea::Plain);

    app->pushButtonAddNewDataVaults->setShortcut(QKeySequence("Ctrl+N"));

    connect(app->pushButtonAddNewDataVaults,
            &QPushButton::clicked,
            this,
            &PageDataVaults::addNewDataVault);

    loadVaults();
}

void PageDataVaults::addNewDataVault()
{
    auto newDataVaultDlg = new DataVaultDialog;
    if (newDataVaultDlg->exec() == DataVaultDialog::Accepted) {
        const auto storageFile = QString("%1/%2.obfx")
                                     .arg(VaultStorage::instance()->storagePath(),
                                          newDataVaultDlg->vaultName());

        const auto storagePassword = newDataVaultDlg->vaultPassword();
        auto storageFiles = VaultStorage::instance()
                                ->setting(StorageSettingGroupKey, StorageSettingGroup, QStringList())
                                .toStringList();

        storageFiles << storageFile;

        VaultStorage::instance()->storeSetting(StorageSettingGroupKey,
                                               storageFiles,
                                               StorageSettingGroup);
        removeVaultInfo();

        addDataVault(newDataVaultDlg->vaultName(), storageFile);

        m_scrollAreaDataVaultsContentsLayout->addItem(m_scrollAreaSpacerBottom);
        m_scrollAreaDataVaultsContentsLayout->update();
        d_ptr->app()->scrollAreaDataVaultsContents->update();
        d_ptr->app()->scrollAreaDataVaults->update();

        qApp->setOverrideCursor(Qt::WaitCursor);

        QEventLoop loop(this);
        QFutureWatcher<void> storageWatcher(this);
        connect(&storageWatcher, &QFutureWatcher<void>::finished, &loop, [&]() {
            loop.quit();
            storageWatcher.cancel();
            storageWatcher.waitForFinished();

            qApp->restoreOverrideCursor();
        });

        storageWatcher.setFuture(QtConcurrent::run([storageFile, storagePassword]() -> void {
            VaultStorage::instance()->setDatabaseKey(storageFile, storagePassword);
            VaultStorage::instance()->initialize(true);
            VaultStorage::instance()->close();
        }));
        loop.exec();
    }
    newDataVaultDlg->deleteLater();
}

void PageDataVaults::loadVaults()
{
    auto storageFiles = VaultStorage::instance()
                            ->setting(StorageSettingGroupKey, StorageSettingGroup, QStringList())
                            .toStringList();

    if (storageFiles.isEmpty()) {
        removeVaultInfo();
        addVaultInfo();
    } else {
        removeVaultInfo();

        for (const auto &file : qAsConst(storageFiles)) {
            addDataVault(QFileInfo(file).baseName(), file);
        }

        m_scrollAreaDataVaultsContentsLayout->addItem(m_scrollAreaSpacerBottom);
        m_scrollAreaDataVaultsContentsLayout->update();

        storageFiles.clear();
    }
}

void PageDataVaults::addDataVault(const QString &title, const QString &fileName)
{
    auto dataVaultItem = new DataVaultItem();
    dataVaultItem->setVaultTitle(title);
    dataVaultItem->setVaultFilePath(fileName);

    QFileInfo fi(fileName);
    QString lastModifiedDateTimeString = fi.lastModified().toString(DateTimeFormat);
    if (lastModifiedDateTimeString.isEmpty()) {
        lastModifiedDateTimeString = QDateTime::currentDateTime().toString(DateTimeFormat);
    }
    const QString vaultItemMessageString = tr("Created on %1").arg(lastModifiedDateTimeString);

    dataVaultItem->setVaultFileInfo(vaultItemMessageString);

    disconnect(dataVaultItem, &DataVaultItem::vaultOpened, nullptr, nullptr);
    disconnect(dataVaultItem, &DataVaultItem::vaultDeleted, nullptr, nullptr);

    connect(dataVaultItem, &DataVaultItem::vaultOpened, this, &PageDataVaults::openDataVaultItem);
    connect(dataVaultItem, &DataVaultItem::vaultDeleted, this, &PageDataVaults::deleteDataVaultItem);

    m_scrollAreaDataVaultsContentsLayout->addWidget(dataVaultItem);
}

void PageDataVaults::addVaultInfo()
{
    m_scrollAreaDataVaultsContentsLayout->addItem(m_scrollAreaSpacerTop);
    m_scrollAreaDataVaultsContentsLayout->addWidget(m_vaultInfoLabel);
    m_scrollAreaDataVaultsContentsLayout->addItem(m_scrollAreaSpacerBottom);
    m_scrollAreaDataVaultsContentsLayout->update();
    d_ptr->app()->scrollAreaDataVaultsContents->update();
    d_ptr->app()->scrollAreaDataVaults->update();
}

void PageDataVaults::removeVaultInfo()
{
    int indexOf = m_scrollAreaDataVaultsContentsLayout->indexOf(m_scrollAreaSpacerTop);
    if (indexOf >= 0) {
        auto item = m_scrollAreaDataVaultsContentsLayout->takeAt(indexOf);
        delete item;

        m_scrollAreaSpacerTop = new QSpacerItem(1, 1, QSizePolicy::Fixed, QSizePolicy::Expanding);
    }

    indexOf = m_scrollAreaDataVaultsContentsLayout->indexOf(m_vaultInfoLabel);
    if (indexOf >= 0) {
        auto item = m_scrollAreaDataVaultsContentsLayout->takeAt(indexOf);
        delete item->widget();
        delete item;

        createVaultInfoLabel();
    }

    indexOf = m_scrollAreaDataVaultsContentsLayout->indexOf(m_scrollAreaSpacerBottom);
    if (indexOf >= 0) {
        auto item = m_scrollAreaDataVaultsContentsLayout->takeAt(indexOf);
        delete item;

        m_scrollAreaSpacerBottom = new QSpacerItem(1, 1, QSizePolicy::Fixed, QSizePolicy::Expanding);
    }

    m_scrollAreaDataVaultsContentsLayout->update();
    d_ptr->app()->scrollAreaDataVaultsContents->update();
    d_ptr->app()->scrollAreaDataVaults->update();
}

void PageDataVaults::createVaultInfoLabel()
{
    m_vaultInfoLabel = new QLabel();
    m_vaultInfoLabel->setTextFormat(Qt::RichText);
    m_vaultInfoLabel->setAlignment(Qt::AlignCenter);
    m_vaultInfoLabel->setText(
        tr("<h1>Welcome to OlbaFlinx</h1>"
           "<p>Click the plus sign or type Ctrl+N to create a new data vault.</p>"
           "<p>You can create as many vaults as you like, each with its own password, e.g. for "
           "different user</p>"));
}

void PageDataVaults::createVaultInfoSpacerItems()
{
    m_scrollAreaSpacerTop = new QSpacerItem(1, 1, QSizePolicy::Fixed, QSizePolicy::Expanding);
    m_scrollAreaSpacerBottom = new QSpacerItem(1, 1, QSizePolicy::Fixed, QSizePolicy::Expanding);
}

void PageDataVaults::openDataVaultItem(const QString &filePath, const QString &password)
{
    VaultStorage::instance()->setDatabaseKey(filePath, password);
    VaultStorage::instance()->initialize(false);

    if (!VaultStorage::instance()->isStorageValid()) {
        VaultStorage::instance()->close();
        QMessageBox::critical(this,
                              tr("Error"),
                              tr("Your data vault is corrupted and or not readable / writeable."));
        return;
    }

    Q_EMIT vaultOpen();

    QVector<quint32> ids = VaultStorage::instance()->accountIds();
    if (ids.empty()) {
        const auto setupAssistant = new SetupAssistant();
        connect(setupAssistant,
                &SetupAssistant::accountsReceived,
                this,
                [&](const AccountList &accounts) {
                    VaultStorage::instance()->addAccounts(accounts);
                    d_ptr->app()->pageBanking->initialize(d_ptr->app());
                });
        setupAssistant->exec();
        setupAssistant->deleteLater();
    }

    ids.clear();
}

void PageDataVaults::deleteDataVaultItem(bool success,
                                         DataVaultItem *item,
                                         const QString &errorMessage)
{
    if (!success) {
        QMessageBox::critical(this, tr("Error"), errorMessage);
        return;
    }

    auto storageFiles = VaultStorage::instance()
                            ->setting(StorageSettingGroupKey, StorageSettingGroup, QStringList())
                            .toStringList();

    if (storageFiles.contains(item->vaultFilePath(), Qt::CaseSensitive)) {
        storageFiles.removeOne(item->vaultFilePath());
    }

    VaultStorage::instance()->storeSetting(StorageSettingGroupKey,
                                           storageFiles,
                                           StorageSettingGroup);

    if (storageFiles.isEmpty()) {
        removeVaultInfo();
        addVaultInfo();
    }

    m_scrollAreaDataVaultsContentsLayout->removeWidget(item);
    item->deleteLater();
    storageFiles.clear();
}
