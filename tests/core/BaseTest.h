/**
 * Copyright (C) 2021-2022, Alexander Saal <developer@olbaflinx.chm-projects.de>
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

#include <QtCore/QMap>

#include <aqbanking/account_type.h>

#ifndef OLBAFLINX_BASETEST_H
#define OLBAFLINX_BASETEST_H

namespace olbaflinx::core::tests {

class BaseTest
{
public:
    static Account *createFakeAccount(int accountType = AB_AccountType_Bank)
    {
        const auto map = createFakeAccountMap(accountType);
        return Account::create(map);
    }

    static QMap<QString, QVariant> createFakeAccountMap(int accountType = AB_AccountType_Bank)
    {
        QMap<QString, QVariant> map = {};

        // Test fake data: https://ibanvalidieren.de/beispiele.html
        map["type"] = accountType;
        map["uniqueId"] = QRandomGenerator::system()->generate();
        map["backendName"] = "aqhbci";
        map["ownerName"] = "Test User";
        map["accountName"] = "Kontokorrent";
        map["currency"] = "";
        map["memo"] = "";
        map["iban"] = "DE02500105170137075030";
        map["bic"] = "INGDDEFF";
        map["country"] = "";
        map["bankCode"] = "50010517";
        map["bankName"] = "ING-DIBA";
        map["branchId"] = "";
        map["accountNumber"] = "0137075030";
        map["subAccountNumber"] = "";
        map["balance"] = 0.0;

        return map;
    }

    static AccountBalance *createFakeAccountBalance() { return Q_NULLPTR; }
    static Transaction *createFakeTransaction() { return Q_NULLPTR; }
};

} // namespace olbaflinx::core::storage::tests

#endif //OLBAFLINX_BASETEST_H
