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

#include "core/Banking/OnlineBanking.h"
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
{
    setupUi(this);
    setWizardStyle(QWizard::ModernStyle);

    pbImExportProgress->setVisible(false);
    pbIntroductionProgress->setVisible(false);

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

    connect(OnlineBanking::instance(),
            &OnlineBanking::progress,
            this,
            &ImExportAssistant::profileLoadingProgress);

    m_metaTypeIds << qRegisterMetaType<const ImExportProfileData *>();
    m_metaTypeIds << qRegisterMetaType<ImExportProfileDataList>();
}

ImExportAssistant::~ImExportAssistant() = default;

bool ImExportAssistant::validateCurrentPage()
{
    const bool isImportChecked = isImport();

    cbxImExportPlugin->clear();
    cbxImExportProfile->clear();

    m_imExportProfileList = OnlineBanking::instance()->importExportProfiles(isImportChecked);
    for (const auto profile : qAsConst(m_imExportProfileList)) {
        cbxImExportPlugin->addItem(profile->name, QVariant::fromValue(profile->profiles));
    }

    QWizardPage *page = currentPage();
    if (page->objectName() == "wizardPageIntroduction") {
        if (isImportChecked) {
            lblImExportPlugin->setText(tr("Import Plugin"));
            lblImExportProfile->setText(tr("Import Profile"));
            lblImExportFile->setText(tr("Import File"));
            pbIntroductionProgress->setFormat(tr("Loading import profiles ... %p%"));
            pbImExportProgress->setFormat(tr("Import transactions ... %p%"));
        } else {
            lblImExportPlugin->setText(tr("Export Plugin"));
            lblImExportProfile->setText(tr("Export Profile"));
            lblImExportFile->setText(tr("Export File"));
            pbIntroductionProgress->setFormat(tr("Loading export profiles ... %p%"));
            pbImExportProgress->setFormat(tr("Export transactions ... %p%"));
        }

        return rbIntroductionImport->isChecked() || rbIntroductionExport->isChecked();
    }

    return QWizard::validateCurrentPage();
}

void ImExportAssistant::setAccounts(const AccountList &accounts)
{
    for (const auto account : accounts) {
        cbxIntroductionAccounts->addItem(account->toString(), account->uniqueId());
    }
}

void ImExportAssistant::done(int result)
{
    if (result == QWizard::Rejected) {
        QWizard::done(result);
        return;
    }

    const bool isImportChecked = isImport();

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

    auto transactions = OnlineBanking::instance()->importTransactionsFromFile(imExporterName,
                                                                              imExporterProfile,
                                                                              leImExportFile->text());

    if (transactions.isEmpty()) {
        QMessageBox::critical(
            this,
            tr("Im- / Export Assistant"),
            tr("Transactions could not be imported with the specified importer / exporter!"));
        return;
    }

    const int accountId = cbxIntroductionAccounts->itemData(cbxIntroductionAccounts->currentIndex())
                              .toInt();
    VaultStorage::instance()->addTransactions(accountId, transactions);

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

void ImExportAssistant::initializePage(int id)
{
    if (id == ImExportAssistant::ImExport) {
        pbIntroductionProgress->setVisible(false);
        pbIntroductionProgress->setValue(0);
    }

    QWizard::initializePage(id);
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
        lblImExportProfileHelp->setText(profile->name);
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

void ImExportAssistant::profileLoadingProgress(qreal progress)
{
    if (!pbIntroductionProgress->isVisible()) {
        pbIntroductionProgress->setVisible(true);
    }

    pbIntroductionProgress->setValue((int) progress);
}

bool ImExportAssistant::isImport() const
{
    return rbIntroductionImport->isChecked() && !rbIntroductionExport->isChecked();
}
