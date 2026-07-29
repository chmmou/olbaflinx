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

#include <cstdlib>

#include "core/Banking/Account/Account.h"

#include <QtCore/QMap>
#include <QtCore/QRandomGenerator>

using namespace olbaflinx::core::banking::account;

namespace olbaflinx::core::tests {

class BaseTest
{
public:
    static std::shared_ptr<Account> createFakeAccount(const int accountType = 1)
    {
        return Account::fromMap(createFakeAccountMap(accountType));
    }

    static QMap<QString, QVariant> createFakeAccountMap(const int accountType = 1)
    {
        QMap<QString, QVariant> map = {};

        // Test fake data: https://ibanvalidieren.de/beispiele.html
        map[":type"] = accountType;
        map[":uniqueId"] = QRandomGenerator::system()->generate();
        map[":backend_name"] = "aqhbci";
        map[":owner_name"] = randomString();
        map[":account_name"] = randomString();
        map[":currency"] = "EURO";
        map[":memo"] = "";
        map[":iban"] = "DE02500105170137075030";
        map[":bic"] = "INGDDEFF";
        map[":country"] = "";
        map[":bank_code"] = "50010517";
        map[":bank_name"] = "ING-DIBA";
        map[":branch_id"] = "";
        map[":account_number"] = "0137075030";
        map[":sub_account_number"] = "";
        map[":balance"] = (rand() * 1.01);

        return map;
    }

    static QString randomString()
    {
        QString randomString;
        for (int i = 0; i < 12; ++i) {
            const auto letter = 'A' + (rand() % (2 * 26));
            randomString.append(QChar(letter));
        }

        return randomString;
    }
};

} // namespace olbaflinx::core::tests
