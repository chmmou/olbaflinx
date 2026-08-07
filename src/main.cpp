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

#include "ui/App.h"

#include "core/ApplicationInfo.h"
#include "core/Banking/Account/Account.h"
#include "core/Logger/Logger.h"
#include "core/Storage/Storage.h"
#include "ui/Assistant/SetupAssistant.h"
#include "ui/Logging.h"

#include <QtCore/QLocale>
#include <QtCore/QSet>
#include <QtCore/QTranslator>
#include <QtWidgets/QApplication>

#include <memory>

using namespace olbaflinx::core;
using namespace olbaflinx::core::banking;
using namespace olbaflinx::core::banking::account;
using namespace olbaflinx::core::logger;
using namespace olbaflinx::core::storage;

using namespace olbaflinx::ui;

namespace {

/**
 * Puts the result of the wizard into the store. Three groups come out of it and
 * each is treated differently:
 *
 * - offered and chosen goes in as kept,
 * - offered and turned down goes in as dropped, without losing its transactions,
 * - never offered is not touched at all. The wizard cannot offer the accounts of
 *   an institution it failed to reach, and that must not take a user's accounts
 *   out of sight.
 *
 * A cancelled wizard hands over two empty lists and nothing is written.
 *
 * The writing itself happens in a thread of its own. This function returns while
 * it runs, and the outcome arrives through the signals of the storage.
 */
void storeTheResultOfTheWizard(App &app, Storage &storage, const assistant::SetupAssistant &wizard)
{
    const auto offered = wizard.offeredAccounts();
    if (offered.isEmpty()) {
        return;
    }

    if (!storage.isValid()) {
        // The store is opened from the overview on the first page of the window,
        // which the user may not have got to yet. Nothing can be written until
        // then.
        qCWarning(lcUi) << "the wizard chose accounts while no storage was open, nothing stored";
        return;
    }

    auto chosenIds = QSet<quint32>();
    const auto chosen = wizard.selectedAccounts();
    for (const auto &item : chosen) {
        if (const auto account = std::dynamic_pointer_cast<Account>(item)) {
            chosenIds.insert(account->uniqueId());
        }
    }

    auto accounts = BankingItems();
    for (const auto &item : offered) {
        const auto account = std::dynamic_pointer_cast<Account>(item);
        if (account == nullptr) {
            continue;
        }

        account->setActive(chosenIds.contains(account->uniqueId()));
        accounts << item;
    }

    if (accounts.isEmpty()) {
        return;
    }

    const int total = static_cast<int>(accounts.size());

    // Single shot, because this run is the only one this connection is for. The
    // storage outlives the window and would otherwise report every later run
    // into a message about the wizard.
    //
    // The count carries the whole outcome: a run that ends early leaves fewer
    // accounts than it was given. The technical cause is already in the log, put
    // there by the storage, and none of it belongs on the screen (FR-036a).
    QObject::connect(
        &storage,
        &Storage::itemsStored,
        &app,
        [&app, total](int stored) {
            if (stored == total) {
                app.showMessage(
                    QCoreApplication::translate("main", "%n account(s) set up.", nullptr, stored));
                return;
            }

            app.showMessage(QCoreApplication::translate(
                                "main",
                                "The setup was not completed. %n of %1 accounts stored.",
                                nullptr,
                                stored)
                                .arg(total));
        },
        Qt::SingleShotConnection);

    storage.storeItems(accounts);
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication::setApplicationName(QStringLiteral("OlbaFlinx"));
    QApplication::setApplicationVersion(QStringLiteral(OLBAFLINX_VERSION));
    QApplication::setOrganizationName(QStringLiteral("de.chm-projects.olbaflinx"));
    QApplication::setOrganizationDomain(QStringLiteral("https://olbaflinx.chm-projects.de"));

    QApplication a(argc, argv);

    QObject::connect(&a, &QApplication::lastWindowClosed, &a, &QApplication::quit);

    // The catalogues are embedded under the resource prefix set by
    // qt_add_translations. A missing catalogue leaves the source strings in
    // place, which is why the return value only gates the installation.
    QTranslator translator;
    if (translator.load(QLocale(),
                        QStringLiteral("OlbaFlinxApp"),
                        QStringLiteral("_"),
                        QStringLiteral(":/i18n"))) {
        QApplication::installTranslator(&translator);
    }

    const ApplicationInfo applicationInfo{QApplication::organizationName(),
                                          QApplication::applicationName(),
                                          QApplication::applicationVersion()};

    // Logger and Storage live on the stack of main. Their lifetime encloses the
    // one of every window, which leaves exactly one owner.
    Logger logger;
    Storage storage(applicationInfo);

    App app(&logger, &storage);
    app.initialize();
    app.show();

    assistant::SetupAssistant setup(applicationInfo, &app);
    setup.exec();

    // The wizard used to end here and its result was dropped. No account had
    // ever reached the store because of it.
    storeTheResultOfTheWizard(app, storage, setup);

    const int result = QApplication::exec();

    storage.close();

    return result;
}
