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

#include "core/Banking/Account/Account.h"
#include "core/Error.h"

#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QString>

#include <memory>

namespace olbaflinx::core::storage {
class Storage;
}

namespace olbaflinx::ui {

class BankingSession;

/**
 * One fetch of the standing orders of one account, from the session to the
 * stored rows.
 *
 * It runs over the banking session of the window, the same one the fetch of the
 * bookings runs over, and only one of them is out at a time. It is a run of its
 * own all the same: an order the bank refuses here must not take the bookings of
 * the same account with it.
 *
 * The window is left with the presentation: this class says what happened, it
 * shows nothing and refreshes no view.
 *
 * Ownership: the creator owns the instance. The session and the storage are
 * observed only and have to outlive it.
 *
 * Concurrency: the session runs in a thread of its own, inside the banking
 * layer. Everything this class does happens in the thread it was built in.
 */
class StandingOrderFetch : public QObject
{
    Q_OBJECT

public:
    /**
     * How a fetch ended.
     *
     * Four of the six are no failure and each says something else to the user.
     * Skipped: the account has no online access and was passed over before
     * anything was sent. NothingOffered: the bank holds no order of any kind for
     * it. NotOffered: it holds orders for the account, but not the one that
     * brings standing orders, so nothing was sent and the account is not one
     * without standing orders.
     *
     * StoreFailed is a failure, and the only one where the session was fine and
     * what it brought is lost. Nothing of the failed run stays behind, the mark
     * on the orders it did not carry included.
     */
    enum class Outcome {
        Received,
        Skipped,
        NothingOffered,
        NotOffered,
        Aborted,
        Failed,
        StoreFailed,
    };
    Q_ENUM(Outcome)

    /**
     * What a fetch over several accounts amounted to.
     *
     * The four counts add up to the accounts that were handed in.
     */
    struct Summary
    {
        /** How the run as a whole ended. */
        Outcome outcome = Outcome::Received;

        /** Accounts whose order went out and came back without a refusal. */
        int fetched = 0;

        /** Accounts without online access, passed over before anything was sent. */
        int skipped = 0;

        /**
         * Accounts whose order the bank refused.
         *
         * Nought after an abort: what did not come through was cut off and not
         * refused, and the two read differently to whoever is told.
         */
        int failed = 0;

        /** Accounts whose bank does not carry the request for standing orders. */
        int notOffered = 0;

        /** Rows that were added over all accounts. One already stored adds none. */
        int storedCount = 0;

        /** Whether an abort was answered by keeping what had already arrived. */
        bool keptAfterAbort = false;

        /** What to tell the user, already worded. Empty where the outcome says it. */
        QString reason;
    };

    /**
     * The session and the storage are owned elsewhere and have to outlive this
     * object.
     */
    explicit StandingOrderFetch(BankingSession *session,
                                core::storage::Storage *storage,
                                QObject *parent = nullptr);
    ~StandingOrderFetch() override;

    /**
     * Brings the banking session up and listens to it.
     *
     * Called by start when it has not run yet, so that a window which never
     * fetches never reaches the banking layer. Calling it twice does nothing and
     * is no failure.
     */
    core::Error initialize();

    /**
     * Fetches the standing orders of one account, which stays with its caller.
     *
     * started is emitted before the session goes out, ended once the whole run
     * is over, whichever way it ended. What comes back is written once the
     * session has ended, and every order of the account the run did not carry is
     * marked as ended in the same bracket.
     *
     * A call while a fetch of any kind runs is ignored, and so is one while the
     * result of the last is still being written.
     */
    void start(const std::shared_ptr<core::banking::account::Account> &account);

    /**
     * Fetches the standing orders of every given account, which stay with their
     * caller.
     *
     * The orders of all accounts travel in one session, and only after it has
     * ended is anything written: the storage is reached account by account. No
     * row of the run therefore stands in the file while the question of an abort
     * is open, and discarding is a matter of not writing rather than of deleting.
     *
     * started is emitted before the session goes out, allEnded once the whole run
     * is over. ended is not emitted for a run of this kind.
     *
     * A call while a fetch of any kind runs is ignored, and so is an empty list.
     */
    void startAll(const QList<std::shared_ptr<core::banking::account::Account>> &accounts);

    /**
     * The answer to abortNeedsAnswer. Keeping writes what the run had already
     * brought in; not keeping ends the run without a single row being written,
     * and the holding is then the one from before it started.
     *
     * No order is marked as ended either way. A run that was cut off says
     * nothing about what the institution still holds.
     *
     * Does nothing while no question is open, so an answer that arrives twice is
     * no failure.
     */
    void answerAbort(bool keep);

Q_SIGNALS:
    /**
     * A fetch has begun.
     *
     * The bank has not been reached yet at this point. It is the moment the
     * window switches its entries off.
     */
    void started();

    /**
     * A fetch has ended, on every path.
     *
     * The count is the rows that were added; an order that is already stored
     * adds none, so nought is an answer and not a failure. The reason is already
     * worded for the user and empty where the outcome says everything.
     */
    void ended(olbaflinx::ui::StandingOrderFetch::Outcome outcome,
               int storedCount,
               const QString &reason);

    /**
     * A fetch over several accounts has ended, on every path. The summary names
     * no account and carries no amount.
     */
    void allEnded(const olbaflinx::ui::StandingOrderFetch::Summary &summary);

    /**
     * The user stopped a fetch over several accounts, and the run is waiting for
     * the answer to answerAbort.
     *
     * The question belongs to the window: this class shows nothing. Nothing is
     * written until the answer arrives, and the answer decides whether anything
     * is written at all.
     */
    void abortNeedsAnswer();

private:
    class Private;
    std::unique_ptr<Private> d_ptr;
};

} // namespace olbaflinx::ui

Q_DECLARE_METATYPE(olbaflinx::ui::StandingOrderFetch::Summary)
