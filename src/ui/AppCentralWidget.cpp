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
#include "ui/AppCentralWidget.h"

#include "ui/App.h"

#include "ui_AppCentralWidget.h"

#include <QtCore/QAbstractItemModel>

#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QStackedWidget>

using namespace olbaflinx::ui;

class AppCentralWidget::Private
{
public:
    explicit Private(AppCentralWidget *widget)
        : ui(new Ui::UiAppCentralWidget)
        , accountModel(nullptr)
        , app(nullptr)
        , q_ptr(widget)
    {
        ui->setupUi(q_ptr);

        // Neither view carries a visible label that could name it, so both need
        // one of their own for an assistive tool to announce.
        ui->treeViewBankingAccounts->setAccessibleName(AppCentralWidget::tr("Accounts"));
        ui->tableViewTransactions->setAccessibleName(AppCentralWidget::tr("Transactions"));

        applyAccountNotice();
        applyTransactionNotice();
    }

    // The generated form is created with new above and belongs to nobody else.
    // Without this it leaked, which is what an AddressSanitizer run reported
    // against tst_apperrorhandling.
    ~Private() { delete ui; }

    void initialize(QMainWindow *window) { app = qobject_cast<App *>(window); }

    void setAccountModel(QAbstractItemModel *model)
    {
        if (accountModel) {
            QObject::disconnect(accountModel, nullptr, q_ptr, nullptr);
        }

        accountModel = model;
        unreadable.clear();
        ui->treeViewBankingAccounts->setModel(model);

        if (accountModel) {
            const auto refresh = [this] { applyAccountNotice(); };

            QObject::connect(accountModel, &QAbstractItemModel::modelReset, q_ptr, refresh);
            QObject::connect(accountModel, &QAbstractItemModel::rowsInserted, q_ptr, refresh);
            QObject::connect(accountModel, &QAbstractItemModel::rowsRemoved, q_ptr, refresh);
        }

        applyAccountNotice();
    }

    void showAccountsUnreadable(const QString &message)
    {
        unreadable = message;

        applyAccountNotice();
    }

    void setTransactionModel(QAbstractItemModel *model)
    {
        if (transactionModel) {
            QObject::disconnect(transactionModel, nullptr, q_ptr, nullptr);
        }

        transactionModel = model;
        ui->tableViewTransactions->setModel(model);

        if (transactionModel) {
            const auto refresh = [this] { applyTransactionNotice(); };

            QObject::connect(transactionModel, &QAbstractItemModel::modelReset, q_ptr, refresh);
            QObject::connect(transactionModel, &QAbstractItemModel::rowsInserted, q_ptr, refresh);
            QObject::connect(transactionModel, &QAbstractItemModel::rowsRemoved, q_ptr, refresh);
        }

        applyTransactionNotice();
    }

    void setTransactionNotice(AppCentralWidget::TransactionNotice notice)
    {
        transactionNotice = notice;

        applyTransactionNotice();
    }

    /**
     * Decides between the table and the notice that stands in for it, and puts
     * the words of the current state into that notice.
     */
    void applyTransactionNotice()
    {
        if (transactionModel != nullptr && transactionModel->rowCount() > 0) {
            ui->stackedWidgetTransactions->setCurrentWidget(ui->pageTransactionTable);
            return;
        }

        // Choosing a bank is no choice of an account, so it shares the headline
        // of the state where nothing is chosen at all.
        const bool withoutAnAccount
            = transactionNotice != AppCentralWidget::TransactionNotice::AccountWithoutTransactions;

        ui->labelTransactionsHeadline->setText(withoutAnAccount
                                                   ? AppCentralWidget::tr("No account selected")
                                                   : AppCentralWidget::tr("No transactions"));

        switch (transactionNotice) {
        case AppCentralWidget::TransactionNotice::NoAccountSelected:
            ui->labelTransactionsNotice->setText(
                AppCentralWidget::tr("Choose an account on the left to see its transactions."));
            break;
        case AppCentralWidget::TransactionNotice::BankSelected:
            ui->labelTransactionsNotice->setText(
                AppCentralWidget::tr("A bank only groups the accounts it keeps. Choose one of them "
                                     "to see its transactions."));
            break;
        case AppCentralWidget::TransactionNotice::AccountWithoutTransactions:
            ui->labelTransactionsNotice->setText(
                AppCentralWidget::tr("This account holds no transactions yet."));
            break;
        }

        ui->stackedWidgetTransactions->setCurrentWidget(ui->pageTransactionsNotice);
    }

    /**
     * Decides between the tree and the notice that stands in for it.
     *
     * A read that failed is not a storage without accounts. The notice therefore
     * only names the missing accounts while nothing went wrong, and a run that
     * did bring accounts back clears the message a previous one left.
     */
    void applyAccountNotice()
    {
        if (accountModel != nullptr && accountModel->rowCount() > 0) {
            unreadable.clear();
            ui->stackedWidgetAccounts->setCurrentWidget(ui->pageAccountTree);
            return;
        }

        ui->labelAccountsNotice->setText(
            unreadable.isEmpty() ? AppCentralWidget::tr("No account has been set up yet. The setup "
                                                        "assistant fetches them from your bank.")
                                 : unreadable);

        ui->stackedWidgetAccounts->setCurrentWidget(ui->pageAccountsNotice);
    }

    Ui::UiAppCentralWidget *ui;

private:
    QAbstractItemModel *accountModel;
    QAbstractItemModel *transactionModel = nullptr;
    AppCentralWidget::TransactionNotice transactionNotice
        = AppCentralWidget::TransactionNotice::NoAccountSelected;
    QString unreadable;
    App *app;
    AppCentralWidget *q_ptr;
};

AppCentralWidget::AppCentralWidget(QWidget *parent, Qt::WindowFlags f)
    : QWidget(parent, f)
    , d_ptr(new Private(this))
{}

AppCentralWidget::~AppCentralWidget()
{
    delete d_ptr;
}

void AppCentralWidget::initialize(QMainWindow *window)
{
    d_ptr->initialize(window);
}

QTreeView *AppCentralWidget::accountWidget() const
{
    return d_ptr->ui->treeViewBankingAccounts;
}

void AppCentralWidget::setAccountModel(QAbstractItemModel *model)
{
    d_ptr->setAccountModel(model);
}

void AppCentralWidget::showAccountsUnreadable(const QString &message)
{
    d_ptr->showAccountsUnreadable(message);
}

void AppCentralWidget::setTransactionModel(QAbstractItemModel *model)
{
    d_ptr->setTransactionModel(model);
}

void AppCentralWidget::setTransactionNotice(TransactionNotice notice)
{
    d_ptr->setTransactionNotice(notice);
}

void AppCentralWidget::setPage(Page page)
{
    d_ptr->ui->stackedWidgetPages->setCurrentIndex(static_cast<int>(page));
}

AppCentralWidget::Page AppCentralWidget::page() const
{
    return static_cast<Page>(d_ptr->ui->stackedWidgetPages->currentIndex());
}

void AppCentralWidget::setStorageOverview(QWidget *overview)
{
    d_ptr->ui->storagesLayout->addWidget(overview);
}
