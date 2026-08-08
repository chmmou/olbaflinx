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

#include "ui/Storage/NewStorageDialog.h"

#include "ui_NewStorageDialog.h"

#include "core/Storage/Storage.h"

#include <QtCore/QLatin1StringView>

#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>

using namespace olbaflinx::ui::storage;
using namespace olbaflinx::core::storage;

class NewStorageDialog::Private
{
public:
    explicit Private(NewStorageDialog *dialog, Storage *dialogStorage)
        : ui(new Ui::UiNewStorageDialog)
        , storage(dialogStorage)
    {
        ui->setupUi(dialog);
    }

    ~Private() { delete ui; }

    /**
     * Puts the length the core enforces into the texts that announce it. The
     * form carries the sentences with a placeholder; it used to carry the
     * number as well, and named six where the core asks for twelve.
     */
    void applyPasswordGuideline() const
    {
        const int minLength = storage->minPasswordLength();

        ui->labelStoragePasswordDescriptionTop->setText(
            ui->labelStoragePasswordDescriptionTop->text().arg(minLength));

        ui->labelHelpPassword->setToolTip(ui->labelHelpPassword->toolTip().arg(minLength));
        ui->labelHelpPasswordConfirm->setToolTip(
            ui->labelHelpPasswordConfirm->toolTip().arg(minLength));
    }

    /**
     * @return Empty if the input can carry a storage, otherwise the reason it
     *  cannot, in the words the user is shown
     */
    [[nodiscard]] QString validationMessage() const
    {
        const QString name = ui->lineEditStorageName->text();

        if (name.isEmpty()) {
            return NewStorageDialog::tr("Please enter a name for the storage.");
        }

        // The name becomes the file name, so the set that cannot be one under
        // Linux is refused here rather than replaced. The message of a name
        // that is already taken announces a name to the user, and the storage
        // has to carry the one that was read.
        if (name.contains(QLatin1Char('/'))) {
            return NewStorageDialog::tr("The name cannot contain the character \"/\".");
        }

        if (name.contains(QChar(QChar::Null))) {
            return NewStorageDialog::tr("The name cannot contain a null character.");
        }

        if (name.trimmed().isEmpty()) {
            return NewStorageDialog::tr("The name cannot consist of spaces alone.");
        }

        if (name == QLatin1StringView(".") || name == QLatin1StringView("..")) {
            return NewStorageDialog::tr("The name cannot be \".\" or \"..\", both stand for a "
                                        "directory.");
        }

        const QString password = ui->lineEditPassword->text();

        if (password.isEmpty()) {
            return NewStorageDialog::tr("Please enter a password for the storage.");
        }

        if (!storage->minPasswordGuidelines().match(password).hasMatch()) {
            return NewStorageDialog::tr("The password needs at least %1 characters, among them a "
                                        "lower and an upper case letter, a digit and a special "
                                        "character.")
                .arg(storage->minPasswordLength());
        }

        if (ui->lineEditPasswordConfirm->text() != password) {
            return NewStorageDialog::tr("The two passwords do not match.");
        }

        return {};
    }

    void validate() const
    {
        const QString message = validationMessage();

        ui->labelValidation->setText(message);
        ui->pushButtonOk->setEnabled(message.isEmpty());
    }

    Ui::UiNewStorageDialog *ui;
    Storage *storage;
};

NewStorageDialog::NewStorageDialog(Storage *storage, QWidget *parent, Qt::WindowFlags f)
    : QDialog(parent, f)
    , d_ptr(new Private(this, storage))
{
    d_ptr->applyPasswordGuideline();

    // Ok used to be wired to accept() in the form, which closed the dialog
    // before anything was looked at. It is gated on the check now.
    connect(d_ptr->ui->pushButtonOk, &QPushButton::clicked, this, &QDialog::accept);

    for (auto *field : {d_ptr->ui->lineEditStorageName,
                        d_ptr->ui->lineEditPassword,
                        d_ptr->ui->lineEditPasswordConfirm}) {
        connect(field, &QLineEdit::textChanged, this, [this]() { d_ptr->validate(); });
    }

    d_ptr->validate();
}

NewStorageDialog::~NewStorageDialog()
{
    delete d_ptr;
}

QString NewStorageDialog::name() const
{
    return d_ptr->ui->lineEditStorageName->text();
}

QString NewStorageDialog::password() const
{
    return d_ptr->ui->lineEditPassword->text();
}
