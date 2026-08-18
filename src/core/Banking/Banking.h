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

#include "core/OlbaFlinxCore.h"

#include "core/ApplicationInfo.h"
#include "core/Banking/Account/Account.h"
#include "core/Error.h"

// The C interface of the gwenhywfar user interface. It pulls no Qt module; the
// Qt implementation of it lives in the ui layer, which is where the widgets
// belong.
#include <gwenhywfar/gui.h>

#include <aqbanking/types/imexporter_context.h>
#include <aqbanking/types/transaction.h>

#include <QtCore/QDate>
#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QObject>

#include <memory>

namespace olbaflinx::core::banking {

using namespace ::account;

/**
 * How a session ended.
 *
 * The user interface needs the abort told apart from the failure: it says
 * something else, and it keeps what an earlier account of the same run had
 * already brought.
 */
enum class FetchOutcome {
    /** The session ran through and the container may be read. */
    Received,
    /** The user stopped the session. Nothing of this account is to be kept. */
    Aborted,
    /** The session failed, or the bank refused an order of this account. */
    Failed,
};

/**
 * The banking backend of the application, wrapping aqbanking.
 *
 * What it can do is fetch accounts and their transfers, direct debits and
 * standing orders. Sending any of them is not part of the interface.
 *
 * Ownership: the creator owns the instance. The accounts reported through
 * itemsReceived pass into the ownership of the receiver. The user interface
 * handed to initialize stays with its creator; see there.
 */
class OLBAFLINX_CORE_EXPORT Banking : public QObject
{
    Q_OBJECT

public:
    /**
     * The application info carries what the chip card service is signed on with
     * and the title of the setup dialog.
     */
    explicit Banking(ApplicationInfo applicationInfo, QObject *parent = nullptr);
    ~Banking() override;

    /**
     * Name, version and key are the registration German HBCI ZKA issues.
     *
     * The user interface is the one the backend asks for a PIN, a TAN and a
     * dialog. It stays the property of the caller: this class neither frees it
     * in finalize nor in its destructor, and it has to outlive this instance.
     * It must not be null. Without one the backend aborts the process instead
     * of reporting a failure, because AB_Gui_Extend asserts on it while
     * AB_Banking_Init before it runs through and answers success. A caller with
     * no display passes GWEN_Gui_new(), the non-interactive interface of
     * gwenhywfar, and frees it with GWEN_Gui_free() afterwards.
     */
    Error initialize(const QString &name, const QString &version, const QString &key, GWEN_GUI *gui);

    /**
     * Frees everything the backend holds. The user interface handed to
     * initialize is not among it.
     */
    void finalize();

    /**
     * Opens the aqbanking setup dialog and answers what that dialog returned,
     * or -1 where the backend is not initialized or the call failed.
     */
    int setupAccounts();

    /**
     * Asks for the accounts that were set up through setupAccounts. They arrive
     * through itemsReceived, and finished ends the call on every path.
     *
     * Refused while a fetch runs: aqbanking takes no lock, and the session
     * walks the same handle from a thread of its own.
     */
    void accounts();

    /**
     * Fetches the transactions and the balance of one account. Both requests
     * travel in one session. The result arrives through itemsReceived,
     * transactions and balance in one list, each record carrying the id of the
     * account it belongs to and told apart by its item type.
     *
     * The account stays with its caller. The stored date is the day its holding
     * ends on, as Storage::latestTransactionDate reports it; the fetch starts a
     * fixed lead time before it, and an invalid date fetches everything the
     * bank offers.
     *
     * Preconditions: the backend is initialized. The user interface handed to
     * initialize is set for the thread of the session by this call itself; the
     * caller has nothing to do for it.
     *
     * Errors: a failed session reports errorOccurred. An abort by the user
     * reports aborted instead. An account without online access reports
     * accountSkipped and is not an error, and neither is an empty result. Every
     * path of an accepted fetch ends in finished.
     *
     * Concurrency: the call returns at once and runs the session in a thread of
     * its own. A second fetch while one runs is refused with errorOccurred and
     * no finished, which would otherwise declare the running one over. The
     * instance belongs to one thread at a time: while a session runs, no caller
     * may reach into this object, and finalize waits for the session rather
     * than pulling it away.
     */
    void fetchAccount(const Account &account, const QDate &latestStoredDate = {});

    /**
     * Fetches the transactions and the balances of every given account. The
     * orders travel in one list and one call; the backend sorts them by
     * institution and runs one session per institution, so two accounts of the
     * same bank share a session and share whatever brings it down.
     *
     * The accounts stay with their caller; the session works on a copy made
     * here. An empty list is no failure and ends in finished without anything
     * being sent. The stored dates are read under the identifier of the
     * account; one that is missing fetches everything the bank offers.
     *
     * Preconditions: as fetchAccount.
     *
     * Errors: an account without online access reports accountSkipped and is
     * kept out of the list, because the backend refuses the whole run over one
     * such account. An account whose orders the bank refused reports
     * accountFailed and its records are dropped, while the accounts of other
     * institutions run on. Only a failure of the call itself reports
     * errorOccurred. Every path ends in finished.
     *
     * Concurrency: as fetchAccount, and refused while a fetch of either kind
     * runs.
     */
    void fetchAccounts(const QList<std::shared_ptr<Account>> &accounts,
                       const QHash<quint32, QDate> &latestStoredDates = {});

    /**
     * Builds the two orders of a fetch without sending them. The lead time is
     * subtracted here, so the orders start before the day the stored holding
     * ends on and carry no starting point at all when that date is invalid.
     *
     * What the backend offers decides which orders are built. Where the
     * description names the orders it holds, only those are built; where it is
     * missing or names none at all, both are built, because the description
     * that comes with a stored account carries no such names and reading its
     * silence as a refusal would leave every fetch empty.
     *
     * The caller owns the returned list and releases it, orders included, with
     * AB_Transaction_List2_freeAll.
     */
    [[nodiscard]] static AB_TRANSACTION_LIST2 *buildFetchCommands(
        const Account &account,
        const QDate &latestStoredDate,
        const AB_ACCOUNT_SPEC *offered = nullptr);

    /**
     * Whether the backend holds an order of this kind for the account. It
     * writes the limits of an order into the description exactly where it can
     * build that order, and leaves them out where it cannot. Asking beforehand
     * keeps an order out of a session that would come back as a failure of the
     * whole account.
     *
     * Nothing, or a description that names no order at all, means nothing is
     * known: the field may be left empty by a backend, so everything is taken
     * as offered. Only a description that names other orders and not this one
     * is a refusal of it.
     */
    [[nodiscard]] static bool accountOffers(const AB_ACCOUNT_SPEC *offered,
                                            AB_TRANSACTION_COMMAND command);

    /**
     * Reads the transactions and the balance out of the container a session
     * filled, in one list, and hands both to their new owner.
     *
     * The orders that were sent carry the outcome per order. An account whose
     * order failed is not in the list, neither with its transactions nor with
     * its balance.
     */
    [[nodiscard]] static BankingItems itemsFromContext(const AB_IMEXPORTER_CONTEXT *context,
                                                       AB_TRANSACTION_LIST2 *commands);

    /**
     * Tells apart how a session ended, from what AB_Banking_SendCommands
     * returned and from the orders that were sent. A session can come back
     * successful and still carry an order the bank refused.
     *
     * Aborted where the user stopped the session, Failed where the session
     * failed or an order of the named account was refused, Received otherwise.
     * A session that was cut in the middle, say because the far end went away,
     * is a failure and not an abort.
     */
    [[nodiscard]] static FetchOutcome outcomeOfSession(int sessionResult,
                                                       AB_TRANSACTION_LIST2 *commands,
                                                       quint32 uniqueAccountId);

Q_SIGNALS:
    /**
     * An error occurred on an asynchronous path. The reason is a technical
     * message meant for the log, and it carries the return value of the banking
     * backend where there is one.
     */
    void errorOccurred(olbaflinx::core::ErrorCode errorCode, const QString &reason);

    /**
     * An account the fetch passed over, named by the identifier the banking
     * backend keeps. The reason carries no account data.
     *
     * Not an error of the session: an account without online access is skipped
     * before anything is sent, so that the accounts beside it still run.
     *
     * An account the bank holds no order at all for is a different case and has
     * a signal of its own, noOrderOffered. Sharing one signal would tell the
     * user of both that the account has no online access.
     */
    void accountSkipped(quint32 uniqueAccountId, const QString &reason);

    /**
     * An account whose orders the bank refused, while the fetch went on for the
     * accounts beside it.
     *
     * Only a fetch over several accounts reports this way. A fetch of one
     * account has nothing to go on with, so its refusal is the failure of the
     * whole run and travels through errorOccurred.
     *
     * Nothing of this account is reported: itemsReceived carries neither its
     * bookings nor its balance. Whoever counts the outcome of a run counts this
     * account as failed and no other way.
     */
    void accountFailed(quint32 uniqueAccountId);

    /**
     * An account the bank holds no order of any kind for.
     *
     * The account has online access, so it is not the case accountSkipped
     * names, and the bank offers neither the bookings nor the balance of it.
     * Nothing is sent and the session ends here.
     */
    void noOrderOffered(quint32 uniqueAccountId);

    /**
     * An account the bank holds no order for transactions for.
     *
     * Not an error and not a skipped account: the fetch goes on and brings the
     * balance. What it says is that no booking can arrive for this account, so
     * that an empty result is not read as an account with nothing new.
     *
     * It arrives before the session starts, and finished still ends the fetch.
     */
    void transactionsNotOffered(quint32 uniqueAccountId);

    /**
     * The user stopped a session. An abort is no failure and therefore no
     * errorOccurred; the user interface says something else for each of the
     * two.
     *
     * Nothing of the aborted account is reported. finished follows.
     */
    void aborted();

    void progressValueChanged(qreal progress);
    void itemsReceived(const BankingItems &items);
    void finished();

private:
    class Private;
    Private *d_ptr = nullptr;
};

} // namespace olbaflinx::core::banking
