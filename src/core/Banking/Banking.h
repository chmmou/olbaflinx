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
#include <QtCore/QObject>

namespace olbaflinx::core::banking {

using namespace ::account;

/**
 * @brief How a session ended.
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
 * @brief
 *  The Banking Backend Object contains the encapsulated and complete business logic of AQBanking
 *  for Qt 6.
 * @note
 *  Currently only the fetching of accounts and their (SEPA) transfers / direct debits / standing
 *  orders are supported. The sending of transfers / direct debits will be added step by step.
 * @author Alexander Saal
 * @version 1.0
 * @package olbaflinx::core::banking
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
     * @param applicationInfo Details used to sign on to the chip card service
     *  and for the title of the setup dialog.
     * @param parent Optional owner.
     */
    explicit Banking(ApplicationInfo applicationInfo, QObject *parent = nullptr);
    ~Banking() override;

    /**
     * @brief Initialize the banking backend.
     *
     * @param name Application name registered by German HBCI ZKA
     * @param version Application version registered by German HBCI ZKA
     * @param key The FinTS registration key from German ZKA
     * @param gui The user interface the backend asks for a PIN, a TAN and a
     *  dialog. It stays the property of the caller: this class neither frees it
     *  in finalize nor in its destructor, and it has to outlive this instance.
     *  It must not be null. Without one the backend aborts the process instead
     *  of reporting a failure: AB_Gui_Extend asserts on it, and AB_Banking_Init
     *  before it runs through and answers success. A caller with no display passes
     *  GWEN_Gui_new(), the non-interactive interface of gwenhywfar, and frees it
     *  with GWEN_Gui_free() afterwards.
     *
     * @return A default constructed Error on success, otherwise the reason. The
     *  caller has to check it, the return type is [[nodiscard]].
     */
    Error initialize(const QString &name, const QString &version, const QString &key, GWEN_GUI *gui);

    /**
     * @brief Finalize the banking backend and free all resources.
     */
    void finalize();

    /**
     * @brief Open the aqbaking setup dialog.
     *
     * @return
     *  If banking backend not initialized or other error occurred -1 is returned; otherwise
     *  the return value from setup dialog.
     */
    int setupAccounts();

    /**
     * @brief Get all accounts previously set up with Banking::setupAccounts
     */
    void accounts();

    /**
     * @brief Fetch the transactions and the balance of one account.
     *
     * Both requests travel in one session. The result arrives through
     * itemsReceived, transactions and balance in one list, each record carrying
     * the id of the account it belongs to and told apart by its item type.
     *
     * @param account The account to fetch. It stays with its caller.
     * @param latestStoredDate The day the stored holding of the account ends
     *  on, as Storage::latestTransactionDate reports it. The fetch starts a
     *  fixed lead time before it. An invalid date fetches everything the bank
     *  offers, which is what the first fetch of an account does.
     *
     * Preconditions: the backend is initialized. The user interface handed to
     *  initialize is set for the thread of the session by this call itself; the
     *  caller has nothing to do for it.
     *
     * Errors: a failed session reports errorOccurred. An abort by the user
     *  reports aborted instead, which is the difference the user interface needs.
     *  An account without online access reports accountSkipped and is not an
     *  error. An empty result is none either, it reports an empty list. Every
     *  path of an accepted fetch ends in finished.
     *
     * Concurrency: this call returns at once and runs the session in a thread of
     *  its own. A second fetch while one runs is refused: it reports
     *  errorOccurred and no finished, because a finished of its own would declare
     *  the running one over. The instance belongs to one thread at a time, not to
     *  one thread for good: while a session runs, no caller may reach into this
     *  object, and finalize waits for the session rather than pulling it away.
     */
    void fetchAccount(const Account &account, const QDate &latestStoredDate = {});

    /**
     * @brief Build the two orders of a fetch, without sending them.
     *
     * Separate from the session so that the orders can be read before they go
     * out, which is what makes them measurable without a bank. The lead time is
     * subtracted here for that reason: a session cannot be run without one.
     *
     * @param account The account the orders are built for.
     * @param latestStoredDate The day the stored holding ends on. The orders
     *  start a fixed lead time before it, and carry no starting point at all
     *  when it is invalid.
     *
     * @return A list of two orders, one for the transactions and one for the
     *  balance. The caller owns it and releases it, orders included, with
     *  AB_Transaction_List2_freeAll.
     */
    [[nodiscard]] static AB_TRANSACTION_LIST2 *buildFetchCommands(const Account &account,
                                                                  const QDate &latestStoredDate);

    /**
     * @brief Read the answer of a session out of its container.
     *
     * @param context The container the session filled.
     * @param commands The orders that were sent. They carry the outcome of the
     *  session per order, which decides whether an account is reported at all.
     *
     * @return The transactions and the balance, in one list. Both stay with
     *  their new owner. An account whose order failed is not in it, neither
     *  with its transactions nor with its balance.
     */
    [[nodiscard]] static BankingItems itemsFromContext(const AB_IMEXPORTER_CONTEXT *context,
                                                       AB_TRANSACTION_LIST2 *commands);

    /**
     * @brief Tell apart how a session ended, without running one.
     *
     * Separate from the session for the same reason buildFetchCommands is: the
     * three ways out cannot be brought about without a bank, and a distinction
     * that cannot be measured is a promise without a proof.
     *
     * @param sessionResult What AB_Banking_SendCommands returned.
     * @param commands The orders that were sent. A session can come back
     *  successful and still carry an order the bank refused.
     * @param uniqueAccountId The account the orders belong to.
     *
     * @return Aborted where the user stopped the session, Failed where the
     *  session failed or an order of this account was refused, Received
     *  otherwise. A session that was cut in the middle, say because the far end
     *  went away, is a failure and not an abort.
     */
    [[nodiscard]] static FetchOutcome outcomeOfSession(int sessionResult,
                                                       AB_TRANSACTION_LIST2 *commands,
                                                       quint32 uniqueAccountId);

Q_SIGNALS:
    /**
     * @brief This signal is emitted if an error occurred on an asynchronous path.
     *
     * @param errorCode @ref olbaflinx::core::ErrorCode
     * @param reason Technical message, meant for the log. It carries the return
     *  value of the banking backend where there is one.
     */
    void errorOccurred(olbaflinx::core::ErrorCode errorCode, const QString &reason);

    /**
     * @brief This signal is emitted for an account a fetch has passed over.
     *
     * Not an error of the session: an account without online access is skipped
     * before anything is sent, so that the accounts beside it still run.
     *
     * @param uniqueAccountId The account, as the banking backend keeps it.
     * @param reason Why it was passed over. It carries no account data.
     */
    void accountSkipped(quint32 uniqueAccountId, const QString &reason);

    /**
     * @brief This signal is emitted when the user stopped a session.
     *
     * An abort is no failure and therefore no errorOccurred. It has a way of its
     * own for the same reason accountSkipped has: without one it would either be
     * invisible or read like a failure, and the user interface says something
     * else for each of the two.
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
