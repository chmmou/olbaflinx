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
#include <QtCore/QTimer>

#include <QtGui/QAccessible>
#include <QtGui/QAccessibleEvent>

#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStackedWidget>

using namespace olbaflinx::core::storage;
using namespace olbaflinx::ui::models;

namespace {

/**
 * How long the search field waits after the last keystroke before it asks.
 *
 * Every ask runs a query over the whole holding of the account, so a request per
 * keystroke would put three thousand rows through the storage for a word of ten
 * letters. Short enough that a user who stops typing sees the result at once.
 */
constexpr int SearchDelayMs = 300;

/**
 * The periods the bar offers, as the number of days they reach back. Zero stands
 * for the whole holding, which is what the bar opens with: a narrower default
 * would hide bookings without saying so.
 */
constexpr int WholePeriod = 0;

} // namespace

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

        setUpTransactionFilter();

        applyAccountNotice();
        applyTransactionNotice();
    }

    /**
     * Builds the bar above the transactions and wires it to the model.
     *
     * Five parts and no sixth: a field for the text, the period, the direction, a
     * reset, and the number of transactions the filter leaves. The number is no
     * control, it is what tells the user what he is looking at.
     */
    void setUpTransactionFilter()
    {
        auto *const search = ui->lineEditTransactionSearch;
        auto *const period = ui->comboBoxTransactionPeriod;
        auto *const direction = ui->comboBoxTransactionDirection;
        auto *const reset = ui->pushButtonTransactionFilterReset;

        // The placeholder names both fields that are searched. Without it the
        // user has to guess whether the name of the other party counts.
        search->setPlaceholderText(AppCentralWidget::tr("Search counterparty and purpose"));

        period->addItem(AppCentralWidget::tr("All dates"), WholePeriod);
        period->addItem(AppCentralWidget::tr("Last 30 days"), 30);
        period->addItem(AppCentralWidget::tr("Last 90 days"), 90);
        period->addItem(AppCentralWidget::tr("Last 365 days"), 365);

        // All three values, not two and an unnamed rest.
        direction->addItem(AppCentralWidget::tr("All bookings"),
                           QVariant::fromValue(Storage::Direction::Any));
        direction->addItem(AppCentralWidget::tr("Incoming"),
                           QVariant::fromValue(Storage::Direction::Incoming));
        direction->addItem(AppCentralWidget::tr("Outgoing"),
                           QVariant::fromValue(Storage::Direction::Outgoing));

        reset->setText(AppCentralWidget::tr("Reset"));
        ui->pushButtonTransactionsNoticeReset->setText(AppCentralWidget::tr("Reset the filter"));

        // The three that carry no visible label of their own. The button and the
        // counter say what they are through their text, and a name of their own
        // would be that text a second time.
        search->setAccessibleName(AppCentralWidget::tr("Search transactions"));
        period->setAccessibleName(AppCentralWidget::tr("Period"));
        direction->setAccessibleName(AppCentralWidget::tr("Direction of the booking"));

        searchTimer = new QTimer(q_ptr);
        searchTimer->setSingleShot(true);
        searchTimer->setInterval(SearchDelayMs);

        QObject::connect(searchTimer, &QTimer::timeout, q_ptr, [this] { applyTransactionFilter(); });

        // Typing restarts the wait rather than asking. The other three take
        // effect at once: they change in one step, not letter by letter.
        QObject::connect(search, &QLineEdit::textChanged, q_ptr, [this] { searchTimer->start(); });

        QObject::connect(period, &QComboBox::currentIndexChanged, q_ptr, [this] {
            applyTransactionFilter();
        });

        QObject::connect(direction, &QComboBox::currentIndexChanged, q_ptr, [this] {
            applyTransactionFilter();
        });

        QObject::connect(reset, &QPushButton::clicked, q_ptr, [this] { resetTransactionFilter(); });

        QObject::connect(ui->pushButtonTransactionsNoticeReset,
                         &QPushButton::clicked,
                         q_ptr,
                         [this] { resetTransactionFilter(); });

        applyTransactionCount();
    }

    /**
     * Reads the bar and hands the restriction to the model.
     */
    void applyTransactionFilter()
    {
        searchTimer->stop();

        if (transactionModel == nullptr) {
            return;
        }

        auto filter = TransactionTableModel::Filter();
        filter.text = ui->lineEditTransactionSearch->text().trimmed();

        const int days = ui->comboBoxTransactionPeriod->currentData().toInt();
        if (days > WholePeriod) {
            // Inclusive of today, so a span of thirty days covers thirty and not
            // thirty-one.
            filter.from = QDate::currentDate().addDays(-(days - 1));
        }

        filter.direction = ui->comboBoxTransactionDirection->currentData()
                               .value<Storage::Direction>();

        transactionModel->setFilter(filter);
        applyTransactionNotice();
    }

    void resetTransactionFilter()
    {
        const QSignalBlocker searchBlocker(ui->lineEditTransactionSearch);
        const QSignalBlocker periodBlocker(ui->comboBoxTransactionPeriod);
        const QSignalBlocker directionBlocker(ui->comboBoxTransactionDirection);

        ui->lineEditTransactionSearch->clear();
        ui->comboBoxTransactionPeriod->setCurrentIndex(0);
        ui->comboBoxTransactionDirection->setCurrentIndex(0);

        // Blocked above so that three changes do not make three requests. One
        // request carries the whole reset.
        applyTransactionFilter();
    }

    /**
     * Puts the number of transactions under the filter into the counter, and
     * with it how far the loading has come.
     *
     * One place for both. While rows are still missing the counter names the
     * loaded number beside the whole one, and once the holding is through it
     * names the one. A second element beside it would say the same thing twice,
     * and a progress bar that started over with every page would say nothing
     * about the holding at all.
     */
    void applyTransactionCount()
    {
        const int total = transactionModel == nullptr ? 0 : transactionModel->totalRows();

        if (transactionModel == nullptr || transactionModel->atEnd()) {
            ui->labelTransactionCount->setText(AppCentralWidget::tr("%n transaction(s)", "", total));
            return;
        }

        //: %1 is the number of transactions loaded so far, %n the whole holding
        ui->labelTransactionCount->setText(AppCentralWidget::tr("%1 of %n transaction(s)", "", total)
                                               .arg(transactionModel->rowCount()));
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

    void setTransactionModel(TransactionTableModel *model)
    {
        if (transactionModel) {
            QObject::disconnect(transactionModel, nullptr, q_ptr, nullptr);
        }

        transactionModel = model;
        ui->tableViewTransactions->setModel(model);

        // Without a model there is nothing to order. The indicator is set before
        // sorting is switched on, because switching it on orders by whatever the
        // indicator says at that moment.
        ui->tableViewTransactions->setSortingEnabled(false);

        if (transactionModel) {
            const auto refresh = [this] {
                applyTransactionCount();
                applyTransactionNotice();
            };

            QObject::connect(transactionModel, &QAbstractItemModel::modelReset, q_ptr, refresh);
            QObject::connect(transactionModel, &QAbstractItemModel::rowsInserted, q_ptr, refresh);
            QObject::connect(transactionModel, &QAbstractItemModel::rowsRemoved, q_ptr, refresh);

            QObject::connect(transactionModel,
                             &TransactionTableModel::totalRowsChanged,
                             q_ptr,
                             [this] { applyTransactionCount(); });

            ui->tableViewTransactions->horizontalHeader()
                ->setSortIndicator(transactionModel->sortColumn(), transactionModel->sortOrder());
            ui->tableViewTransactions->setSortingEnabled(true);

            applyTransactionCount();
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
        const bool accountChosen
            = transactionNotice == AppCentralWidget::TransactionNotice::AccountWithoutTransactions;

        // Visible without an account, and not operable. A bar that comes and
        // goes moves what stands below it, and an assistive tool can only say
        // that a control exists while it is there.
        ui->widgetTransactionFilter->setEnabled(accountChosen);

        if (transactionModel != nullptr && transactionModel->rowCount() > 0) {
            ui->stackedWidgetTransactions->setCurrentWidget(ui->pageTransactionTable);
            return;
        }

        // The fourth state: an account that holds transactions of which none
        // meets the filter. It is the only one the user can undo where he stands,
        // so it is the only one with a button.
        const bool filterTookThemAway = accountChosen && transactionModel != nullptr
                                        && transactionModel->filter().isSet();

        // Choosing a bank is no choice of an account, so it shares the headline
        // of the state where nothing is chosen at all.
        const bool withoutAnAccount = !accountChosen;

        if (filterTookThemAway) {
            ui->labelTransactionsHeadline->setText(AppCentralWidget::tr("No transaction matches"));
            ui->labelTransactionsNotice->setText(
                AppCentralWidget::tr("This account holds transactions, but none of them meets the "
                                     "filter above."));
        } else {
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
                    AppCentralWidget::tr("A bank only groups the accounts it keeps. Choose one of "
                                         "them to see its transactions."));
                break;
            case AppCentralWidget::TransactionNotice::AccountWithoutTransactions:
                ui->labelTransactionsNotice->setText(
                    AppCentralWidget::tr("This account holds no transactions yet."));
                break;
            }
        }

        ui->pushButtonTransactionsNoticeReset->setVisible(filterTookThemAway);
        ui->stackedWidgetTransactions->setCurrentWidget(ui->pageTransactionsNotice);

        announceTransactionNotice();
    }

    /**
     * Has the assistive tools read out what the area now says.
     *
     * A label that changes its text raises no event of its own and reaches
     * nobody who is not looking at it. An announcement asks for the words
     * themselves to be spoken, which a state change would not do, and it does
     * so without taking the focus away from wherever it is.
     *
     * Politely: the user is not to be interrupted mid-sentence for a view that
     * has nothing in it.
     */
    void announceTransactionNotice()
    {
        const auto message = QStringLiteral("%1. %2").arg(ui->labelTransactionsHeadline->text(),
                                                          ui->labelTransactionsNotice->text());

        QAccessibleAnnouncementEvent event(ui->labelTransactionsHeadline, message);
        event.setPoliteness(QAccessible::AnnouncementPoliteness::Polite);

        QAccessible::updateAccessibility(&event);
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
    TransactionTableModel *transactionModel = nullptr;
    QTimer *searchTimer = nullptr;
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

void AppCentralWidget::setTransactionModel(TransactionTableModel *model)
{
    d_ptr->setTransactionModel(model);
}

void AppCentralWidget::resetTransactionFilter()
{
    d_ptr->resetTransactionFilter();
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
