/**
 * Copyright (C) 2022-2025, Alexander Saal <developer@olbaflinx.chm-projects.de>
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

#include <Banking/Banking.h>

#include "OptionBankingPage.h"

#include <QtWidgets/QApplication>

#include "ui_OptionBankingPage.h"

using namespace olbaflinx::app::assistant::pages;

class OptionBankingPage::Private
{
public:
    explicit Private(OptionBankingPage *bankingPage)
        : isComplete(false)
        , banking(new Banking)
        , q_ptr(bankingPage)
        , ui(new Ui::UiSetupAssistantOptionBankingPage)
    {
        ui->setupUi(q_ptr);

        banking->initialize(QApplication::applicationName(),
                            QApplication::applicationVersion(),
                            "3E1B97FF72A24783EC2215B12");
    }

    ~Private()
    {
        banking->finalize();
        banking->deleteLater();

        delete ui;
    }

    void addItems(const QList<BankingItem *> &items)
    {
        ui->treeWidgetAccounts->clear();
        for (const auto item : items) {
            if (item->isValid()) {
                const auto treeItem = new QTreeWidgetItem;
                treeItem->setText(0, item->toString());
                treeItem->setData(0, Qt::UserRole, ((Account *) item)->uniqueId());
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

void OptionBankingPage::initialize()
{
    connect(d_ptr->ui->treeWidgetAccounts, &QTreeWidget::itemSelectionChanged, this, [&]() {
        d_ptr->isComplete = false;
        const bool hasItemsSelected = !d_ptr->ui->treeWidgetAccounts->selectedItems().isEmpty();
        if (hasItemsSelected) {
            d_ptr->isComplete = true;
        }

        Q_EMIT completeChanged();
    });

    connect(d_ptr->banking, &Banking::itemsReceived, this, [&](const QList<BankingItem *> &items) {
        d_ptr->addItems(items);
    });

    d_ptr->banking->accounts();
}

bool OptionBankingPage::isComplete() const
{
    return d_ptr->isComplete && d_ptr->ui->treeWidgetAccounts->topLevelItemCount() > 0;
}

QList<quint32> OptionBankingPage::selectedAccountIds()
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

    int result = d_ptr->banking->setupAccounts();
    if (result == 1) {
        d_ptr->banking->accounts();
    }
}
