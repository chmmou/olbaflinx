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
#include <QtConcurrent/QtConcurrent>

#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QPointer>

#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QSpacerItem>

#include "DataVaultItem.h"

#include "core/Container.h"
#include "core/Storage/VaultStorage.h"

using namespace olbaflinx::app::datavault;
using namespace olbaflinx::core;
using namespace olbaflinx::core::storage;

DataVaultItem::DataVaultItem(QWidget *parent)
    : QWidget(parent)
{
    setupUi(this);
}

DataVaultItem::~DataVaultItem() = default;

void DataVaultItem::setVaultTitle(const QString &title) const
{
    labelDataVaultTitel->setText(title);
}

void DataVaultItem::setVaultFileInfo(const QString &info) const
{
    labelDataVaultFileInfo->setText(info);
}

void DataVaultItem::setVaultFilePath(const QString &filePath) const
{
    labelDataVaultFilePath->setText(filePath);
}

QString DataVaultItem::vaultFilePath() const
{
    return labelDataVaultFilePath->text();
}

void DataVaultItem::showMenu()
{
    auto buttonMenu = new QMenu(this);

    buttonMenu->addAction(tr("Information"), this, &DataVaultItem::aboutDataVault);
    buttonMenu->addAction(tr("Change Password"), this, &DataVaultItem::showPasswordChangeDialog);
    buttonMenu->addSeparator();
    buttonMenu->addAction(tr("Backup"), this, &DataVaultItem::backupDataVault);
    buttonMenu->addAction(tr("Delete"), this, &DataVaultItem::deleteDataVault);

    connect(buttonMenu, &QMenu::aboutToHide, buttonMenu, &QMenu::deleteLater);

    // Retrieve a valid width of the menu. (It's not the same as using "buttonMenu->width()"!)
    const int menuWidth = buttonMenu->sizeHint().width();
    const int x = pushButtonDataVaultMenu->width() - menuWidth;
    const int y = pushButtonDataVaultMenu->height();

    QPoint pos(pushButtonDataVaultMenu->mapToGlobal(QPoint(x, y)));

    buttonMenu->popup(pos);
}

void DataVaultItem::openVault()
{
    Q_EMIT vaultOpened(vaultFilePath(), lineEditDataVaultPassword->text());
}

void DataVaultItem::showPasswordChangeDialog()
{
    QDialog passwordChangeDlg(this);
    passwordChangeDlg.setWindowTitle(tr("Vault Password"));

    QFormLayout form(&passwordChangeDlg);

    QPointer<QLabel> titleLabelField = new QLabel(&passwordChangeDlg);

    auto labelFont = titleLabelField->font();
    labelFont.setBold(true);
    labelFont.setPointSize(12);
    titleLabelField->setFont(labelFont);
    titleLabelField->setText(tr("Change Data Vault Password"));
    form.addRow(titleLabelField);

    form.addItem(new QSpacerItem(1, 3, QSizePolicy::Minimum, QSizePolicy::Fixed));

    QPointer<QLabel> infoLabelField = new QLabel(&passwordChangeDlg);
    infoLabelField->setWordWrap(true);
    infoLabelField->setText(tr("Choose an secure password possible with at least 6 letters, "
                               "numbers and special characters."));
    form.addRow(infoLabelField);

    form.addItem(new QSpacerItem(1, 12, QSizePolicy::Minimum, QSizePolicy::Fixed));

    QPointer<QLineEdit> currPasswordField = new QLineEdit(&passwordChangeDlg);
    form.addRow(tr("Current Password"), currPasswordField);

    QPointer<QLineEdit> newPasswordField = new QLineEdit(&passwordChangeDlg);
    form.addRow(tr("New Password"), newPasswordField);

    QDialogButtonBox passwordChangeDlgButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                              Qt::Horizontal,
                                              &passwordChangeDlg);

    form.addRow(&passwordChangeDlgButtons);

    connect(&passwordChangeDlgButtons,
            &QDialogButtonBox::rejected,
            &passwordChangeDlg,
            &QDialog::reject);

    connect(&passwordChangeDlgButtons,
            &QDialogButtonBox::accepted,
            &passwordChangeDlg,
            [&passwordChangeDlg, currPasswordField, newPasswordField, this]() {
                const auto dlgTitle = passwordChangeDlg.windowTitle();

                const auto currPassword = currPasswordField->text();
                if (currPassword.isEmpty()) {
                    QMessageBox::critical(&passwordChangeDlg,
                                          dlgTitle,
                                          tr("The current password cannot be empty!"));
                    return;
                }

                const auto newPassword = newPasswordField->text();
                if (newPassword.isEmpty()) {
                    QMessageBox::critical(&passwordChangeDlg,
                                          dlgTitle,
                                          tr("The new password cannot be empty!"));
                    return;
                }

                const auto passwordMatch = MinPasswordReqEx.match(newPassword);
                if (!passwordMatch.hasMatch()) {
                    QMessageBox::critical(
                        &passwordChangeDlg,
                        dlgTitle,
                        tr("The new password you entered does not match the criteria."));
                    return;
                }

                qApp->setOverrideCursor(Qt::WaitCursor);
                VaultStorage::instance()->setDatabaseKey(vaultFilePath(), currPassword);
                VaultStorage::instance()->initialize(false);
                const bool isStorageValid = VaultStorage::instance()->isStorageValid();
                if (!isStorageValid) {
                    QMessageBox::critical(&passwordChangeDlg,
                                          dlgTitle,
                                          tr("The current password is not correct!"));
                    qApp->restoreOverrideCursor();
                    return;
                }

                const bool success = VaultStorage::instance()->changeKey(currPassword, newPassword);
                if (!success) {
                    QMessageBox::critical(&passwordChangeDlg,
                                          dlgTitle,
                                          tr("The password could not be changed!"));
                    qApp->restoreOverrideCursor();
                    return;
                }
                VaultStorage::instance()->close();
                qApp->restoreOverrideCursor();
                passwordChangeDlg.accept();
            });

    passwordChangeDlg.exec();

    currPasswordField->deleteLater();
    newPasswordField->deleteLater();
    infoLabelField->deleteLater();
    titleLabelField->deleteLater();
}

void DataVaultItem::deleteDataVault()
{
    const QMessageBox::StandardButton result = QMessageBox::question(
        this,
        tr("Data Vault"),
        tr("Are you sure you want to delete your data vault?\nThis can not be undone!"));

    if (result == QMessageBox::Yes) {
        bool removed = true;
        QFile vaultFile(vaultFilePath());
        if (vaultFile.exists()) {
            removed &= vaultFile.remove();
        }

        Q_EMIT vaultDeleted(removed, this, vaultFile.errorString());
    }
}

void DataVaultItem::backupDataVault()
{
    QString storageBackupPath = VaultStorage::instance()->storagePath().append("/backup");
    QDir backupDir(storageBackupPath);
    if (!backupDir.exists()) {
        backupDir.mkpath(storageBackupPath);
    }

    QFile storageFile(vaultFilePath());
    if (storageFile.exists()) {
        const QString timeStamp = QDateTime::currentDateTime().toString(
            StorageFileBackupDateTimeFormat);

        QFileInfo info(vaultFilePath());
        const QString storageBackupFile = storageBackupPath.append("/%1.%2")
                                              .arg(timeStamp, info.completeSuffix());

        storageFile.copy(storageBackupFile);
    }
}

void DataVaultItem::aboutDataVault()
{
    QMessageBox::information(this, tr("Data Vault"), tr("Not implemented yet!"));
}
