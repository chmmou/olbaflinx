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
#include "ui/Storage/NewStorageItem.h"

#include "ui_NewStorageItem.h"

#include "core/Storage/Storage.h"
#include "ui/Logging.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QPointer>

#include <QtGui/QFont>

#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMessageBox>

using namespace olbaflinx::ui::storage;
using namespace olbaflinx::core::storage;

class NewStorageItem::Private
{
public:
    explicit Private(NewStorageItem *storagePageItem, Storage *itemStorage)
        : ui(new Ui::UiNewStorageItem)
        , storage(itemStorage)
    {
        ui->setupUi(storagePageItem);
    }

    ~Private() { delete ui; }

    Ui::UiNewStorageItem *ui;
    Storage *storage;
};

NewStorageItem::NewStorageItem(Storage *storage, QWidget *parent, Qt::WindowFlags f)
    : QWidget(parent, f)
    , d_ptr(new Private(this, storage))
{}

NewStorageItem::~NewStorageItem()
{
    delete d_ptr;
}

void NewStorageItem::setTitle(const QString &title) const
{
    d_ptr->ui->lblStorageTitel->setText(title);
}

void NewStorageItem::setFileInfo(const QString &info) const
{
    d_ptr->ui->lblFileInfo->setText(info);
}

void NewStorageItem::setFilePath(const QString &filePath) const
{
    d_ptr->ui->lblFilePath->setText(filePath);
}

QString NewStorageItem::filePath() const
{
    return d_ptr->ui->lblFilePath->text();
}

void NewStorageItem::showMenu()
{
    auto itemMenu = new QMenu(this);

    itemMenu->addAction(tr("Information"), this, &NewStorageItem::aboutStorage);
    itemMenu->addAction(tr("Passwort ändern"), this, &NewStorageItem::showPasswordChangeDialog);
    itemMenu->addSeparator();
    itemMenu->addAction(tr("Sicherungen"), this, &NewStorageItem::backupStorage);
    itemMenu->addAction(tr("Löschen"), this, &NewStorageItem::deleteStorage);

    connect(itemMenu, &QMenu::aboutToHide, itemMenu, &QMenu::deleteLater);

    const int menuWidth = itemMenu->sizeHint().width();
    const int x = d_ptr->ui->btnStorageMenu->width() - menuWidth;
    const int y = d_ptr->ui->btnStorageMenu->height();

    QPoint pos(d_ptr->ui->btnStorageMenu->mapToGlobal(QPoint(x, y)));

    itemMenu->popup(pos);
}

void NewStorageItem::openVault()
{
    Q_EMIT storageOpened(filePath(), d_ptr->ui->leStoragePassword->text());
}

void NewStorageItem::showPasswordChangeDialog()
{
    QDialog pwdChangeDlg(this);
    pwdChangeDlg.setWindowTitle(tr("Change Storage Password"));

    QFormLayout form(&pwdChangeDlg);

    QPointer<QLabel> titleLabelField = new QLabel(&pwdChangeDlg);

    auto labelFont = titleLabelField->font();
    labelFont.setBold(true);
    labelFont.setPointSize(12);

    titleLabelField->setFont(labelFont);
    titleLabelField->setText(tr("Change Storage Password"));
    form.addRow(titleLabelField);

    form.addItem(new QSpacerItem(1, 3, QSizePolicy::Minimum, QSizePolicy::Fixed));

    QPointer<QLabel> infoLabelField = new QLabel(&pwdChangeDlg);
    infoLabelField->setWordWrap(true);
    // The number has to match the guideline the storage enforces below, a user
    // who is told six and then rejected learns nothing from the rejection.
    infoLabelField->setText(tr("Choose an secure password possible with at least 12 letters, "
                               "numbers and special characters."));
    form.addRow(infoLabelField);

    form.addItem(new QSpacerItem(1, 12, QSizePolicy::Minimum, QSizePolicy::Fixed));

    QPointer<QLineEdit> currPasswordField = new QLineEdit(&pwdChangeDlg);
    form.addRow(tr("Current Password"), currPasswordField);

    QPointer<QLineEdit> newPasswordField = new QLineEdit(&pwdChangeDlg);
    form.addRow(tr("New Password"), newPasswordField);

    QDialogButtonBox pwdChangeDlgBtns(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                      Qt::Horizontal,
                                      &pwdChangeDlg);
    form.addRow(&pwdChangeDlgBtns);

    connect(&pwdChangeDlgBtns, &QDialogButtonBox::rejected, &pwdChangeDlg, &QDialog::reject);

    connect(&pwdChangeDlgBtns,
            &QDialogButtonBox::accepted,
            &pwdChangeDlg,
            [&pwdChangeDlg, currPasswordField, newPasswordField, this]() {
                const auto dlgTitle = pwdChangeDlg.windowTitle();

                const auto currPassword = currPasswordField->text();
                if (currPassword.isEmpty()) {
                    QMessageBox::critical(&pwdChangeDlg,
                                          dlgTitle,
                                          tr("The current password cannot be empty!"));
                    return;
                }

                const auto newPassword = newPasswordField->text();
                if (newPassword.isEmpty()) {
                    QMessageBox::critical(&pwdChangeDlg,
                                          dlgTitle,
                                          tr("The new password cannot be empty!"));
                    return;
                }

                const auto passwordMatch = d_ptr->storage->minPasswordGuidelines().match(
                    newPassword);

                if (!passwordMatch.hasMatch()) {
                    QMessageBox::critical(&pwdChangeDlg,
                                          dlgTitle,
                                          tr("The password you have entered does not comply with "
                                             "the minimum guideline."));
                    return;
                }

                qApp->setOverrideCursor(Qt::WaitCursor);

                // The guideline was checked above, so this can only fail if the
                // two checks ever drift apart. It is not left unread for that.
                if (const auto error = d_ptr->storage->setKey(currPassword); error.isError()) {
                    qApp->restoreOverrideCursor();
                    qCWarning(lcUiStorage) << "the key was refused:" << error.message();

                    QMessageBox::critical(&pwdChangeDlg,
                                          dlgTitle,
                                          tr("The password you have entered does not comply with "
                                             "the minimum guideline."));
                    return;
                }

                d_ptr->storage->setStorageFile(filePath());

                if (const auto error = d_ptr->storage->initialize(false); error.isError()) {
                    qCWarning(lcUiStorage) << "could not open the storage:" << error.message();

                    QMessageBox::critical(&pwdChangeDlg,
                                          dlgTitle,
                                          tr("The storage could not be opened. Check the "
                                             "current password."));
                    qApp->restoreOverrideCursor();
                    return;
                }

                const bool isStorageValid = d_ptr->storage->isValid();
                if (!isStorageValid) {
                    QMessageBox::critical(&pwdChangeDlg,
                                          dlgTitle,
                                          tr("The current password is not correct!"));
                    qApp->restoreOverrideCursor();
                    return;
                }

                if (const auto error = d_ptr->storage->changeKey(currPassword, newPassword);
                    error.isError()) {
                    qCWarning(lcUiStorage) << "could not change the key:" << error.message();

                    QMessageBox::critical(&pwdChangeDlg,
                                          dlgTitle,
                                          tr("The password could not be changed!"));
                    qApp->restoreOverrideCursor();
                    return;
                }
                d_ptr->storage->close();

                qApp->restoreOverrideCursor();
                pwdChangeDlg.accept();
            });

    pwdChangeDlg.exec();

    currPasswordField->deleteLater();
    newPasswordField->deleteLater();
    infoLabelField->deleteLater();
    titleLabelField->deleteLater();
}

void NewStorageItem::deleteStorage()
{
    const QMessageBox::StandardButton result = QMessageBox::question(
        this,
        tr("Storage"),
        tr("Are you sure you want to delete your storage?\nThis can not be undone!"));

    if (result == QMessageBox::Yes) {
        bool removed = true;
        QFile file(filePath());
        if (file.exists()) {
            removed &= file.remove();
        }

        Q_EMIT storageDeleted(removed, this, file.errorString());
    }
}

void NewStorageItem::backupStorage()
{
    QString storageBackupPath = d_ptr->storage->storagePath().append(QStringLiteral("/backup"));
    QDir backupDir(storageBackupPath);
    if (!backupDir.exists()) {
        backupDir.mkpath(storageBackupPath);
    }

    QFile storageFile(filePath());
    if (storageFile.exists()) {
        // UTC. A backup taken during the hour a daylight saving change repeats
        // would otherwise sort before one taken an hour earlier.
        const QString timeStamp = QDateTime::currentDateTimeUtc().toString(
            QStringLiteral("yyyyMMddhhmmsszzz"));

        QFileInfo info(filePath());
        const QString storageBackupFile = storageBackupPath.append(QStringLiteral("/%1.%2"))
                                              .arg(timeStamp, info.completeSuffix());

        storageFile.copy(storageBackupFile);
    }
}

void NewStorageItem::aboutStorage()
{
    QMessageBox::information(this, tr("Storage"), tr("Not implemented yet!"));
}
