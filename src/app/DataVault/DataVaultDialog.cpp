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

#include <QtCore/QRegularExpression>
#include <QtGui/QRegularExpressionValidator>
#include <QtWidgets/QMessageBox>
#include <QtCore/QDebug>

#include "DataVaultDialog.h"
#include "core/Container.h"

using namespace olbaflinx::app::datavault;

DataVaultDialog::DataVaultDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUi(this);
}

DataVaultDialog::~DataVaultDialog() = default;

QString DataVaultDialog::vaultName() const
{
    return lineEditVaultName->text();
}

QString DataVaultDialog::vaultPassword() const
{
    return lineEditPassword->text();
}

void DataVaultDialog::accept()
{
    if (lineEditVaultName->text().isEmpty()) {
        QMessageBox::critical(
            this,
            tr("Data Vault Error"),
            tr("The name for you new data vault can not be empty.")
        );
        return;
    }

    if (lineEditPassword->text().isEmpty()) {
        QMessageBox::critical(
            this,
            tr("Password Error"),
            tr("The password can not be empty.")
        );
        return;
    }

    if (lineEditPasswordConfirm->text().isEmpty()) {
        QMessageBox::critical(
            this,
            tr("Password Error"),
            tr("The confirmation password can not be empty.")
        );
        return;
    }

    if (lineEditPassword->text() != lineEditPasswordConfirm->text()) {
        QMessageBox::critical(
            this,
            tr("Password Error"),
            tr("The password you entered does not match.")
        );
        return;
    }

    auto passwordMatch = MinPasswordReqEx.match(lineEditPassword->text());
    auto passwordConfirmMatch = MinPasswordReqEx.match(lineEditPasswordConfirm->text());

    if (!passwordMatch.hasMatch() || !passwordConfirmMatch.hasMatch()) {
        QMessageBox::critical(
            this,
            tr("Password Error"),
            tr("The password you entered does not match.")
        );
        return;
    }

    QDialog::accept();
}
