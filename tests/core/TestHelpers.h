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

#include "core/Banking/Account/Account.h"

#include <QtCore/QMap>
#include <QtCore/QRandomGenerator>
#include <QtCore/QString>

using namespace olbaflinx::core::banking::account;

namespace olbaflinx::core::tests {

class TestHelpers
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
        // The keys carry no colon. It belongs to the binding of a query, not to
        // a property map, and the two sides used to disagree about it.
        map[QStringLiteral("type")] = accountType;
        map[QStringLiteral("unique_id")] = generator()->generate();
        map[QStringLiteral("backend_name")] = QStringLiteral("aqhbci");
        map[QStringLiteral("owner_name")] = randomString();
        map[QStringLiteral("account_name")] = randomString();
        map[QStringLiteral("currency")] = QStringLiteral("EURO");
        map[QStringLiteral("memo")] = QString();
        map[QStringLiteral("iban")] = QStringLiteral("DE02500105170137075030");
        map[QStringLiteral("bic")] = QStringLiteral("INGDDEFF");
        map[QStringLiteral("country")] = QString();
        map[QStringLiteral("bank_code")] = QStringLiteral("50010517");
        map[QStringLiteral("bank_name")] = QStringLiteral("ING-DIBA");
        map[QStringLiteral("branch_id")] = QString();
        map[QStringLiteral("account_number")] = QStringLiteral("0137075030");
        map[QStringLiteral("sub_account_number")] = QString();
        map[QStringLiteral("balance")] = generator()->bounded(1000.0);

        return map;
    }

    /**
     * A reference account with every field set. Ownership passes to the caller,
     * which for a property map means to Account::fromMap.
     */
    static ReferenceAccount *createFakeReferenceAccount()
    {
        return ReferenceAccount::create({
            {QStringLiteral("account_type"), 1},
            {QStringLiteral("owner_name"), QStringLiteral("Erika Müller-Groß")},
            {QStringLiteral("owner_name2"), QStringLiteral("Max Mustermann")},
            {QStringLiteral("account_name"), QStringLiteral("Sparkonto")},
            {QStringLiteral("iban"), QStringLiteral("DE02120300000000202051")},
            {QStringLiteral("bic"), QStringLiteral("BYLADEM1001")},
            {QStringLiteral("country"), QStringLiteral("de")},
            {QStringLiteral("bank_code"), QStringLiteral("12030000")},
            {QStringLiteral("account_number"), QStringLiteral("0000202051")},
            {QStringLiteral("sub_account_number"), QStringLiteral("01")},
        });
    }

    /**
     * An account map that carries one reference account, so that the write path
     * across all three tables can be exercised.
     */
    static QMap<QString, QVariant> createFakeAccountMapWithReferenceAccount()
    {
        auto map = createFakeAccountMap();
        map[QStringLiteral("refAccounts")] = QVariant::fromValue(
            ReferenceAccounts{createFakeReferenceAccount()});

        return map;
    }

    /**
     * Twelve letters. The former version added a random offset to 'A', which also
     * covers the six characters between 'Z' and 'a'.
     */
    static QString randomString()
    {
        static const QString alphabet = QStringLiteral(
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");

        QString result;
        result.reserve(12);

        for (int i = 0; i < 12; ++i) {
            result.append(alphabet.at(generator()->bounded(alphabet.size())));
        }

        return result;
    }

private:
    /**
     * A generator with a fixed seed. A failing run has to be reproducible, which
     * QRandomGenerator::system() cannot give. Consecutive calls still differ, so
     * two accounts built within one test do not collide on their unique id.
     */
    static QRandomGenerator *generator()
    {
        static QRandomGenerator instance(0x0bfa2026u);
        return &instance;
    }
};

} // namespace olbaflinx::core::tests
