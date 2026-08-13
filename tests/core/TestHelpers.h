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

#include "core/ApplicationInfo.h"
#include "core/Banking/Account/Account.h"

#include <QtCore/QMap>
#include <QtCore/QRandomGenerator>
#include <QtCore/QString>
#include <QtCore/QVariant>

#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>

using namespace olbaflinx::core;
using namespace olbaflinx::core::banking::account;

namespace olbaflinx::core::tests {

/**
 * What every test needs: the way past Storage into the file, the pass phrase,
 * the application details, and an account to write.
 *
 * Three further headers sit beside this one, each for one subject. A helper
 * belongs here when it fits none of them:
 *
 * - TransactionHelpers.h builds a booking, in either of the two forms.
 * - BankingHelpers.h builds what the banking backend hands over.
 * - ../ui/UiTestHelpers.h reaches the window and its models, which the core
 *   targets deliberately cannot see: they link no Widgets.
 */
class TestHelpers
{
public:
    /**
     * The pass phrase every storage of a test is opened with. It meets the
     * guideline of the core and carries a quote, a backslash and a slash, so a
     * test file is also a test of the escaping.
     */
    static QString password() { return QStringLiteral("M'yF13\"stP\\$44W0$3d/"); }

    /**
     * The shortest pass phrase the guideline still accepts: twelve characters
     * with one of each class. What tests the boundary rather than the escaping.
     */
    static QString minimalPassword() { return QStringLiteral("Aa1!Aa1!Aa1!"); }

    /**
     * The application details of a test. Organisation and version are the same
     * everywhere; the name is what keeps the settings of two test binaries
     * apart, so it is the one thing a caller says.
     */
    static ApplicationInfo applicationInfo(const QString &name)
    {
        return {QStringLiteral("de.chm-projects.olbaflinx.test"), name, QStringLiteral("1.0.0")};
    }

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
     * Runs one statement against a storage file, past Storage, and says whether
     * it went through. For statements that answer with no row: a schema change,
     * a delete. storageScalar cannot serve there, it counts a missing row as a
     * failure.
     */
    static bool runStatement(const QString &file, const QString &key, const QString &statement)
    {
        auto executed = false;
        const auto connectionName = QStringLiteral("TestHelpersStatement");

        {
            auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLCIPHER"), connectionName);
            database.setDatabaseName(file);

            if (database.open()) {
                auto quotedKey = key;
                quotedKey.replace(QLatin1Char('\''), QLatin1StringView("''"));

                QSqlQuery query(database);
                executed = query.exec(QStringLiteral("PRAGMA key='%1';").arg(quotedKey))
                           && query.exec(statement);

                database.close();
            }
        }
        QSqlDatabase::removeDatabase(connectionName);

        return executed;
    }

    /**
     * The number of rows a table holds, read past Storage. What a failed run
     * left behind is exactly what Storage offers no way to ask.
     *
     * @return The count, or -1 when the file would not open or the table is not
     *  there.
     */
    static int rowCount(const QString &file, const QString &key, const QString &table)
    {
        const auto value = storageScalar(file,
                                         key,
                                         QStringLiteral("SELECT COUNT(*) FROM %1;").arg(table));

        return value.isValid() ? value.toInt() : -1;
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
     * An account under an identifier the caller chooses, with a balance it
     * chooses. Where a test has to find its account again, or to hang a balance
     * on it, the random identifier of the map above is of no use.
     */
    static QMap<QString, QVariant> accountMapWith(quint32 uniqueId, double balance)
    {
        auto map = createFakeAccountMap();

        map[QStringLiteral("unique_id")] = uniqueId;
        map[QStringLiteral("balance")] = balance;

        return map;
    }

    /**
     * An account whose every field is a value one can read in a message. What a
     * test of a view needs: the random name of the map above says nothing when a
     * comparison over it fails, and a tree ordered by bank needs banks that are
     * named.
     */
    static QMap<QString, QVariant> namedAccountMap(
        const QString &accountName = QStringLiteral("Girokonto"),
        const QString &bankName = QStringLiteral("ING-DiBa"),
        quint32 uniqueId = 4711)
    {
        return {{QStringLiteral("type"), 1},
                {QStringLiteral("unique_id"), uniqueId},
                {QStringLiteral("backend_name"), QStringLiteral("aqhbci")},
                {QStringLiteral("owner_name"), QStringLiteral("Max Mustermann")},
                {QStringLiteral("account_name"), accountName},
                {QStringLiteral("currency"), QStringLiteral("EUR")},
                {QStringLiteral("iban"), QStringLiteral("DE02500105170137075030")},
                {QStringLiteral("bic"), QStringLiteral("INGDDEFF")},
                {QStringLiteral("bank_code"), QStringLiteral("50010517")},
                {QStringLiteral("bank_name"), bankName},
                {QStringLiteral("account_number"), QStringLiteral("0137075030")},
                {QStringLiteral("balance"), 12.5}};
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
