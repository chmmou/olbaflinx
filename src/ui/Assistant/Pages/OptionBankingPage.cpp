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

#include "ui/Assistant/Pages/OptionBankingPage.h"

#include "core/Banking/Banking.h"
#include "ui/ErrorMessage.h"
#include "ui/Logging.h"

// The Qt implementation of the gwenhywfar user interface. The header keeps its
// qt5 name across the version change; the library built from it for Qt 6 is
// libgwengui-qt6. It belongs here and not in the core, which must stay usable
// without a display.
#include <gwen-gui-qt5/qt5_gui.hpp>

#include "ui_OptionBankingPage.h"

using namespace olbaflinx::core;
using namespace olbaflinx::ui::assistant::pages;

class OptionBankingPage::Private
{
public:
    explicit Private(OptionBankingPage *bankingPage)
        : isComplete(false)
        , qtGui(nullptr)
        , banking(nullptr)
        , q_ptr(bankingPage)
        , ui(new Ui::UiSetupAssistantOptionBankingPage)
    {
        ui->setupUi(q_ptr);
    }

    ~Private()
    {
        // The order matters and is the reason the backend carries no Qt parent.
        // Banking still reaches into the interface while it shuts down, in
        // AB_Gui_Unextend. Left to the parent it would be destroyed after this
        // function, and the interface would already be gone by then.
        delete banking;
        banking = nullptr;

        delete qtGui;
        qtGui = nullptr;

        delete ui;
    }

    void createBanking(const ApplicationInfo &applicationInfo)
    {
        if (banking != nullptr) {
            return;
        }

        // The interface is built here and stays here. The core is handed the C
        // side of it and neither frees it nor outlives it.
        qtGui = new QT5_Gui();
        banking = new Banking(applicationInfo);

        if (const auto error = banking->initialize(applicationInfo.name,
                                                   applicationInfo.version,
                                                   applicationInfo.registrationKey,
                                                   qtGui->getCInterface());
            error.isError()) {
            qCWarning(lcUi) << "could not initialize the banking backend:" << error.message();
        }
    }

    void addItems(const BankingItems &items)
    {
        ui->treeWidgetAccounts->clear();
        offered.clear();

        for (const auto &item : items) {
            const auto account = std::dynamic_pointer_cast<Account>(item);
            if (account && account->isValid()) {
                const auto treeItem = new QTreeWidgetItem;
                treeItem->setText(0, account->toString());
                treeItem->setData(0, Qt::UserRole, account->uniqueId());
                ui->treeWidgetAccounts->addTopLevelItem(treeItem);

                offered << item;
            }
        }

        Q_EMIT q_ptr->completeChanged();
    }

    bool isComplete;
    QT5_Gui *qtGui;
    Banking *banking;
    OptionBankingPage *q_ptr;
    Ui::UiSetupAssistantOptionBankingPage *ui;

    // The entries the tree shows carry the unique id of their account, no more.
    // Whoever stores the result of the wizard needs the accounts themselves, and
    // the page is the only place that still has them.
    BankingItems offered;
};

OptionBankingPage::OptionBankingPage(QWidget *parent)
    : QWizardPage(parent)
    , d_ptr(new Private(this))
{}

OptionBankingPage::~OptionBankingPage()
{
    delete d_ptr;
}

void OptionBankingPage::initialize(const ApplicationInfo &applicationInfo)
{
    d_ptr->createBanking(applicationInfo);

    connect(d_ptr->ui->treeWidgetAccounts, &QTreeWidget::itemSelectionChanged, this, [&]() {
        d_ptr->isComplete = false;
        const bool hasItemsSelected = !d_ptr->ui->treeWidgetAccounts->selectedItems().isEmpty();
        if (hasItemsSelected) {
            d_ptr->isComplete = true;
        }

        Q_EMIT completeChanged();
    });

    connect(d_ptr->banking, &Banking::itemsReceived, this, [this](const BankingItems &items) {
        setAccounts(items);
    });

    // Without this the errors of the backend had no receiver. The page says what
    // went wrong in its subtitle; a modal box would block a wizard the user can
    // still go back in.
    connect(d_ptr->banking,
            &Banking::errorOccurred,
            this,
            [this](ErrorCode code, const QString &reason) {
                qCWarning(lcUi) << "error from the banking backend:" << reason;
                setSubTitle(userMessage(code));
            });

    d_ptr->banking->accounts();
}

bool OptionBankingPage::isComplete() const
{
    return d_ptr->isComplete && d_ptr->ui->treeWidgetAccounts->topLevelItemCount() > 0;
}

void OptionBankingPage::setAccounts(const BankingItems &accounts)
{
    d_ptr->addItems(accounts);
}

QList<quint32> OptionBankingPage::selectedAccountIds() const
{
    auto accountIds = QList<quint32>();

    const auto selectedItems = d_ptr->ui->treeWidgetAccounts->selectedItems();
    for (const auto item : selectedItems) {
        accountIds << item->data(0, Qt::UserRole).toUInt();
    }

    return accountIds;
}

BankingItems OptionBankingPage::selectedAccounts() const
{
    // One list, held while the set is built from it. Two calls would hand the
    // set the begin of one temporary and the end of another.
    const auto ids = selectedAccountIds();
    const auto selectedIds = QSet<quint32>(ids.cbegin(), ids.cend());

    auto accounts = BankingItems();
    for (const auto &item : std::as_const(d_ptr->offered)) {
        const auto account = std::dynamic_pointer_cast<Account>(item);
        if (account && selectedIds.contains(account->uniqueId())) {
            accounts << item;
        }
    }

    return accounts;
}

BankingItems OptionBankingPage::offeredAccounts() const
{
    return d_ptr->offered;
}

void OptionBankingPage::showSetupDialog()
{
    d_ptr->isComplete = false;

    // 1 means the dialog was accepted, 0 that the user dismissed it. Anything
    // below is a failure of the backend, which used to go by unnoticed.
    const int result = d_ptr->banking->setupAccounts();
    if (result < 0) {
        qCWarning(lcUi) << "the account setup dialog failed with" << result;
        setSubTitle(userMessage(ErrorCode::BankingFailure));
        return;
    }

    if (result == 1) {
        d_ptr->banking->accounts();
    }
}
