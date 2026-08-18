/**
 * Copyright (C) 2022-2026, Alexander Saal <developer@olbaflinx.chm-projects.de>
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the
 * GNU General Public License as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
 * even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "ui/App.h"

#include "core/Banking/Banking.h"
#include "core/Logger/Logger.h"
#include "core/Storage/Storage.h"
#include "ui/AccountFetch.h"
#include "ui/AppCentralWidget.h"
#include "ui/Assistant/SetupAssistant.h"
#include "ui/ErrorMessage.h"
#include "ui/Logging.h"
#include "ui/Models/AccountTreeModel.h"
#include "ui/Models/TransactionTableModel.h"
#include "ui/Storage/StorageDialog.h"

#include "ui_App.h"

#include <QtCore/QDir>
#include <QtCore/QScopeGuard>
#include <QtCore/QTimer>

#include <QtGui/QAccessible>
#include <QtGui/QAccessibleAnnouncementEvent>
#include <QtGui/QCloseEvent>

#include <QtWidgets/QApplication>
#include <QtWidgets/QLayout>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QStatusBar>

#include <qtadvanceddocking-qt6/DockAreaWidget.h>
#include <qtadvanceddocking-qt6/DockManager.h>
#include <qtadvanceddocking-qt6/DockWidget.h>

namespace {

/**
 * The names under which a saved layout finds its areas again.
 *
 * The dock manager takes the object name of an area as the key of the saved
 * state, and it takes the title for it unless one is set. A title is
 * translatable, so a layout saved in one language would no longer be found in
 * another. These names are set apart from the titles and never change.
 */
const QString TransactionDockName = QStringLiteral("transactionDock");
const QString AccountDockName = QStringLiteral("accountDock");

/**
 * Where the window keeps what it remembers between two runs.
 *
 * The arrangement of the areas sits beside the position and the size of the
 * window, in plain settings and not in the encrypted storage: a layout is no
 * secret, and it has to be readable before any storage is opened.
 */
const QString WindowGroup = QStringLiteral("App");
const QString PositionKey = QStringLiteral("Position");
const QString SizeKey = QStringLiteral("Size");
const QString DockLayoutKey = QStringLiteral("DockLayout");

/**
 * The number a saved arrangement carries along.
 *
 * The dock manager compares it on restore and refuses a state that carries a
 * different one. Raising it is how a rework of the areas retires the layouts of
 * every earlier run at once, instead of applying them to areas they were never
 * written for.
 */
constexpr int DockLayoutVersion = 1;

/**
 * How long the refresh after a fetch waits before it asks again whether the
 * storage is free.
 *
 * Short enough to pass unnoticed, long enough not to ask a thousand times over
 * a read that takes a moment.
 */
constexpr int RefreshRetryMs = 50;

/**
 * How long the window waits after the last move or resize before it writes its
 * geometry down.
 *
 * Qt delivers an event for every step of a drag, and a write goes through
 * QSettings::sync, which puts the whole file out and reads it back in the thread
 * that draws. Long enough that one drag makes one write, short enough that a
 * window closed right afterwards still has it.
 */
constexpr int GeometrySaveDelayMs = 400;

} // namespace

using namespace olbaflinx::core;
using namespace olbaflinx::ui;
using namespace olbaflinx::ui::assistant;

using namespace olbaflinx::core::storage;
using namespace olbaflinx::core::banking;
using namespace olbaflinx::core::logger;
using namespace olbaflinx::ui::models;
using namespace olbaflinx::ui::storage;

using namespace ads;

class App::Private
{
public:
    explicit Private(App *app, Logger *appLogger, Storage *appStorage, ApplicationInfo info)
        : logger(appLogger)
        , storage(appStorage)
        , accountTreeModel(new AccountTreeModel(app))
        , transactionTableModel(new TransactionTableModel(app))
        , fetch(new AccountFetch(std::move(info), appStorage, app))
        , ui(new Ui::UiApp)
        , dockManager(nullptr)
        , centralDockWidget(nullptr)
        , accountDockWidget(nullptr)
        , overview(nullptr)
        , q_ptr(app)
    {
        // Connected before the log is opened, because a file that cannot be
        // opened reports it during the call.
        QObject::connect(logger, &Logger::logFileUnavailable, q_ptr, [this] {
            // Through the event loop, because this runs before setupUi. Asking
            // for the status bar here builds an empty one, which setupUi then
            // replaces by the one of the form; the message would go to the bar
            // that was thrown away and the user would never see it.
            //
            // Not the way of an error from core: that way leads into the log
            // that is missing.
            QTimer::singleShot(0, q_ptr, [this] {
                q_ptr->statusBar()->showMessage(
                    App::tr("No log is being kept. The program runs on, but a report about a "
                            "failure will carry no cause."));
            });
        });

        logger->enable(Logger::LoggerLevel::Notice, Logger::defaultLogFile());

        ui->setupUi(q_ptr);

        QApplication::setWindowIcon(QIcon(QStringLiteral(":/app/olbaflinx-logo-128")));
        q_ptr->setWindowIconText(QApplication::applicationName());

        // Every failure core reports on an asynchronous path ends up here, from
        // either run. Without this the signals had no receiver at all and the
        // user saw nothing. What the two are told apart for is the state of the
        // run, and the window shows both the same way.
        QObject::connect(storage, &Storage::readFailed, q_ptr, &App::showError);
        QObject::connect(storage, &Storage::writeFailed, q_ptr, &App::showError);

        // A request the storage turned down never becomes a run and therefore
        // never reaches either of the two above. The model says so itself.
        QObject::connect(transactionTableModel,
                         &TransactionTableModel::readRefused,
                         q_ptr,
                         &App::showError);

        // The window writes its position and its size once it comes to rest.
        geometryTimer = new QTimer(q_ptr);
        geometryTimer->setSingleShot(true);
        geometryTimer->setInterval(GeometrySaveDelayMs);

        QObject::connect(geometryTimer, &QTimer::timeout, q_ptr, [this] { saveGeometry(); });
    }

    ~Private()
    {
        // A move or a resize the window was closed on still has its write
        // pending, and the timer will not fire any more.
        if (geometryTimer != nullptr && geometryTimer->isActive()) {
            geometryTimer->stop();
            saveGeometry();
        }

        // Here and not in a close event: the entry for quitting ends the program
        // without one, and an arrangement that only survives the window button
        // would be lost on the other way out.
        saveDockLayout();

        // Logger and Storage belong to whoever created the window. The logger is
        // only shut down here, neither of the two is released.
        logger->disable();

        delete ui;
    }

    void showAbout() const
    {
        QMessageBox::about(q_ptr,
                           App::tr("About %1").arg(QApplication::applicationName()),
                           App::tr("<h3>%1 %2</h3>"
                                   "<p>Multibank-capable online banking software for Linux.</p>"
                                   "<p><a href=\"%3\">%3</a></p>")
                               .arg(QApplication::applicationName(),
                                    QApplication::applicationVersion(),
                                    QApplication::organizationDomain()));
    }

    /**
     * Wires the menu and fills the tool bar.
     */
    void setUpActions()
    {
        QObject::connect(ui->appAboutAction, &QAction::triggered, q_ptr, [this] { showAbout(); });

        QObject::connect(ui->appNewStorageAction, &QAction::triggered, q_ptr, [this] {
            overview->addStorage();
        });

        QObject::connect(ui->appCloseStorageAction, &QAction::triggered, q_ptr, [this] {
            q_ptr->closeStorage();
        });

        // Closed rather than quit, so that the entry from the menu goes the same
        // way as the button of the window manager. Quitting outright steps past
        // closeEvent and with it past the refusal to leave while a fetch runs.
        // The shutdown of the banking layer waits for the session, and a session
        // reports its course into this thread and waits for that.
        QObject::connect(ui->appQuitAction, &QAction::triggered, q_ptr, [this] { q_ptr->close(); });

        // The window does not know what a wizard needs to be built. The assembly
        // does, so the request travels there and the result comes back through
        // setAccounts like any other.
        QObject::connect(ui->appSetupAssistantAction, &QAction::triggered, q_ptr, [this] {
            Q_EMIT q_ptr->setupAssistantRequested();
        });

        QObject::connect(overview, &StorageDialog::storageOpened, q_ptr, [this] {
            applyPage(AppCentralWidget::Page::Banking);
        });

        QObject::connect(overview, &StorageDialog::message, q_ptr, &App::showMessage);

        QObject::connect(ui->appResetLayoutAction, &QAction::triggered, q_ptr, [this] {
            resetDockLayout();
        });

        QObject::connect(ui->appFetchTransactionsAction, &QAction::triggered, q_ptr, [this] {
            fetchTheChosenAccount();
        });

        // The key the platform offers for fetching something anew, rather than
        // one chosen here. None of the other entries carries it.
        ui->appFetchTransactionsAction->setShortcut(QKeySequence::Refresh);

        // The second command, and it hangs on no choice in the tree: whoever
        // wants the whole holding has nothing to pick first.
        QObject::connect(ui->appFetchAllTransactionsAction, &QAction::triggered, q_ptr, [this] {
            fetchEveryAccount();
        });

        // The third way to the command, beside the menu and the tool bar. A
        // plain addAction would leave it unreachable: the tree stands on the
        // default policy and shows no menu of its own for the actions it holds.
        auto *const accountView = ui->appCentralWidget->accountWidget();
        accountView->addAction(ui->appFetchTransactionsAction);
        accountView->setContextMenuPolicy(Qt::ActionsContextMenu);

        setUpFetch();

        ui->appToolBar->addAction(ui->appSetupAssistantAction);
        ui->appToolBar->addAction(ui->appFetchTransactionsAction);
        ui->appToolBar->addAction(ui->appFetchAllTransactionsAction);
        ui->appToolBar->addSeparator();
        ui->appToolBar->addAction(ui->appCloseStorageAction);
    }

    /**
     * Takes the outcome of a fetch and turns it into what the window shows.
     *
     * The state of the entries is held here rather than asked of the fetch: what
     * the window switches off is decided by the two moments, not by a second
     * reading of a state that lives elsewhere.
     */
    void setUpFetch()
    {
        QObject::connect(fetch, &AccountFetch::started, q_ptr, [this] {
            fetchIsRunning = true;
            applyActionStates();
        });

        QObject::connect(fetch,
                         &AccountFetch::ended,
                         q_ptr,
                         [this](AccountFetch::Outcome outcome, int storedCount, const QString &) {
                             fetchIsRunning = false;
                             applyActionStates();

                             q_ptr->statusBar()->showMessage(outcomeMessage(outcome, storedCount));

                             if (outcome == AccountFetch::Outcome::Received
                                 || outcome == AccountFetch::Outcome::BalanceOnly) {
                                 // Through the event loop, so that whatever the
                                 // storage still has queued is delivered first.
                                 // The refresh asks it whether it is reading,
                                 // and an answer given before that queue is
                                 // empty is out of date.
                                 QTimer::singleShot(0, q_ptr, [this] { refreshFromStorage(); });
                             }
                         });

        QObject::connect(fetch,
                         &AccountFetch::allEnded,
                         q_ptr,
                         [this](const AccountFetch::Summary &summary) {
                             fetchIsRunning = false;
                             applyActionStates();

                             // The one sign the user has that the run is over.
                             // The progress window of the library comes and goes
                             // once per institution and says nothing about the
                             // end of the whole fetch.
                             q_ptr->statusBar()->showMessage(collectiveMessage(summary));

                             // Asked on every way out that could have written
                             // something, the count included: a run that brought
                             // balances alone writes rows without raising it.
                             const bool nothingWasWritten = summary.outcome
                                                                == AccountFetch::Outcome::Failed
                                                            || (summary.outcome
                                                                    == AccountFetch::Outcome::Aborted
                                                                && !summary.keptAfterAbort);

                             if (!nothingWasWritten) {
                                 QTimer::singleShot(0, q_ptr, [this] { refreshFromStorage(); });
                             }
                         });

        // The question belongs to the window: the fetch shows nothing. Nothing
        // of the run stands in the file while it is open, so the answer decides
        // whether anything is written at all.
        QObject::connect(fetch, &AccountFetch::abortNeedsAnswer, q_ptr, [this] {
            askWhetherToKeep();
        });
    }

    /**
     * Asks whether what an interrupted collective fetch had already brought is
     * written, and hands the answer back.
     *
     * Modal on purpose: the records are held nowhere else, and a question that
     * can be walked past would lose them to a click elsewhere.
     */
    void askWhetherToKeep()
    {
        QMessageBox question(q_ptr);

        question.setObjectName(QStringLiteral("appFetchAbortQuestion"));
        question.setIcon(QMessageBox::Question);
        question.setWindowTitle(App::tr("Fetch stopped"));
        question.setText(App::tr("The fetch was stopped. Keep what has already been fetched?"));
        question.setInformativeText(
            App::tr("What was fetched is not stored yet. Discarding it leaves your holding as it "
                    "was before the fetch."));

        auto *const keep = question.addButton(App::tr("&Keep"), QMessageBox::AcceptRole);
        auto *const discard = question.addButton(App::tr("&Discard"), QMessageBox::DestructiveRole);

        keep->setObjectName(QStringLiteral("appFetchKeepButton"));
        discard->setObjectName(QStringLiteral("appFetchDiscardButton"));

        // Keeping is what a stray press of the return key answers. What was
        // fetched cost minutes on the line and is gone for good the other way.
        question.setDefaultButton(keep);
        question.setEscapeButton(keep);

        question.exec();

        fetch->answerAbort(question.clickedButton() != discard);
    }

    /**
     * What the status bar carries once a fetch is over.
     *
     * Never an IBAN, an account number or an amount: the bar is read out to
     * assistive tools, and what is spoken in a room is not the place for them.
     */
    static QString outcomeMessage(AccountFetch::Outcome outcome, int storedCount)
    {
        switch (outcome) {
        case AccountFetch::Outcome::Received:
            // Nought is said in words of its own. The same sentence with a nought
            // in it reads like a fetch that went wrong.
            return storedCount == 0
                       ? App::tr("The fetch is through. No new transactions came in.")
                       : App::tr("The fetch is through. %n new transaction(s) came in.",
                                 "",
                                 storedCount);
        case AccountFetch::Outcome::BalanceOnly:
            return App::tr("The balance is up to date. Your bank offers no transactions for this "
                           "account.");
        case AccountFetch::Outcome::Skipped:
            return App::tr("This account has no online access, so nothing was fetched.");
        case AccountFetch::Outcome::NothingOffered:
            return App::tr("Your bank offers neither transactions nor a balance for this account, "
                           "so nothing was fetched.");
        case AccountFetch::Outcome::Aborted:
            return App::tr("The fetch was stopped. Nothing of this account was stored.");
        case AccountFetch::Outcome::StoreFailed:
            // The bookings are written before the balance and in a run of their
            // own. A count that survived the failure says the failure came after
            // them, and telling the user nothing was kept would send them into a
            // second fetch that finds those records already there.
            return storedCount == 0
                       ? App::tr("The fetch could not be stored and nothing of it was kept. "
                                 "Please fetch again.")
                       : App::tr("%n new transaction(s) came in, but the balance could not be "
                                 "stored.",
                                 "",
                                 storedCount);
        case AccountFetch::Outcome::Failed:
            return App::tr("The fetch failed. Your bank could not be reached, or it refused the "
                           "request.");
        }

        return {};
    }

    /**
     * What the status bar carries once a fetch over all accounts is over.
     *
     * Four figures, because three would leave a sum that does not add up: an
     * account without online access is neither fetched nor failed. They are
     * named rather than put into a sentence, so that no count has to agree with
     * a word around it in any language.
     *
     * Never an IBAN, an account number or an amount, for the reason the message
     * of a single fetch gives.
     */
    static QString collectiveMessage(const AccountFetch::Summary &summary)
    {
        const QString figures = App::tr("Fetched: %1. Skipped: %2. Failed: %3. New transactions: "
                                        "%4.")
                                    .arg(summary.fetched)
                                    .arg(summary.skipped)
                                    .arg(summary.failed)
                                    .arg(summary.storedCount);

        switch (summary.outcome) {
        case AccountFetch::Outcome::Aborted:
            // Told apart, because the two leave a different holding behind and
            // the user has just chosen which.
            return summary.keptAfterAbort
                       ? App::tr("The fetch was stopped. What had already been fetched was kept. "
                                 "%1")
                             .arg(figures)
                       : App::tr("The fetch was stopped. Nothing of it was stored.");

        case AccountFetch::Outcome::StoreFailed:
            return App::tr("The fetch could not be stored in full. Please fetch again. %1")
                .arg(figures);

        case AccountFetch::Outcome::Failed:
            return summary.reason.isEmpty()
                       ? App::tr("The fetch failed. Your bank could not be reached, or it refused "
                                 "the request.")
                       : summary.reason;

        case AccountFetch::Outcome::Received:
        case AccountFetch::Outcome::BalanceOnly:
        case AccountFetch::Outcome::Skipped:
        case AccountFetch::Outcome::NothingOffered:
            break;
        }

        return App::tr("The fetch is through. %1").arg(figures);
    }

    /** The account the tree has chosen, or an empty pointer for anything else. */
    [[nodiscard]] std::shared_ptr<Account> chosenAccount() const
    {
        return accountTreeModel->accountAt(ui->appCentralWidget->accountWidget()->currentIndex());
    }

    /**
     * Every account the tree holds, banks first and their accounts under them.
     *
     * Read out of the model rather than out of the storage: the tree is what the
     * user sees, and an account he cannot see is not one he asked to fetch.
     */
    [[nodiscard]] QList<std::shared_ptr<Account>> everyAccount() const
    {
        QList<std::shared_ptr<Account>> accounts;

        for (int bank = 0; bank < accountTreeModel->rowCount(); ++bank) {
            const QModelIndex bankIndex = accountTreeModel->index(bank, 0);

            for (int row = 0; row < accountTreeModel->rowCount(bankIndex); ++row) {
                if (auto account = accountTreeModel->accountAt(
                        accountTreeModel->index(row, 0, bankIndex));
                    account != nullptr) {
                    accounts.append(std::move(account));
                }
            }
        }

        return accounts;
    }

    void fetchEveryAccount()
    {
        const auto accounts = everyAccount();
        if (accounts.isEmpty()) {
            return;
        }

        // Said before anything goes out, for the reason the single fetch gives:
        // the starting points are read first, and until the progress window of
        // the banking layer stands the command would be unacknowledged.
        q_ptr->statusBar()->showMessage(App::tr("Your bank is being contacted."));

        fetch->startAll(accounts);
    }

    void fetchTheChosenAccount()
    {
        const auto account = chosenAccount();
        if (account == nullptr) {
            return;
        }

        // Said before anything goes out. Reading the starting point takes a
        // moment, and until the progress window of the banking layer stands the
        // command would otherwise be unacknowledged.
        q_ptr->statusBar()->showMessage(App::tr("Your bank is being contacted."));

        fetch->start(account);
    }

    /**
     * Brings what was written into the storage onto the screen, whoever wrote
     * it: a fetch of this window, or the wizard from outside it.
     *
     * The accounts first and the transactions after them: both go over the read
     * path of the storage, and that takes one run at a time. The accounts carry
     * the new balance, and reading them empties the tree, which takes the choice
     * of the user with it - so the choice is put back before the transactions
     * are asked for.
     */
    void refreshFromStorage()
    {
        // Both ways in here outlive the file: the retry below fires after it was
        // closed, and the write of the wizard ends whether the window still has
        // it open or not. A read on a closed storage answers with a failure that
        // reads as a damaged file, and would send the user to a backup over a
        // vault that is sound.
        if (!storage->isOpen()) {
            return;
        }

        // The storage takes one read at a time. A read of the transactions may
        // well be going here, and a refusal that reached the receivers below
        // would be taken for the end of the run they are waiting for: the tree
        // would keep the balance of the fetch before, and the choice of the user
        // would go with it.
        //
        // Asked first and listened to afterwards. A call that starts no run says
        // so through its return value and emits nothing, so nothing can reach a
        // connection that is not there yet, and the run that does start cannot
        // report before the event loop is entered again.
        if (const auto error = storage->receiveItems(
                {.type = Storage::StorageAccount, .limit = Storage::MaxItemsPerQuery});
            error.isError()) {
            // A busy storage is a moment, not a failure. Asked again rather than
            // hung on the end of the read that holds the way: its signal may
            // already be in the queue, and a connection made now would not be
            // among the receivers it was emitted to.
            if (error.code() == ErrorCode::Busy) {
                QTimer::singleShot(RefreshRetryMs, q_ptr, [this] { refreshFromStorage(); });
                return;
            }

            q_ptr->showError(error.code(), error.message());
            return;
        }

        const quint32 chosen = transactionTableModel->accountId();

        QObject::connect(storage,
                         &Storage::itemsReceived,
                         q_ptr,
                         &App::setAccounts,
                         Qt::SingleShotConnection);

        // The tree shows the whole holding and knows no second page, so the
        // window is opened as wide as a read may go. What lies beyond it would
        // otherwise leave the tree without a word, and with it the account the
        // user had chosen.
        QObject::connect(
            storage,
            &Storage::itemsCounted,
            q_ptr,
            [](int count) {
                if (count > Storage::MaxItemsPerQuery) {
                    qCWarning(lcUi)
                        << "the storage holds" << count << "accounts and the tree shows the first"
                        << Storage::MaxItemsPerQuery;
                }
            },
            Qt::SingleShotConnection);

        QObject::connect(
            storage,
            &Storage::readFinished,
            q_ptr,
            [this, chosen] {
                // A single shot connection only parts once its signal has fired.
                // Every way out of a read that brings no record leaves both of
                // the two above standing, and the empty account table is one the
                // application itself treats as an everyday case. They would then
                // take the result of the next read, which carries transactions;
                // the tree cannot build an account from one, so it would empty
                // itself and drop the choice of the user with it.
                QObject::disconnect(storage, &Storage::itemsReceived, q_ptr, nullptr);
                QObject::disconnect(storage, &Storage::itemsCounted, q_ptr, nullptr);

                restoreSelection(chosen);
                ui->appCentralWidget->refreshTransactions();
            },
            Qt::SingleShotConnection);
    }

    /**
     * Puts the choice of the user back on the account it stood on.
     *
     * The tree is rebuilt from the ground up by a read, and an index of the run
     * before points nowhere afterwards. The identifier survives it, which is
     * what the account is found again by.
     */
    void restoreSelection(quint32 uniqueAccountId)
    {
        if (uniqueAccountId == 0) {
            return;
        }

        const auto matches = accountTreeModel->match(accountTreeModel->index(0, 0),
                                                     AccountTreeModel::UniqueIdRole,
                                                     uniqueAccountId,
                                                     1,
                                                     Qt::MatchExactly | Qt::MatchRecursive);

        if (!matches.isEmpty()) {
            ui->appCentralWidget->accountWidget()->setCurrentIndex(matches.constFirst());
        }
    }

    /**
     * Shows a page and puts the controls into the state that belongs to it.
     *
     * Every command in the tool bar needs an open storage, so the bar itself
     * only belongs on the second page. The menu entries stay where they are and
     * turn grey instead, so that the menu does not change shape underneath the
     * user while he learns it.
     */
    void applyPage(AppCentralWidget::Page page)
    {
        ui->appCentralWidget->setPage(page);

        // A message belongs to the page it was raised on. "Nothing was found,
        // import your accounts" says nothing on the overview, where there is no
        // storage to import into.
        q_ptr->statusBar()->clearMessage();

        const bool storageIsOpen = page == AppCentralWidget::Page::Banking;

        ui->appToolBar->setVisible(storageIsOpen);

        applyActionStates();

        // Where the keyboard starts on this page. The focus chain is a ring, so
        // which of the two areas comes first is decided by where the walk
        // begins, not by their order in the chain: without this it begins at the
        // area the dock manager built first, and that has to be the central one
        // because the library refuses any other as the first. The accounts stand
        // left of the transactions and are what a user picks from, so the walk
        // starts there and reaches the transactions next.
        if (storageIsOpen) {
            ui->appCentralWidget->accountWidget()->setFocus(Qt::OtherFocusReason);
        }

        // Held back from the start until the areas are on screen, and said once.
        // Repeating it every time a storage is opened would nag about something
        // that was over with the first arrangement that got saved.
        if (storageIsOpen && dockLayoutFellBack) {
            dockLayoutFellBack = false;

            q_ptr->statusBar()->showMessage(
                App::tr("Your arrangement of the areas could not be restored. The standard "
                        "arrangement is in place, and there is nothing you need to do."));
        }
    }

    /**
     * Which commands grip right now.
     *
     * Every one of them needs an open storage, and the fetch needs an account
     * on top of that. None of them is taken away while it does not grip: an
     * entry that turns grey tells an assistive tool that the command exists and
     * that it does not apply, and one that is gone tells it nothing.
     *
     * A running fetch switches off everything that could pull the ground from
     * under it: closing the storage would leave the half stored account the
     * writing path goes to lengths to prevent, and the wizard holds a banking
     * instance of its own, which must not run beside a fetch.
     */
    void applyActionStates()
    {
        const bool storageIsOpen = ui->appCentralWidget->page() == AppCentralWidget::Page::Banking;
        const bool idle = !fetchIsRunning;

        ui->appCloseStorageAction->setEnabled(storageIsOpen && idle);
        ui->appSetupAssistantAction->setEnabled(storageIsOpen && idle);
        ui->appFetchTransactionsAction->setEnabled(storageIsOpen && idle
                                                   && chosenAccount() != nullptr);

        // No choice is needed for this one, an account is: a fetch over an empty
        // tree has nothing to ask any bank about.
        ui->appFetchAllTransactionsAction->setEnabled(storageIsOpen && idle
                                                      && accountTreeModel->rowCount() > 0);

        // The areas only stand on the second page, so there is nothing to put
        // back on the first. A fetch does not touch them.
        ui->appResetLayoutAction->setEnabled(storageIsOpen);
    }

    /**
     * Turns what is picked in the tree into the account the transactions are
     * shown for.
     *
     * A bank node is no account: the tree answers its account roles with an
     * invalid value, and that is what tells the two apart. Whatever the state,
     * the notice of the empty transaction view is set along with it, so that the
     * right words are in place by the time a read comes back with nothing.
     */
    void applySelection(const QModelIndex &index)
    {
        const QVariant uniqueId = accountTreeModel->data(index, AccountTreeModel::UniqueIdRole);

        // The fetch hangs on the choice, so every way through this function ends
        // with the entries in the state the new choice leaves them in.
        const auto applyStatesAtTheEnd = qScopeGuard([this] { applyActionStates(); });

        if (!index.isValid()) {
            transactionTableModel->setAccountId(0);
            ui->appCentralWidget->setTransactionNotice(
                AppCentralWidget::TransactionNotice::NoAccountSelected);
            return;
        }

        if (!uniqueId.isValid()) {
            transactionTableModel->setAccountId(0);
            ui->appCentralWidget->setTransactionNotice(
                AppCentralWidget::TransactionNotice::BankSelected);
            return;
        }

        transactionTableModel->setAccountId(uniqueId.toUInt());
        ui->appCentralWidget->setTransactionNotice(
            AppCentralWidget::TransactionNotice::AccountWithoutTransactions);
    }

    /**
     * A single click on an account is what shows its transactions.
     *
     * The tree is refilled whenever the accounts are read, and an account that
     * the user has since deselected is gone from it. The selection then points
     * nowhere, which is a state of its own and not a bank node, so the reset of
     * the model is followed up here rather than waiting for a click.
     */
    void setUpAccountSelection()
    {
        auto *const view = ui->appCentralWidget->accountWidget();
        auto *const selection = view->selectionModel();

        QObject::connect(selection,
                         &QItemSelectionModel::currentChanged,
                         q_ptr,
                         [this](const QModelIndex &current, const QModelIndex &) {
                             applySelection(current);
                         });

        QObject::connect(accountTreeModel, &QAbstractItemModel::modelReset, q_ptr, [this, view] {
            applySelection(view->currentIndex());
        });
    }

    /**
     * Builds the two dock areas of the second page.
     *
     * The manager gets the page as its parent and not the window. With a
     * QMainWindow as parent it makes itself the central widget, and that would
     * push out the stack which carries the overview on its first page.
     *
     * Two things about the order. The configuration flags are static and only
     * reach a manager that is built after them. And a central area has to be the
     * first area the manager is given; the library refuses it once another one
     * stands.
     */
    void setUpDockAreas()
    {
        CDockManager::setConfigFlags(CDockManager::DefaultBaseConfig);
        CDockManager::setConfigFlag(CDockManager::OpaqueSplitterResize, true);
        CDockManager::setConfigFlag(CDockManager::XmlCompressionEnabled, false);
        CDockManager::setConfigFlag(CDockManager::FocusHighlighting, true);
        CDockManager::setConfigFlag(CDockManager::DockAreaHasCloseButton, false);
        CDockManager::setConfigFlag(CDockManager::MiddleMouseButtonClosesTab, false);
        CDockManager::setConfigFlag(CDockManager::AllTabsHaveCloseButton, false);
        CDockManager::setConfigFlag(CDockManager::DockAreaHideDisabledButtons, true);

        auto *const page = ui->appCentralWidget->bankingPage();
        dockManager = new CDockManager(page);

        centralDockWidget = new CDockWidget(dockManager, App::tr("Transactions"));
        centralDockWidget->setObjectName(TransactionDockName);
        centralDockWidget->setWidget(ui->appCentralWidget->transactionPanel());

        auto *const centralArea = dockManager->setCentralWidget(centralDockWidget);
        centralArea->setAllowedAreas(OuterDockAreas);

        // Dragging an area is a matter for the mouse; the library offers no key
        // for it. It stays a convenience: every function of the window is
        // reachable without it, and whoever loses his way in a layout gets the
        // grouping back through the menu.
        accountDockWidget = new CDockWidget(dockManager, App::tr("Accounts"));
        accountDockWidget->setObjectName(AccountDockName);
        accountDockWidget->setFeature(CDockWidget::DockWidgetClosable, false);
        accountDockWidget->setFeature(CDockWidget::DockWidgetFloatable, false);
        accountDockWidget->setWidget(ui->appCentralWidget->accountPanel(),
                                     CDockWidget::ForceNoScrollArea);

        dockManager->addDockWidget(LeftDockWidgetArea, accountDockWidget, centralArea);

        // Last, because taking the two panels over emptied the layout of the
        // page. Handing the manager over before that would have put it beside
        // the very widgets it has just taken.
        page->layout()->addWidget(dockManager);

        // What the user gets back when he asks for the grouping again. Taken
        // here, so that the way to it is the arrangement just built and not a
        // second description of it that could drift away.
        defaultDockLayout = dockManager->saveState(DockLayoutVersion);
    }

    /**
     * Puts the accounts side back where a restore may have taken it from.
     *
     * The feature that marks an area as not closable takes the close button on
     * its tab and nothing besides. A restore applies the saved open state
     * without asking the area about it, and an area the saved state does not
     * know at all is closed and taken out of its dock area. Both count as a
     * successful restore, so falling back to the default arrangement never
     * catches them, and without the accounts there is nothing left to choose an
     * account with.
     */
    void ensureAccountsVisible()
    {
        if (accountDockWidget == nullptr) {
            return;
        }

        if (accountDockWidget->dockAreaWidget() == nullptr) {
            dockManager->addDockWidget(LeftDockWidgetArea,
                                       accountDockWidget,
                                       centralDockWidget->dockAreaWidget());
        }

        if (accountDockWidget->isClosed()) {
            accountDockWidget->toggleView(true);
        }
    }

    /**
     * Writes down where the window stands and how big it is.
     *
     * Read from the window rather than from the event that started the timer:
     * several moves may have happened since, and what is to be restored is where
     * it came to rest.
     */
    void saveGeometry() const
    {
        storage->storeSetting(PositionKey, q_ptr->pos(), WindowGroup);
        storage->storeSetting(SizeKey, q_ptr->size(), WindowGroup);
    }

    void saveDockLayout() const
    {
        if (dockManager == nullptr) {
            return;
        }

        storage->storeSetting(DockLayoutKey, dockManager->saveState(DockLayoutVersion), WindowGroup);
    }

    /**
     * Brings the arrangement of the last run back, or leaves the default one.
     *
     * Whether a saved arrangement still fits is what the restore answers; there
     * is no check of our own beside it. It refuses an empty or unreadable state,
     * one from a format or a version it does not know, and one that misses an
     * area the window carries, and it tries the whole state before it changes
     * anything, so nothing is ever applied in halves.
     */
    void restoreDockLayout()
    {
        const QByteArray state = storage->setting(DockLayoutKey, WindowGroup, QByteArray())
                                     .toByteArray();

        if (state.isEmpty()) {
            return;
        }

        if (!dockManager->restoreState(state, DockLayoutVersion)) {
            qCWarning(lcUi) << "the saved dock layout was refused, the default one stands";

            // Told, not shown. The window opens on the overview, and a word about
            // areas the user cannot see yet would be cleared by the very step that
            // brings them up.
            dockLayoutFellBack = true;
        }

        ensureAccountsVisible();
    }

    /**
     * Puts the grouping back the way the window opens with it.
     *
     * The way out of an arrangement the user can no longer undo. Falling back to
     * the default only catches a state the restore refuses, and an arrangement
     * can be perfectly valid and still leave him stuck. Saved right away, so that
     * the next start does not hand him back what he has just left.
     */
    void resetDockLayout()
    {
        dockManager->restoreState(defaultDockLayout, DockLayoutVersion);

        ensureAccountsVisible();
        saveDockLayout();
    }

    void initialize()
    {
        ui->appCentralWidget->initialize(q_ptr);
        ui->appCentralWidget->setAccountModel(accountTreeModel);

        transactionTableModel->setStorage(storage);
        ui->appCentralWidget->setTransactionModel(transactionTableModel);

        setUpAccountSelection();

        // The overview is the first page of the central area. It needs the
        // storage, which is why it is built here and not in the central
        // widget.
        overview = new StorageDialog(storage, q_ptr);
        ui->appCentralWidget->setStorageOverview(overview);
        overview->initialize(q_ptr);

        setUpActions();
        setUpDockAreas();
        applyPage(AppCentralWidget::Page::Storages);

        // After the areas stand. There is nothing to restore an arrangement onto
        // before that.
        restoreDockLayout();
    }

    Logger *logger;
    Storage *storage;
    AccountTreeModel *accountTreeModel;
    TransactionTableModel *transactionTableModel;

    // Owned by the window through the object hierarchy. It holds the banking
    // instance of the window and comes up on the first fetch, so a window that
    // never fetches never reaches the banking layer.
    AccountFetch *fetch;

    Ui::UiApp *ui;

    // Owned by the second page through the widget hierarchy. The two areas are
    // owned by the manager once they are docked; they are kept here because the
    // restore of a saved layout has to reach the accounts side again.
    CDockManager *dockManager;
    CDockWidget *centralDockWidget;
    CDockWidget *accountDockWidget;
    QByteArray defaultDockLayout;
    bool dockLayoutFellBack = false;

    // Owned by the window. It carries the write of the position and the size,
    // which must not happen once per step of a drag.
    QTimer *geometryTimer = nullptr;

    // Whether a fetch the user started is still going. It steers the entries and
    // the close command, and it is set from the two signals of the fetch rather
    // than read back from it: what the window switches off follows those two
    // moments.
    bool fetchIsRunning = false;

    // The first page of the central area. Owned by the window through the widget
    // hierarchy; kept here because the menu reaches into it.
    StorageDialog *overview;

private:
    App *q_ptr;
};

App::App(Logger *logger,
         Storage *storage,
         ApplicationInfo applicationInfo,
         QWidget *parent,
         const Qt::WindowFlags &flags)
    : QMainWindow(parent, flags)
    , d_ptr(new Private(this, logger, storage, std::move(applicationInfo)))
{
    // Messages reach the bar from four places, and it swaps its text without a
    // sound. This signal is the one point all four pass through. An empty text
    // means the message was taken away, and there is nothing to announce.
    connect(statusBar(), &QStatusBar::messageChanged, this, [this](const QString &message) {
        if (message.isEmpty()) {
            return;
        }

        QAccessibleAnnouncementEvent announcement(statusBar(), message);
        QAccessible::updateAccessibility(&announcement);
    });
}

App::~App()
{
    delete d_ptr;
}

void App::initialize()
{
    const QPoint pos = d_ptr->storage->setting(PositionKey, WindowGroup, QPoint()).toPoint();
    if (!pos.isNull()) {
        move(pos);
    }

    const QSize size = d_ptr->storage->setting(SizeKey, WindowGroup, QSize()).toSize();
    if (!size.isNull() && size.isValid()) {
        resize(size);
    }

    d_ptr->initialize();
}

void App::setAccounts(const BankingItems &items)
{
    d_ptr->accountTreeModel->setItems(items);

    // The fetch over all accounts hangs on the tree holding one, so its state
    // follows every read that fills the tree or empties it.
    d_ptr->applyActionStates();
}

void App::refreshAccounts()
{
    // Through the event loop, so that whatever the storage still has queued is
    // delivered first. The read asks it whether it is busy, and an answer given
    // before that queue is empty is out of date.
    QTimer::singleShot(0, this, [this] { d_ptr->refreshFromStorage(); });
}

void App::closeStorage()
{
    // The page is what says whether a storage is open. Asking the storage itself
    // would answer for the file, and the entry is reachable through its shortcut
    // long before a file was ever opened.
    if (d_ptr->ui->appCentralWidget->page() == AppCentralWidget::Page::Storages) {
        return;
    }

    d_ptr->storage->close();
    d_ptr->accountTreeModel->setItems({});

    // The interface of the banking layer belongs to the window and outlives the
    // storage. Left to the span that empties it after a fetch, a PIN entered for
    // this storage would still be cached while the next one is open.
    d_ptr->fetch->clearPasswordCache();

    // A choice of account does not outlive the storage it was made in. Emptying
    // the tree takes the selection with it, and the transactions of the account
    // that was shown go with it as well. Neither does the filter: it survives a
    // change of account, not the storage it was set in.
    d_ptr->transactionTableModel->setAccountId(0);
    d_ptr->ui->appCentralWidget->resetTransactionFilter();

    // Building the overview is the moment an entry whose file went away leaves
    // the list, so the way back is a good moment to build it.
    d_ptr->overview->reload();

    d_ptr->applyPage(AppCentralWidget::Page::Storages);
}

void App::showError(ErrorCode code, const QString &reason)
{
    // A read that found no record is not a failure. The storage reports it
    // through the same signal as one, with the code for "nothing found", and
    // taken as a failure it would hold the views away from the very notices that
    // are meant for the case: a storage without accounts, an account without
    // transactions, and a filter without a match. The models are empty at this
    // point, which is all those notices need.
    //
    // It is noted rather than reported, and not as a warning: a log that calls
    // it an error says the opposite of what happened.
    if (code == ErrorCode::NotFound) {
        qCDebug(lcUi) << "a read came back empty:" << reason;
        return;
    }

    // The technical message can name a file or a statement, and one out of a
    // foreign library is not translated either. It goes to the log, never to the
    // screen; what the user reads is made from the code alone.
    qCWarning(lcUi) << "error from core:" << reason;

    const QString message = userMessage(code);
    if (message.isEmpty()) {
        return;
    }

    statusBar()->showMessage(message);

    // A failed read is not an empty storage, and the views must not fall into
    // the notice that says nothing is there.
    //
    // Which view it belongs to is what the transaction model answers: while it
    // is reading, the failure is about the transactions, and the accounts on the
    // left are readable. A notice at that view would point at a holding that is
    // in order and hide the tree that shows it.
    //
    // A fetch is the third origin. It reads nothing, so the question above would
    // hand its failure to the accounts, and the tree it covered would be in
    // perfect order. The status bar above carries it, and the outcome of the
    // fetch follows with what it means.
    if (d_ptr->ui->appCentralWidget->page() == AppCentralWidget::Page::Banking
        && !d_ptr->fetchIsRunning && !d_ptr->transactionTableModel->isReading()) {
        d_ptr->ui->appCentralWidget->showAccountsUnreadable(message);
    }
}

void App::showMessage(const QString &message)
{
    statusBar()->showMessage(message);
}

bool App::event(QEvent *event)
{
    return QMainWindow::event(event);
}

void App::moveEvent(QMoveEvent *event)
{
    // Restarted rather than written. Dragging the window delivers an event per
    // step, and each write puts the whole settings file out and reads it back.
    d_ptr->geometryTimer->start();

    QMainWindow::moveEvent(event);
}

void App::resizeEvent(QResizeEvent *event)
{
    d_ptr->geometryTimer->start();

    QMainWindow::resizeEvent(event);
}

void App::closeEvent(QCloseEvent *event)
{
    if (d_ptr->fetchIsRunning) {
        // Not a dialog: the progress window of the banking layer already stands
        // in front of everything, and a second one over it would ask the user to
        // answer the wrong question first.
        statusBar()->showMessage(
            tr("A fetch is running. Stop it in the progress window of your bank, then close the "
               "application."));

        event->ignore();
        return;
    }

    QMainWindow::closeEvent(event);
}
