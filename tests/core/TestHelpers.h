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
#include <QtCore/QVariant>

#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>

using namespace olbaflinx::core::banking::account;

namespace olbaflinx::core::tests {

class TestHelpers
{
public:
    /**
     * Runs one statement against a storage file, past Storage, and hands back
     * the first value of the first row. An invalid QVariant means the file would
     * not open, the statement failed, or it returned no row.
     *
     * It reaches what Storage offers no way to ask: what a column holds after a
     * write, and what a table carries that nothing has read yet.
     */
    static QVariant storageScalar(const QString &file, const QString &key, const QString &statement)
    {
        auto value = QVariant();
        const auto connectionName = QStringLiteral("TestHelpersDirect");

        {
            auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLCIPHER"), connectionName);
            database.setDatabaseName(file);

            if (database.open()) {
                auto quotedKey = key;
                quotedKey.replace(QLatin1Char('\''), QLatin1StringView("''"));

                QSqlQuery query(database);
                if (query.exec(QStringLiteral("PRAGMA key='%1';").arg(quotedKey))
                    && query.exec(statement) && query.next()) {
                    value = query.value(0);
                }

                database.close();
            }
        }
        QSqlDatabase::removeDatabase(connectionName);

        return value;
    }

    /**
     * Puts transactions into a storage past Storage, hung on the identifier the
     * institution assigns. This epic fetches none from a bank, so whoever reads
     * them has to write them first.
     *
     * They all share their account_id and differ in unique_account_id. That is
     * what tells a read over the right column from one over the wrong one.
     */
    static bool putTransactions(const QString &file,
                                const QString &key,
                                quint32 uniqueAccountId,
                                int count,
                                const QString &purpose)
    {
        return storageScalar(file,
                             key,
                             QStringLiteral(
                                 "WITH RECURSIVE seq(n) AS (SELECT 1 UNION ALL SELECT n + 1 FROM "
                                 "seq WHERE n < %2) INSERT INTO transactions (account_id, "
                                 "unique_account_id, purpose) SELECT 1, %1, '%3 ' || n FROM seq "
                                 "RETURNING unique_account_id;")
                                 .arg(uniqueAccountId)
                                 .arg(count)
                                 .arg(purpose))
            .isValid();
    }

    /**
     * Puts transactions that carry a date, an amount and a counterparty, each of
     * them one step apart from the record before it.
     *
     * putTransactions writes none of the three, so a read over it has no expected
     * order in any column but the purpose. Ordering by a column needs values that
     * differ, and it needs them to differ the same way in every column, so that
     * one answer is right for all four.
     *
     * The rows go in even numbers first and odd numbers after, so that the order
     * of the row ids is neither the order of the values nor its reverse. Written
     * in the plain order, a read that ignores the chosen column and falls back on
     * the row id would answer exactly as one that honours it, and a test over it
     * would pass against an implementation that does not order at all.
     *
     * @param firstDate The day of the record that carries the smallest amount, in
     *  ISO form. Every further record moves one day on, so a span that crosses a
     *  month or a year is a matter of choosing the day.
     */
    static bool putOrderedTransactions(const QString &file,
                                       const QString &key,
                                       quint32 uniqueAccountId,
                                       int count,
                                       const QString &firstDate = QStringLiteral("2026-01-01"))
    {
        return storageScalar(file,
                             key,
                             QStringLiteral(
                                 "WITH RECURSIVE seq(n) AS (SELECT 1 UNION ALL SELECT n + 1 FROM "
                                 "seq WHERE n < %2) INSERT INTO transactions (account_id, "
                                 "unique_account_id, purpose, remote_name, `date`, `value`) "
                                 "SELECT 1, %1, 'Buchung ' || v, 'Partner ' || v, "
                                 "date('%3', '+' || (v - 1) || ' days'), v * 1.0 FROM "
                                 "(SELECT CASE WHEN n <= %2 / 2 THEN n * 2 "
                                 "ELSE (n - %2 / 2) * 2 - 1 END AS v FROM seq) "
                                 "RETURNING unique_account_id;")
                                 .arg(uniqueAccountId)
                                 .arg(count)
                                 .arg(firstDate))
            .isValid();
    }

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
     * A reference account with every field set. The entry is shared; nobody has
     * to release it.
     */
    static std::shared_ptr<ReferenceAccount> createFakeReferenceAccount()
    {
        return ReferenceAccount::fromMap({
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
