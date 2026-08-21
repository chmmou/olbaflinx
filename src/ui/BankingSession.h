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
#include "core/Error.h"

#include <QtCore/QObject>

namespace olbaflinx::core::banking {
class Banking;
}

namespace olbaflinx::ui {

class BankingGui;

/**
 * The one banking instance of the window, and the interface it extends.
 *
 * Every kind of fetch the window offers runs over this object. Two banking
 * instances must not extend the same interface, and an instance belongs to one
 * thread at a time, so a second one beside this is not a way around the turn
 * taking: only one fetch is out at a time, whichever kind it is.
 *
 * The object holds no fetch of its own. It hands out the instance and the
 * interface, keeps the cached credential, and answers whether the way is free.
 *
 * Ownership: the creator owns the instance. The banking instance and the
 * interface belong here and are taken down in the order the banking layer asks
 * for.
 *
 * Concurrency: a session runs in a thread of its own, inside the banking layer.
 * Everything this class does happens in the thread it was built in.
 */
class BankingSession : public QObject
{
    Q_OBJECT

public:
    /**
     * The registration key of the application info must not be empty;
     * initialize refuses an empty one.
     */
    explicit BankingSession(core::ApplicationInfo applicationInfo, QObject *parent = nullptr);
    ~BankingSession() override;

    /**
     * Brings the banking instance and its interface up.
     *
     * Called by whoever is about to fetch, so that a window which never fetches
     * never reaches the banking layer. Calling it twice does nothing and is no
     * failure.
     */
    core::Error initialize();

    /**
     * The banking instance, or nothing before initialize has gone through.
     *
     * It stays with this object. Whoever fetches connects to it and sends its
     * orders over it.
     */
    [[nodiscard]] core::banking::Banking *banking() const;

    /**
     * The interface of the banking layer, or nothing before initialize has gone
     * through. It stays with this object.
     */
    [[nodiscard]] BankingGui *gui() const;

    /**
     * Takes the way for one fetch, and answers whether it was free.
     *
     * A refusal means a fetch is out, of whichever kind, or its result is still
     * being written. Nothing is reported on a refusal: the running fetch is not
     * over, and saying so would end it for whoever is waiting.
     */
    [[nodiscard]] bool beginFetch();

    /** Gives the way back. Does nothing where no fetch was out. */
    void endFetch();

    /**
     * Whether a fetch is out or its result is still being written.
     *
     * The span reaches past the session on purpose: a fetch whose records are
     * still on their way into the storage would find the storage busy, and the
     * minutes on the line would be spent for nothing.
     */
    [[nodiscard]] bool isFetchRunning() const;

    /**
     * Whether the span the cached PIN outlives a fetch by is running.
     *
     * It is stopped while a fetch runs and started again at its end, so that a
     * second fetch within the span asks for nothing.
     */
    [[nodiscard]] bool isPasswordCacheExpiring() const;

    /**
     * Empties the cached PIN at once, rather than at the end of the span.
     *
     * For the moment a storage is closed. The interface belongs to the window
     * and outlives the storage, so a PIN entered for one would otherwise still
     * be cached while the next one is open.
     *
     * Does nothing before the first fetch, when there is no interface yet.
     */
    void clearPasswordCache();

private:
    class Private;
    std::unique_ptr<Private> d_ptr;
};

} // namespace olbaflinx::ui
