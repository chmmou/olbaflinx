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

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QMetaType>

#include <QtWidgets/QComboBox>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QWizardPage>

#include "core/Banking/Banking.h"
#include "core/Storage/VaultStorage.h"

#include "ImExportAssistant.h"

using namespace olbaflinx::core;
using namespace olbaflinx::core::storage;
using namespace olbaflinx::core::banking;
using namespace olbaflinx::app::assistant;

ImExportAssistant::ImExportAssistant(QWidget *parent)
    : QWizard(parent)
    , m_imExportProfileList({})
    , m_metaTypeIds({})
    , m_accountId(0)
{
    setupUi(this);
    setWizardStyle(QWizard::ModernStyle);

    pbImExportProgress->setVisible(false);

    connect(cbxImExportProfile,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &ImExportAssistant::comboboxIndexChanged);

    connect(cbxImExportPlugin,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &ImExportAssistant::comboboxIndexChanged);

    connect(tbImExportFile, &QToolButton::clicked, this, &ImExportAssistant::openImExportFile);
    connect(VaultStorage::instance(),
            &VaultStorage::progress,
            this,
            &ImExportAssistant::imExportProgress);

    m_metaTypeIds << qRegisterMetaType<const ImExportProfileData *>();
    m_metaTypeIds << qRegisterMetaType<ImExportProfileDataList>();
}

ImExportAssistant::~ImExportAssistant() = default;

bool ImExportAssistant::validateCurrentPage()
{
    const bool isImportChecked = rbIntroductionImport->isChecked()
                                 && !rbIntroductionExport->isChecked();

    cbxImExportPlugin->clear();
    cbxImExportProfile->clear();

    m_imExportProfileList = Banking::instance()->importExportProfiles(isImportChecked);
    for (const auto profile : qAsConst(m_imExportProfileList)) {
        cbxImExportPlugin->addItem(profile->name, QVariant::fromValue(profile->profiles));
    }

    QWizardPage *page = currentPage();
    if (page->objectName() == "wizardPageIntroduction") {
        const bool importChecked = rbIntroductionImport->isChecked()
                                   && !rbIntroductionExport->isChecked();

        if (importChecked) {
            lblImExportPlugin->setText(tr("Import Plugin"));
            lblImExportProfile->setText(tr("Import Profile"));
            lblImExportFile->setText(tr("Import File"));
        } else {
            lblImExportPlugin->setText(tr("Export Plugin"));
            lblImExportProfile->setText(tr("Export Profile"));
            lblImExportFile->setText(tr("Export File"));
        }

        return rbIntroductionImport->isChecked() || rbIntroductionExport->isChecked();
    }

    return QWizard::validateCurrentPage();
}

void ImExportAssistant::setAccountId(quint32 id)
{
    m_accountId = id;
}

void ImExportAssistant::done(int result)
{
    if (result == QWizard::Rejected) {
        QWizard::done(result);
        return;
    }

    const bool isImportChecked = rbIntroductionImport->isChecked()
                                 && !rbIntroductionExport->isChecked();

    if (leImExportFile->text().isEmpty()) {
        QMessageBox::critical(this,
                              tr("Im- / Export Assistant"),
                              tr("A file must be selected for %1!")
                                  .arg(isImportChecked ? tr("import") : tr("export")));
        return;
    }

    QFile f(leImExportFile->text());
    if (!f.exists()) {
        QMessageBox::critical(this,
                              tr("Im- / Export Assistant"),
                              tr("The selected file for %1 does not exist anymore!")
                                  .arg(isImportChecked ? tr("import") : tr("export")));
        return;
    }

    const auto imExporterName = cbxImExportPlugin->currentText();
    const auto imExporterProfile = cbxImExportProfile->currentText();

    auto transactions = Banking::instance()->import(imExporterName,
                                                    imExporterProfile,
                                                    leImExportFile->text());

    if (transactions.isEmpty()) {
        QMessageBox::critical(
            this,
            tr("Im- / Export Assistant"),
            tr("Transactions could not be imported with the specified importer / exporter!"));
        return;
    }

    VaultStorage::instance()->addTransactions(m_accountId, transactions);

    qDeleteAll(transactions);
    transactions.clear();

    for (const auto profile : qAsConst(m_imExportProfileList)) {
        qDeleteAll(profile->profiles);
    }
    qDeleteAll(m_imExportProfileList);
    m_imExportProfileList.clear();

    for (const int type : qAsConst(m_metaTypeIds)) {
        QMetaType::unregisterType(type);
    }
    m_metaTypeIds.clear();

    QWizard::done(result);
}

void ImExportAssistant::comboboxIndexChanged(int index)
{
    const auto cbx = qobject_cast<QComboBox *>(sender());
    if (cbx == Q_NULLPTR) {
        return;
    }

    if (cbx->objectName() == "cbxImExportPlugin") {
        cbxImExportProfile->clear();
        const auto profiles = qvariant_cast<ImExportProfileDataList>(cbx->itemData(index));
        for (const auto profile : profiles) {
            cbxImExportProfile->addItem(profile->name, QVariant::fromValue(profile));
        }
    } else if (cbx->objectName() == "cbxImExportProfile") {
        const auto profile = qvariant_cast<const ImExportProfileData *>(cbx->itemData(index));
        if (profile == Q_NULLPTR) {
            return;
        }
        lblImExportProfileHhelp->setText(profile->name);
        lblImExportProfileHelpDesc->setText(profile->longDescr);
    }
}

void ImExportAssistant::openImExportFile()
{
    const auto fileName = QFileDialog::getOpenFileName(
        this,
        tr("Im- / Export Assistant"),
        QDir::homePath(),
        tr("All Files (*.*);;All Supported Files (*.xml *.sta *.ofx *.q43 *.ctx);;XML Files "
           "(*.xml);;SWIFT Files (*.sta);;OFX Files (*.ofx);;Q43 Files (*.q43);;CTX Files (*.ctx)"),
        nullptr,
        QFileDialog::ReadOnly);

    if (!fileName.isEmpty()) {
        leImExportFile->setText(fileName);
    }
}

void ImExportAssistant::imExportProgress(qreal progress)
{
    if (!pbImExportProgress->isVisible()) {
        pbImExportProgress->setVisible(true);
    }

    pbImExportProgress->setValue((int) progress);
}
