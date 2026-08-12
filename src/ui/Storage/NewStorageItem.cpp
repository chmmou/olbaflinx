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

#include <QtGui/QAccessible>
#include <QtGui/QFont>

#include <QtWidgets/QAccessibleWidget>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMessageBox>

using namespace olbaflinx::ui::storage;
using namespace olbaflinx::core::storage;

namespace {

/**
 * An entry holds a title, a password field and two buttons that belong
 * together. A plain QWidget reports itself as a client area, which tells a tool
 * nothing about that grouping, and setting a name does not change the role.
 *
 * It is a group and not a list item: the overview stacks the entries in a
 * layout, there is no selection to be part of, and a list item would promise one.
 */
class NewStorageItemAccessible final : public QAccessibleWidget
{
public:
    explicit NewStorageItemAccessible(QWidget *widget)
        : QAccessibleWidget(widget, QAccessible::Grouping)
    {}
};

QAccessibleInterface *accessibleFactory(const QString &className, QObject *object)
{
    Q_UNUSED(className)

    if (auto *item = qobject_cast<NewStorageItem *>(object)) {
        return new NewStorageItemAccessible(item);
    }

    return nullptr;
}

void installAccessibleFactory()
{
    static const bool installed = [] {
        QAccessible::installFactory(&accessibleFactory);
        return true;
    }();

    Q_UNUSED(installed)
}

} // namespace

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
{
    installAccessibleFactory();
}

NewStorageItem::~NewStorageItem()
{
    delete d_ptr;
}

void NewStorageItem::setTitle(const QString &title)
{
    d_ptr->ui->lblStorageTitel->setText(title);
    setAccessibleName(title);
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

    // The command is not built yet. It stays in the menu and says so through its
    // state, rather than being dropped: a tool can then tell the user that there
    // is such an entry and that it does not act. A missing entry tells nobody
    // anything, and a working one would have to be written first.
    itemMenu->addAction(tr("Information"))->setEnabled(false);

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

    // Both fields carry a pass phrase of the storage. Password mode is what keeps
    // it off the screen and out of the accessibility interface, which hands out
    // the masked text and reports a field that holds a secret. Without it a
    // reading aid speaks the phrase out loud.
    //
    // The names are what a run from outside holds the fields by; a dialog built
    // in code carries none unless it is given one.
    QPointer<QLineEdit> currPasswordField = new QLineEdit(&pwdChangeDlg);
    currPasswordField->setObjectName(QStringLiteral("lineEditCurrentPassword"));
    currPasswordField->setEchoMode(QLineEdit::Password);
    form.addRow(tr("Current Password"), currPasswordField);

    QPointer<QLineEdit> newPasswordField = new QLineEdit(&pwdChangeDlg);
    newPasswordField->setObjectName(QStringLiteral("lineEditNewPassword"));
    newPasswordField->setEchoMode(QLineEdit::Password);
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

    // The current pass phrase is what the user begins with. A dialog that does
    // not say so leaves the focus wherever the build order put it.
    currPasswordField->setFocus();

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
