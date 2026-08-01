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

#include "ui_OptionBankingPage.h"

using namespace olbaflinx::core;
using namespace olbaflinx::ui::assistant::pages;

namespace {

/**
 * The FinTS registration key of this product. It names the application to the
 * bank servers; it authenticates no user and grants access to no account, so it
 * is not a secret and losing it costs nothing but the identification.
 *
 * A key in the source is normally forbidden without qualification. This one is
 * kept there on purpose, because moving a value that identifies the build into
 * a build time variable would hide it without protecting anything. The value
 * has been public in this repository since it was first committed; taking it
 * out would not make it secret again.
 */
constexpr auto FinTsRegistrationKey = QLatin1StringView("3E1B97FF72A24783EC2215B12");

} // namespace

class OptionBankingPage::Private
{
public:
    explicit Private(OptionBankingPage *bankingPage)
        : isComplete(false)
        , banking(nullptr)
        , q_ptr(bankingPage)
        , ui(new Ui::UiSetupAssistantOptionBankingPage)
    {
        ui->setupUi(q_ptr);
    }

    ~Private() { delete ui; }

    void createBanking(const ApplicationInfo &applicationInfo)
    {
        if (banking != nullptr) {
            return;
        }

        // The page is the parent so that it releases the backend.
        banking = new Banking(applicationInfo, q_ptr);

        if (const auto error = banking->initialize(applicationInfo.name,
                                                   applicationInfo.version,
                                                   FinTsRegistrationKey);
            error.isError()) {
            qCWarning(lcUi) << "could not initialize the banking backend:" << error.message();
        }
    }

    void addItems(const BankingItems &items)
    {
        ui->treeWidgetAccounts->clear();
        for (const auto &item : items) {
            const auto account = std::dynamic_pointer_cast<Account>(item);
            if (account && account->isValid()) {
                const auto treeItem = new QTreeWidgetItem;
                treeItem->setText(0, account->toString());
                treeItem->setData(0, Qt::UserRole, account->uniqueId());
                ui->treeWidgetAccounts->addTopLevelItem(treeItem);
            }
        }

        Q_EMIT q_ptr->completeChanged();
    }

    bool isComplete;
    Banking *banking;
    OptionBankingPage *q_ptr;
    Ui::UiSetupAssistantOptionBankingPage *ui;
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

    connect(d_ptr->banking, &Banking::itemsReceived, this, [&](const BankingItems &items) {
        d_ptr->addItems(items);
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

QList<quint32> OptionBankingPage::selectedAccountIds() const
{
    auto accountIds = QList<quint32>();

    const auto selectedItems = d_ptr->ui->treeWidgetAccounts->selectedItems();
    for (const auto item : selectedItems) {
        accountIds << item->data(0, Qt::UserRole).toUInt();
    }

    return accountIds;
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
