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

#include "ui/AccountFetch.h"

using namespace olbaflinx::core;
using namespace olbaflinx::core::banking::account;
using namespace olbaflinx::core::storage;
using namespace olbaflinx::ui;

class AccountFetch::Private
{
public:
    Private(ApplicationInfo info, Storage *appStorage)
        : applicationInfo(std::move(info))
        , storage(appStorage)
    {}

    ApplicationInfo applicationInfo;
    Storage *storage;
};

AccountFetch::AccountFetch(ApplicationInfo applicationInfo, Storage *storage, QObject *parent)
    : QObject(parent)
    , d_ptr(std::make_unique<Private>(std::move(applicationInfo), storage))
{}

AccountFetch::~AccountFetch() = default;

Error AccountFetch::initialize()
{
    return {};
}

void AccountFetch::start(const std::shared_ptr<Account> &account)
{
    Q_UNUSED(account)
}

bool AccountFetch::isPasswordCacheExpiring() const
{
    return false;
}
