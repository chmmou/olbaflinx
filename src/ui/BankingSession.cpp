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

#include "ui/BankingSession.h"

#include "core/Banking/Banking.h"
#include "ui/BankingGui.h"

#include <QtCore/QString>

#include <memory>
#include <utility>

using namespace olbaflinx::core;
using namespace olbaflinx::core::banking;
using namespace olbaflinx::ui;

class BankingSession::Private
{
public:
    explicit Private(ApplicationInfo info)
        : applicationInfo(std::move(info))
    {}

    ~Private()
    {
        // The order matters and is the reason the backend carries no Qt parent.
        // Banking still reaches into the interface while it shuts down, in
        // AB_Gui_Unextend, and the interface would already be gone by then.
        banking.reset();
        gui.reset();
    }

    ApplicationInfo applicationInfo;

    // The interface is declared before the instance that uses it, so that the
    // instance is destroyed first even where the destructor above is not the
    // one that runs.
    std::unique_ptr<BankingGui> gui;
    std::unique_ptr<Banking> banking;

    bool fetchRunning = false;
};

BankingSession::BankingSession(ApplicationInfo applicationInfo, QObject *parent)
    : QObject(parent)
    , d_ptr(std::make_unique<Private>(std::move(applicationInfo)))
{}

BankingSession::~BankingSession() = default;

Error BankingSession::initialize()
{
    if (d_ptr->banking) {
        return {};
    }

    // Checked at runtime and not left to a compiler: the field belongs to an
    // aggregate, and one that names only the fields before it leaves this one
    // empty without a word. The application would then reach a bank without
    // identifying itself.
    if (d_ptr->applicationInfo.registrationKey.isEmpty()) {
        return Error(ErrorCode::InvalidInput,
                     QStringLiteral("A fetch needs the registration key of the application"));
    }

    // The interface belongs here and stays here. The wizard holds one of its
    // own: two banking instances that extend the same interface abort the
    // process on the second extension.
    auto gui = std::make_unique<BankingGui>();
    auto banking = std::make_unique<Banking>(d_ptr->applicationInfo);

    if (const auto error = banking->initialize(d_ptr->applicationInfo.name,
                                               d_ptr->applicationInfo.version,
                                               d_ptr->applicationInfo.registrationKey,
                                               gui->getCInterface());
        error.isError()) {
        return error;
    }

    d_ptr->gui = std::move(gui);
    d_ptr->banking = std::move(banking);

    return {};
}

Banking *BankingSession::banking() const
{
    return d_ptr->banking.get();
}

BankingGui *BankingSession::gui() const
{
    return d_ptr->gui.get();
}

bool BankingSession::beginFetch()
{
    if (d_ptr->fetchRunning) {
        return false;
    }

    d_ptr->fetchRunning = true;

    return true;
}

void BankingSession::endFetch()
{
    d_ptr->fetchRunning = false;
}

bool BankingSession::isFetchRunning() const
{
    return d_ptr->fetchRunning;
}

bool BankingSession::isPasswordCacheExpiring() const
{
    return d_ptr->gui && d_ptr->gui->isPasswordCacheExpiring();
}

void BankingSession::clearPasswordCache()
{
    if (d_ptr->gui) {
        d_ptr->gui->clearPasswordCache();
    }
}
