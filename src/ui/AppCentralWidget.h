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
#pragma once

#include "ui/Models/StandingOrderTableModel.h"
#include "ui/Models/TransactionTableModel.h"

#include <QtWidgets/QMainWindow>
#include <QtWidgets/QTreeView>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE
class QAbstractItemModel;
QT_END_NAMESPACE

namespace olbaflinx::ui {

/**
 * The central area of the main window.
 *
 * Ownership: belongs to its parent widget, as every QWidget does. The window
 * passed to initialize() is only borrowed, it is not kept beyond the call.
 */
class AppCentralWidget : public QWidget
{
    Q_OBJECT

public:
    /**
     * The two pages the central area carries.
     *
     * Storages is what stands there before a storage is open, Banking what comes
     * after it. The numbers are the ones the QStackedWidget counts by.
     */
    enum class Page { Storages = 0, Banking = 1 };
    Q_ENUM(Page)

    /**
     * Why the transaction view has nothing to show.
     *
     * Three states, and the view names the one it is in rather than leaving the
     * area blank. A blank area could as well be a failure.
     *
     * BankSelected shares its headline with NoAccountSelected: choosing a bank
     * is no choice of an account. What it carries of its own is the explanation,
     * that a bank groups its accounts and that one of them is what to pick.
     */
    enum class TransactionNotice {
        NoAccountSelected,
        BankSelected,
        AccountWithoutTransactions,
    };
    Q_ENUM(TransactionNotice)

    /**
     * Why the standing order view has nothing to show.
     *
     * The same three states the transaction view knows, for the same reason: a
     * blank area could as well be a failure. An account whose orders have all
     * ended lands in the third of them, and that it cannot be told from an
     * account that never had one is the answer of the specification, not an
     * oversight.
     */
    enum class StandingOrderNotice {
        NoAccountSelected,
        BankSelected,
        AccountWithoutStandingOrders,
    };
    Q_ENUM(StandingOrderNotice)

    /**
     * Clears the filter bar and with it the restriction on the model.
     *
     * The filter outlives a change of account, so that a user who is looking for
     * something keeps looking for it. It does not outlive the storage.
     */
    void resetTransactionFilter();

    explicit AppCentralWidget(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
    ~AppCentralWidget() override;

    void initialize(QMainWindow *window);

    /**
     * Shows one of the two pages. The other one stays built.
     *
     * Closing a storage drops the records of the models, not the widgets of the
     * page that is left.
     */
    void setPage(Page page);

    [[nodiscard]] Page page() const;

    /**
     * Puts the overview of the storages onto the first page.
     *
     * The overview needs the storage of the application and is therefore built
     * by the window, not here. It becomes a child of this widget through the
     * layout of the page. The widget is taken over.
     */
    void setStorageOverview(QWidget *overview);

    /**
     * The tree of accounts, grouped by bank. Never null once the widget is
     * built.
     */
    [[nodiscard]] QTreeView *accountWidget() const;

    /**
     * The second page, the one that carries the dock areas.
     *
     * The dock manager is built by the window and needs a parent that is not the
     * window itself: a dock manager whose parent is a QMainWindow makes itself
     * the central widget and would push out the stack that holds both pages.
     *
     * Its layout is empty once the two panels below have been taken over by
     * dock widgets.
     */
    [[nodiscard]] QWidget *bankingPage() const;

    /**
     * Everything the accounts side shows: the tree and its notice.
     *
     * Not the tree alone. The notice about accounts that are missing or could
     * not be read stands in the same place, and a dock area that held only the
     * tree would lose it. Never null once the widget is built.
     */
    [[nodiscard]] QWidget *accountPanel() const;

    /**
     * Everything the transactions side shows, with its tabs. Never null once
     * the widget is built.
     */
    [[nodiscard]] QWidget *transactionPanel() const;

    /**
     * Hands the accounts to the view and takes over showing the notices.
     *
     * The view keeps no records of its own, so an empty tree and a tree that was
     * never filled look the same. This is where the difference is told: as long
     * as the model reports no row, the notice from setUpAccountNotice() stands in
     * its place.
     *
     * The model is owned elsewhere and has to outlive this widget. Passing
     * nullptr detaches the view.
     */
    void setAccountModel(QAbstractItemModel *model);

    /**
     * Says that the accounts could not be read.
     *
     * A failed read is not an empty storage. What the view already shows stays
     * where it is; only a view that has nothing to show trades the notice about
     * the missing accounts for this one. The message names no file and no
     * statement.
     */
    void showAccountsUnreadable(const QString &message);

    /**
     * Hands the transactions to the view and takes over the empty states.
     *
     * As long as the model reports no row, the notice for the state set through
     * setTransactionNotice() stands in place of the table.
     *
     * The concrete type, not the interface: the filter bar of this widget sets
     * the restriction on the model and reads the number the model was told.
     *
     * The model is owned elsewhere and has to outlive this widget. Passing
     * nullptr detaches the view.
     */
    void setTransactionModel(olbaflinx::ui::models::TransactionTableModel *model);

    /**
     * Hands the standing orders to the view and takes over the empty states.
     *
     * As long as the model reports no row, the notice for the state set through
     * setStandingOrderNotice() stands in place of the table.
     *
     * The concrete type, not the interface: the view follows the order of the
     * model, and only the model says which column carries it.
     *
     * The model is owned elsewhere and has to outlive this widget. Passing
     * nullptr detaches the view.
     */
    void setStandingOrderModel(olbaflinx::ui::models::StandingOrderTableModel *model);

    /**
     * Says why the standing order view is empty.
     *
     * Only read while the model reports no row. Whoever changes the selection
     * sets it along with the account, so that the right text is in place by the
     * time the read comes back empty.
     */
    void setStandingOrderNotice(StandingOrderNotice notice);

    /**
     * Reads the transactions again and puts the view back where it stood.
     *
     * What a fetch needs afterwards. The model starts over, which resets the
     * view, so the position of the scroll bar is taken beforehand and set again
     * once the rows are back. Without that the view would jump to the top of a
     * holding the user was in the middle of.
     */
    void refreshTransactions();

    /**
     * Says why the transaction view is empty.
     *
     * Only read while the model reports no row. Whoever changes the selection
     * sets it along with the account, so that the right text is in place by the
     * time the read comes back empty.
     */
    void setTransactionNotice(TransactionNotice notice);

private:
    class Private;
    Private *d_ptr = nullptr;
};

} // namespace olbaflinx::ui
