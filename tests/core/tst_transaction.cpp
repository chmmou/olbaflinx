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

#include "core/Banking/Transaction/Transaction.h"

#include "TransactionHelpers.h"

#include <QtTest/QtTest>

using namespace olbaflinx::core::banking;
using namespace olbaflinx::core::banking::transaction;

namespace olbaflinx::core::banking::transaction::tests {

using namespace olbaflinx::core::tests;

class TransactionTest final : public QObject
{
    Q_OBJECT

private:
    /**
     * A transaction with a type set, which the default constructor cannot
     * produce: AB_Transaction_new() leaves the type unknown.
     */
    static std::shared_ptr<Transaction> createTypedTransaction(
        TransactionType type, const QString &endToEndReference = QString())
    {
        return TransactionHelpers::transactionFromBackend(
            {.type = type, .endToEndReference = endToEndReference});
    }

    /**
     * The property map of a typed transaction, as fromMap expects it.
     */
    static QMap<QString, QVariant> mapOfATypedTransaction()
    {
        return createTypedTransaction(AB_Transaction_TypeTransaction)->toMap();
    }

private Q_SLOTS:
    void transactionIsValidRejectsUnknownType();
    void transactionIsValidRejectsNoneType();
    void transactionIsValidAcceptsKnownType();
    void transactionToStringOmitsPersonalData();
    void transactionToStringHandlesEmptyTransaction();
    void toMapAndBackYieldsTheSameTransaction();
    void toMapKeepsNonAsciiNames();
    void toMapOfAnEmptyTransactionCarriesTheSameKeys();
    void toMapCarriesAHashOverTheContent();
    void theStringIdForTheApplicationIsNotRestored();
    void dateSurvivesTheRoundTrip();
    void anUnreadableDateIsInvalidRatherThanToday();
    void itemTypeIsTheNameOfTheClass();
};

void TransactionTest::transactionIsValidRejectsUnknownType()
{
    const Transaction transaction;

    QCOMPARE(transaction.type(), AB_Transaction_TypeUnknown);
    QVERIFY(!transaction.isValid());
}

void TransactionTest::transactionIsValidRejectsNoneType()
{
    const auto transaction = createTypedTransaction(AB_Transaction_TypeNone);

    QVERIFY(!transaction->isValid());
}

void TransactionTest::transactionIsValidAcceptsKnownType()
{
    const auto transaction = createTypedTransaction(AB_Transaction_TypeTransaction);

    QCOMPARE(transaction->type(), AB_Transaction_TypeTransaction);
    QVERIFY(transaction->isValid());
}

void TransactionTest::transactionToStringOmitsPersonalData()
{
    const auto transaction = createTypedTransaction(AB_Transaction_TypeTransaction);

    // Guards the assertions below: contains() on an empty string is always true.
    QVERIFY(!transaction->localIban().isEmpty());
    QVERIFY(!transaction->remoteIban().isEmpty());
    QVERIFY(!transaction->remoteName().isEmpty());
    QVERIFY(!transaction->localName().isEmpty());
    QVERIFY(!transaction->remoteAccountNumber().isEmpty());

    const QString result = transaction->toString();

    // A log entry must not carry an IBAN, an account number or a personal name.
    QVERIFY(!result.contains(transaction->localIban()));
    QVERIFY(!result.contains(transaction->remoteIban()));
    QVERIFY(!result.contains(transaction->remoteName()));
    QVERIFY(!result.contains(transaction->localName()));
    QVERIFY(!result.contains(transaction->remoteAccountNumber()));

    // The entry still has to identify the transaction.
    QVERIFY(result.contains(QString::number(transaction->uniqueId())));
}

void TransactionTest::transactionToStringHandlesEmptyTransaction()
{
    const Transaction transaction;

    const QString result = transaction.toString();

    QVERIFY(!result.isEmpty());
    QVERIFY(result.contains(QString::number(transaction.uniqueId())));
}

/**
 * The round trip is what the storage relies on. The write side used to prefix
 * every key with a colon while the read side asked without one, so every
 * transaction read back carried nothing but default values.
 */
void TransactionTest::toMapAndBackYieldsTheSameTransaction()
{
    const auto written = createTypedTransaction(AB_Transaction_TypeTransaction);

    const auto read = Transaction::fromMap(written->toMap());
    QVERIFY(read != nullptr);

    QCOMPARE(read->type(), written->type());
    QCOMPARE(read->uniqueId(), written->uniqueId());
    QCOMPARE(read->localIban(), written->localIban());
    QCOMPARE(read->remoteIban(), written->remoteIban());
    QCOMPARE(read->remoteName(), written->remoteName());
    QCOMPARE(read->localName(), written->localName());
    QCOMPARE(read->remoteAccountNumber(), written->remoteAccountNumber());
    QCOMPARE(read->value(), written->value());
    QCOMPARE(read->currency(), written->currency());
}

void TransactionTest::toMapKeepsNonAsciiNames()
{
    const auto written = createTypedTransaction(AB_Transaction_TypeTransaction);

    auto map = written->toMap();
    map[QStringLiteral("remote_name")] = QStringLiteral("Erika Müller-Groß");
    map[QStringLiteral("purpose")] = QStringLiteral("Miete für März, 90 €");

    const auto read = Transaction::fromMap(map);
    QVERIFY(read != nullptr);

    QCOMPARE(read->remoteName(), QStringLiteral("Erika Müller-Groß"));
    QCOMPARE(read->purpose(), QStringLiteral("Miete für März, 90 €"));
}

/**
 * A transaction without any field set still has to answer with the full set of
 * columns. A map that shrinks with the content would leave the binding of the
 * insert statement without a value.
 */
void TransactionTest::toMapOfAnEmptyTransactionCarriesTheSameKeys()
{
    const Transaction empty;
    const auto filled = createTypedTransaction(AB_Transaction_TypeTransaction);

    QCOMPARE(empty.toMap().keys(), filled->toMap().keys());
}

/**
 * The hash used to be taken over the end to end reference and nothing else. Two
 * bookings that differ in parties and amount then shared it as long as neither
 * carried such a reference, which is the ordinary case for an incoming booking.
 */
void TransactionTest::toMapCarriesAHashOverTheContent()
{
    const auto first = createTypedTransaction(AB_Transaction_TypeTransaction);
    const auto referenced = createTypedTransaction(AB_Transaction_TypeTransaction,
                                                   QStringLiteral("E2E-4711"));

    const auto hashOf = [](const Transaction *transaction) {
        return transaction->toMap().value(QStringLiteral("hash")).toString();
    };

    QVERIFY(!hashOf(first.get()).isEmpty());
    QVERIFY(hashOf(referenced.get()) != hashOf(first.get()));

    // Same content, same hash. The fingerprint is a function of the booking, not
    // of the moment it was taken.
    const auto again = createTypedTransaction(AB_Transaction_TypeTransaction);
    QCOMPARE(hashOf(again.get()), hashOf(first.get()));

    // Another party has to change it. Under the old rule it did not. Only the
    // name differs from the one above, so nothing else can account for it.
    const auto other = TransactionHelpers::transactionFromBackend(
        {.remoteName = QStringLiteral("Klaus Anders")});

    QVERIFY(hashOf(other.get()) != hashOf(first.get()));
}

/**
 * The backend allocates this field wherever it fills it and releases it
 * nowhere, so it is dropped on the way in. The column itself keeps its place;
 * only its content is gone.
 */
void TransactionTest::theStringIdForTheApplicationIsNotRestored()
{
    auto map = mapOfATypedTransaction();
    map[QStringLiteral("string_id_for_application")] = QStringLiteral("OLB-4711");

    const auto transaction = Transaction::fromMap(map);
    QVERIFY(transaction != nullptr);

    QVERIFY(transaction->stringIdForApplication().isEmpty());
    QVERIFY(transaction->toMap().contains(QStringLiteral("string_id_for_application")));
}

/**
 * A GWEN_DATE carries a year, a month and a day. The template used to ask for a
 * time as well, so the parse failed and the caller was handed today instead.
 */
void TransactionTest::dateSurvivesTheRoundTrip()
{
    auto map = mapOfATypedTransaction();
    map[QStringLiteral("date")] = QDate(2024, 3, 17);
    map[QStringLiteral("valuta_date")] = QDate(2024, 3, 19);

    const auto transaction = Transaction::fromMap(map);
    QVERIFY(transaction != nullptr);

    QCOMPARE(transaction->date(), QDate(2024, 3, 17));
    QCOMPARE(transaction->valutaDate(), QDate(2024, 3, 19));
}

/**
 * A booking without a date must not be given one. An invented day in booking
 * data is worse than a missing one.
 */
void TransactionTest::anUnreadableDateIsInvalidRatherThanToday()
{
    auto map = mapOfATypedTransaction();
    map[QStringLiteral("date")] = QDate();

    const auto transaction = Transaction::fromMap(map);
    QVERIFY(transaction != nullptr);

    QVERIFY(!transaction->date().isValid());
}

void TransactionTest::itemTypeIsTheNameOfTheClass()
{
    const Transaction transaction;

    QCOMPARE(transaction.itemType(), QStringLiteral("Transaction"));
}

} // namespace olbaflinx::core::banking::transaction::tests

QTEST_APPLESS_MAIN(transaction::tests::TransactionTest)

#include "tst_transaction.moc"
