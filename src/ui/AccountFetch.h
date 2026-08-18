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

#include "core/ApplicationInfo.h"
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

/**
 * @brief One fetch of one account, from the session to the stored rows.
 *
 * It holds the banking instance of the window and a user interface of its own.
 * Two banking instances must not extend the same interface, and the wizard holds
 * one already, so this is a second one beside it rather than a shared one.
 *
 * The window is left with the presentation: this class says what happened, it
 * shows nothing and refreshes no view.
 *
 * Ownership: the creator owns the instance. The storage is observed only and has
 * to outlive it. The banking instance and the interface belong here and are
 * taken down in the order the banking layer asks for.
 *
 * Concurrency: the session runs in a thread of its own, inside the banking
 * layer. Everything this class does happens in the thread it was built in.
 */
class AccountFetch : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief How a fetch ended.
     *
     * Seven ways out and each says something else to the user. Three of them
     * are no failure. Skipped: the account has no online access and was passed
     * over before anything was sent. NothingOffered: it has online access and
     * the bank holds no order for it at all, neither the bookings nor the
     * balance. BalanceOnly: the bank holds no order for the bookings, so the
     * fetch brought the balance alone.
     *
     * Skipped and NothingOffered are apart, because one outcome for both would
     * tell the user of either that the account has no online access.
     *
     * StoreFailed is a failure, and the only one where the session was fine and
     * what it brought is lost.
     */
    enum class Outcome {
        Received,
        BalanceOnly,
        Skipped,
        NothingOffered,
        Aborted,
        Failed,
        StoreFailed,
    };
    Q_ENUM(Outcome)

    /**
     * @brief What a fetch over several accounts amounted to.
     *
     * The three counts add up to the accounts that were handed in. An account
     * the bank holds no order of any kind for counts as fetched: it has online
     * access, so it is not skipped, and nothing about it failed.
     */
    struct Summary
    {
        /** How the run as a whole ended. */
        Outcome outcome = Outcome::Received;

        /** Accounts whose orders went out and came back without a refusal. */
        int fetched = 0;

        /** Accounts without online access, passed over before anything was sent. */
        int skipped = 0;

        /**
         * Accounts whose orders the bank refused.
         *
         * Nought after an abort: what did not come through was cut off and not
         * refused, and the two read differently to whoever is told.
         */
        int failed = 0;

        /** New bookings over all accounts. One already stored adds none. */
        int storedCount = 0;

        /** Whether an abort was answered by keeping what had already arrived. */
        bool keptAfterAbort = false;

        /** What to tell the user, already worded. Empty where the outcome says it. */
        QString reason;
    };

    /**
     * @param applicationInfo What the application signs on to a bank with. Its
     *  registration key must not be empty; initialize refuses an empty one.
     * @param storage Externally owned storage, has to outlive this object.
     * @param parent Optional owner.
     */
    explicit AccountFetch(core::ApplicationInfo applicationInfo,
                          core::storage::Storage *storage,
                          QObject *parent = nullptr);
    ~AccountFetch() override;

    /**
     * @brief Brings the banking instance and its interface up.
     *
     * Called by start when it has not run yet, so that a window which never
     * fetches never reaches the banking layer. Calling it twice does nothing and
     * is no failure.
     *
     * @return A default constructed Error on success, otherwise the reason. The
     *  caller has to check it, the return type is [[nodiscard]].
     */
    core::Error initialize();

    /**
     * @brief Fetches the transactions and the balance of one account.
     *
     * Reads the starting point out of the storage, sends the orders, and stores
     * what comes back. started is emitted before the session goes out, ended once
     * the whole run is over, whichever way it ended.
     *
     * A call while a fetch runs is ignored: the entries that lead here are
     * switched off for as long, and a second one would declare the running one
     * over.
     *
     * @param account The account to fetch. It stays with its caller.
     */
    void start(const std::shared_ptr<core::banking::account::Account> &account);

    /**
     * @brief Fetches the transactions and the balances of every given account.
     *
     * The orders of all accounts travel in one session, and only after it has
     * ended is anything written: the storage is reached account by account, the
     * bookings of one and its balance before the next one begins. That order is
     * what lets an abort be answered at all - at the moment the question is put,
     * no row of this run stands in the file, so discarding is a matter of not
     * writing rather than of deleting.
     *
     * started is emitted before the session goes out, allEnded once the whole
     * run is over. ended is not emitted for a run of this kind.
     *
     * A call while a fetch of either kind runs is ignored, and so is an empty
     * list.
     *
     * @param accounts The accounts to fetch. They stay with their caller.
     */
    void startAll(const QList<std::shared_ptr<core::banking::account::Account>> &accounts);

    /**
     * @brief The answer to abortNeedsAnswer.
     *
     * @param keep Whether what the run had already brought in is written. False
     *  ends the run without a single row being written; the holding is then the
     *  one from before it started.
     *
     * Does nothing while no question is open, so an answer that arrives twice is
     * no failure.
     */
    void answerAbort(bool keep);

    /**
     * @brief Whether the span the cached PIN outlives a fetch by is running.
     *
     * It is stopped while a fetch runs and started again at its end, so that a
     * second fetch within the span asks for nothing.
     */
    [[nodiscard]] bool isPasswordCacheExpiring() const;

Q_SIGNALS:
    /**
     * @brief A fetch has begun.
     *
     * The bank has not been reached yet at this point: the starting point is
     * read first. It is the moment the window switches its entries off.
     */
    void started();

    /**
     * @brief A fetch has ended, on every path.
     *
     * @param outcome How it ended.
     * @param storedCount How many bookings were added. A booking that is already
     *  stored adds none, so nought is an answer and not a failure.
     * @param reason What to tell the user, already worded for him. Empty where
     *  the outcome says everything.
     */
    void ended(olbaflinx::ui::AccountFetch::Outcome outcome, int storedCount, const QString &reason);

    /**
     * @brief A fetch over several accounts has ended, on every path.
     *
     * @param summary What the run amounted to. It names no account and carries
     *  no amount.
     */
    void allEnded(const olbaflinx::ui::AccountFetch::Summary &summary);

    /**
     * @brief The user stopped a fetch over several accounts, and the run is
     *  waiting for the answer to answerAbort.
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

Q_DECLARE_METATYPE(olbaflinx::ui::AccountFetch::Summary)
