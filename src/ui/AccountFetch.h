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

#include <QtCore/QObject>

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
     * Five ways out and each says something else to the user. Skipped is no
     * failure: an account without online access is passed over before anything
     * is sent. StoreFailed is one, and the only one where the session was fine
     * and what it brought is lost.
     */
    enum class Outcome {
        Received,
        Skipped,
        Aborted,
        Failed,
        StoreFailed,
    };
    Q_ENUM(Outcome)

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

private:
    class Private;
    std::unique_ptr<Private> d_ptr;
};

} // namespace olbaflinx::ui
