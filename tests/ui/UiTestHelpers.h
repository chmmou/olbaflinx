/**
 * Copyright (C) 2021-2026, Alexander Saal <developer@olbaflinx.chm-projects.de>
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

#include "core/Storage/Storage.h"
#include "ui/App.h"
#include "ui/Models/AccountTreeModel.h"
#include "ui/Storage/StorageDialog.h"

#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include <chrono>

using namespace olbaflinx::core::storage;
using namespace olbaflinx::ui;
using namespace olbaflinx::ui::models;
using namespace olbaflinx::ui::storage;

namespace olbaflinx::ui::tests {

/**
 * What a test of the window needs and a test of the core cannot have.
 *
 * This header stands beside the three under ../core/ rather than in them for
 * one reason: it names App and the models, which pull in Widgets. The core test
 * targets link none, and TestHelpers.h reaches every target through the include
 * path.
 */
class UiTestHelpers
{
public:
    /**
     * How long a spy waits for a signal a worker thread has to produce first.
     * The call returns the moment the signal arrives, so no test sleeps for it.
     */
    static constexpr auto workerTimeout = std::chrono::seconds{30};

    /**
     * The same bound where a macro needs it in milliseconds.
     */
    static constexpr int workerTimeoutMs = 30000;

    /**
     * Brings the accounts of the storage onto the screen the way the application
     * does it, over the core and not by handing the model a list.
     *
     * The connection holds for this one read. A standing one would hand the
     * transactions of an account to the account tree as well and leave it empty.
     */
    static bool readAccountsInto(App &app, Storage &storage)
    {
        auto *const overview = app.findChild<StorageDialog *>();
        if (overview == nullptr) {
            return false;
        }

        QObject::connect(&storage,
                         &Storage::itemsReceived,
                         &app,
                         &App::setAccounts,
                         Qt::SingleShotConnection);

        Q_EMIT overview->storageOpened();

        QSignalSpy finishedSpy(&storage, &Storage::readFinished);

        if (storage.receiveItems({.type = Storage::StorageAccount}).isError()) {
            return false;
        }

        return finishedSpy.wait(workerTimeout);
    }

    /**
     * The index of the first account under the first bank, the one a click in
     * the tree would land on.
     */
    static QModelIndex firstAccountOf(const AccountTreeModel &model)
    {
        return model.index(0, 0, model.index(0, 0));
    }

    /**
     * Puts the window up, still on the overview.
     */
    static bool showTheWindow(App &app)
    {
        app.initialize();
        app.show();

        return QTest::qWaitForWindowExposed(&app);
    }
};

} // namespace olbaflinx::ui::tests
