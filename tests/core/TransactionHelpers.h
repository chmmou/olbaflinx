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

#include "core/Banking/Transaction/Transaction.h"

#include "TestHelpers.h"

#include <aqbanking/types/value.h>

#include <QtCore/QDate>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QVariant>

#include <memory>

using namespace olbaflinx::core::banking::transaction;

namespace olbaflinx::core::tests {

/**
 * What a booking of a test carries. Every field has a value that stands on its
 * own, so a caller names the two or three that matter to it and leaves the rest.
 *
 * The default is a booking with every field of the property map filled: that is
 * what a round trip through the storage and a check for personal data in a log
 * entry both need, and it was written out twice before this.
 */
struct TransactionSpec
{
    TransactionType type = AB_Transaction_TypeTransaction;
    TransactionSubType subType = AB_Transaction_SubTypeStandard;
    TransactionCommand command = AB_Transaction_CommandGetTransactions;

    quint32 uniqueId = 4711;
    quint32 uniqueAccountId = 815;

    QDate date = {};
    QDate valutaDate = {};

    double value = 42.5;
    QString currency = QStringLiteral("EUR");

    QString purpose = QStringLiteral("Miete Februar");
    QString remoteName = QStringLiteral("Erika Musterfrau");
    QString remoteIban = QStringLiteral("DE02120300000000202051");
    QString remoteAccountNumber = QStringLiteral("0137075030");
    QString localName = QStringLiteral("Max Mustermann");
    QString localIban = QStringLiteral("DE02500105170137075030");
    QString endToEndReference = {};

    /**
     * Left empty, the core forms the fingerprint while it writes. A value here
     * is what tells a booking that already carries one.
     */
    QString fingerprint = {};
};

/**
 * The two ways a test needs a booking, and the two ways it can put one into a
 * storage.
 *
 * A booking comes either from the banking backend, as an AB_TRANSACTION, or
 * from the storage, as a property map. Both forms are built here, from the same
 * description, so that a test of the one is a test of the other as well.
 *
 * The two putters below are the third way, and it is deliberately not one of
 * these: they write past the application, by SQL, and leave the fingerprint
 * unset. That is what a holding of an older file looks like, and no path of the
 * application produces it.
 */
class TransactionHelpers
{
public:
    /**
     * A booking the way the banking backend reports one.
     *
     * AB_Transaction_new() leaves the type unknown, which isValid() rejects, so
     * a transaction that is worth anything cannot come from the default
     * constructor.
     */
    static std::shared_ptr<Transaction> transactionFromBackend(const TransactionSpec &spec = {})
    {
        AB_TRANSACTION *abTransaction = AB_Transaction_new();

        AB_Transaction_SetType(abTransaction, spec.type);
        AB_Transaction_SetSubType(abTransaction, spec.subType);
        AB_Transaction_SetCommand(abTransaction, spec.command);
        AB_Transaction_SetUniqueId(abTransaction, spec.uniqueId);
        AB_Transaction_SetUniqueAccountId(abTransaction, spec.uniqueAccountId);

        setText(abTransaction, &AB_Transaction_SetPurpose, spec.purpose);
        setText(abTransaction, &AB_Transaction_SetRemoteName, spec.remoteName);
        setText(abTransaction, &AB_Transaction_SetRemoteIban, spec.remoteIban);
        setText(abTransaction, &AB_Transaction_SetRemoteAccountNumber, spec.remoteAccountNumber);
        setText(abTransaction, &AB_Transaction_SetLocalName, spec.localName);
        setText(abTransaction, &AB_Transaction_SetLocalIban, spec.localIban);
        setText(abTransaction, &AB_Transaction_SetEndToEndReference, spec.endToEndReference);
        setText(abTransaction, &AB_Transaction_SetHash, spec.fingerprint);

        AB_VALUE *value = AB_Value_new();
        AB_Value_SetValueFromDouble(value, spec.value);
        AB_Value_SetCurrency(value, spec.currency.toUtf8().constData());
        // The setter duplicates what it is given, so the extra dup this used to
        // pass was never released.
        AB_Transaction_SetValue(abTransaction, value);
        AB_Value_free(value);

        auto transaction = std::make_shared<Transaction>(abTransaction);
        AB_Transaction_free(abTransaction);

        return transaction;
    }

    /**
     * The property map of a booking, the way fromMap expects it and the way the
     * storage writes it.
     *
     * Built without going through the backend, so that a map without a
     * fingerprint is possible at all: toMap forms the value on every call.
     */
    static QMap<QString, QVariant> transactionMap(const TransactionSpec &spec = {})
    {
        auto map = QMap<QString, QVariant>{
            {QStringLiteral("type"), static_cast<int>(spec.type)},
            {QStringLiteral("sub_type"), static_cast<int>(spec.subType)},
            {QStringLiteral("command"), static_cast<int>(spec.command)},
            {QStringLiteral("unique_id"), spec.uniqueId},
            {QStringLiteral("unique_account_id"), spec.uniqueAccountId},
            {QStringLiteral("date"), spec.date},
            {QStringLiteral("valuta_date"), spec.valutaDate},
            {QStringLiteral("value"), spec.value},
            {QStringLiteral("currency"), spec.currency},
            {QStringLiteral("purpose"), spec.purpose},
            {QStringLiteral("remote_name"), spec.remoteName},
            {QStringLiteral("remote_iban"), spec.remoteIban},
            {QStringLiteral("remote_account_number"), spec.remoteAccountNumber},
            {QStringLiteral("local_name"), spec.localName},
            {QStringLiteral("local_iban"), spec.localIban},
            {QStringLiteral("end_to_end_reference"), spec.endToEndReference},
        };

        if (!spec.fingerprint.isEmpty()) {
            map[QStringLiteral("hash")] = spec.fingerprint;
        }

        return map;
    }

    /**
     * A booking as the storage takes it, built from the map above.
     */
    static std::shared_ptr<Transaction> transactionFromMap(const TransactionSpec &spec = {})
    {
        return Transaction::fromMap(transactionMap(spec));
    }

    /**
     * A run of bookings that differ in their identifier and their purpose, and
     * therefore in their fingerprint. What a fetch of several bookings hands to
     * the storage.
     */
    static BankingItems transactionRun(quint32 uniqueAccountId, quint32 count)
    {
        auto items = BankingItems();

        for (quint32 index = 1; index <= count; ++index) {
            items << transactionFromMap({
                .type = AB_Transaction_TypeStatement,
                .uniqueId = index,
                .uniqueAccountId = uniqueAccountId,
                .date = QDate(2026, 1, 1).addDays(index),
                .valutaDate = QDate(2026, 1, 1).addDays(index),
                .value = index * 1.5,
                .purpose = QStringLiteral("Booking %1").arg(index),
                .remoteName = QStringLiteral("Partner %1").arg(index),
            });
        }

        return items;
    }

    /**
     * A booking the write path refuses, so that a run can fail in its middle. A
     * transaction without a type is what isValid() rejects.
     */
    static std::shared_ptr<Transaction> unusableTransaction()
    {
        return Transaction::fromMap(
            {{QStringLiteral("type"), static_cast<int>(AB_Transaction_TypeNone)}});
    }

    /**
     * Puts bookings into a storage past Storage, hung on the identifier the
     * institution assigns. They carry no fingerprint, which no path of the
     * application produces and an older holding is full of.
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
        return TestHelpers::storageScalar(
                   file,
                   key,
                   QStringLiteral("WITH RECURSIVE seq(n) AS (SELECT 1 UNION ALL SELECT n + 1 FROM "
                                  "seq WHERE n < %2) INSERT INTO transactions (account_id, "
                                  "unique_account_id, purpose) SELECT 1, %1, '%3 ' || n FROM seq "
                                  "RETURNING unique_account_id;")
                       .arg(uniqueAccountId)
                       .arg(count)
                       .arg(purpose))
            .isValid();
    }

    /**
     * Puts bookings that carry a date, an amount and a counterparty, each of
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
     * Each record carries its amount as its unique_id as well, so that a run over
     * several pages can be checked by the set of identifiers it delivered.
     *
     * The first date is the day of the record with the smallest amount, in ISO
     * form. Every further record moves dayStep days on, so a span that crosses
     * a month or a year is a matter of choosing the day. A step of zero puts
     * every record on the same day, which leaves the order to the second
     * criterion alone.
     */
    static bool putOrderedTransactions(const QString &file,
                                       const QString &key,
                                       quint32 uniqueAccountId,
                                       int count,
                                       const QString &firstDate = QStringLiteral("2026-01-01"),
                                       int dayStep = 1)
    {
        return TestHelpers::storageScalar(
                   file,
                   key,
                   QStringLiteral("WITH RECURSIVE seq(n) AS (SELECT 1 UNION ALL SELECT n + 1 FROM "
                                  "seq WHERE n < %2) INSERT INTO transactions (account_id, "
                                  "unique_account_id, unique_id, purpose, remote_name, `date`, "
                                  "`value`) "
                                  "SELECT 1, %1, v, 'Buchung ' || v, 'Partner ' || v, "
                                  "date('%3', '+' || ((v - 1) * %4) || ' days'), v * 1.0 FROM "
                                  "(SELECT CASE WHEN n <= %2 / 2 THEN n * 2 "
                                  "ELSE (n - %2 / 2) * 2 - 1 END AS v FROM seq) "
                                  "RETURNING unique_account_id;")
                       .arg(uniqueAccountId)
                       .arg(count)
                       .arg(firstDate)
                       .arg(dayStep))
            .isValid();
    }

    /**
     * One booking with the fields the filter bar looks at, written past Storage.
     */
    static bool putTransaction(const QString &file,
                               const QString &key,
                               quint32 uniqueAccountId,
                               const QString &purpose,
                               const QString &remoteName,
                               const QDate &date,
                               double value)
    {
        return TestHelpers::storageScalar(
                   file,
                   key,
                   QStringLiteral("INSERT INTO transactions (account_id, unique_account_id, "
                                  "purpose, remote_name, date, value) VALUES (1, %1, '%2', "
                                  "'%3', '%4', %5) RETURNING unique_account_id;")
                       .arg(uniqueAccountId)
                       .arg(purpose, remoteName, date.toString(Qt::ISODate))
                       .arg(value))
            .isValid();
    }

private:
    /**
     * Hands a text to a setter of the backend, or leaves the field alone when
     * there is nothing to set. An empty QString would reach the setter as an
     * empty C string, which is not the same as never having been set.
     */
    static void setText(AB_TRANSACTION *transaction,
                        void (*setter)(AB_TRANSACTION *, const char *),
                        const QString &text)
    {
        if (text.isEmpty()) {
            return;
        }

        setter(transaction, text.toUtf8().constData());
    }
};

} // namespace olbaflinx::core::tests
