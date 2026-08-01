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

#include <QtTest/QtTest>

using namespace olbaflinx::core::banking;
using namespace olbaflinx::core::banking::transaction;

namespace olbaflinx::core::banking::transaction::tests {

class TransactionTest final : public QObject
{
    Q_OBJECT

private:
    /**
     * Builds a transaction with a type set, which the default constructor cannot produce.
     * AB_Transaction_new() initialises type to AB_Transaction_TypeUnknown.
     */
    static Transaction *createTypedTransaction(TransactionType type,
                                               const QString &endToEndReference = QString())
    {
        auto abTransaction = AB_Transaction_new();
        AB_Transaction_SetType(abTransaction, type);

        if (!endToEndReference.isEmpty()) {
            AB_Transaction_SetEndToEndReference(abTransaction,
                                                endToEndReference.toUtf8().constData());
        }

        AB_Transaction_SetUniqueId(abTransaction, 4711);
        AB_Transaction_SetLocalIban(abTransaction, "DE02500105170137075030");
        AB_Transaction_SetRemoteIban(abTransaction, "DE02120300000000202051");
        AB_Transaction_SetRemoteName(abTransaction, "Erika Musterfrau");
        AB_Transaction_SetLocalName(abTransaction, "Max Mustermann");
        AB_Transaction_SetRemoteAccountNumber(abTransaction, "0137075030");

        auto value = AB_Value_new();
        AB_Value_SetValueFromDouble(value, 42.5);
        AB_Value_SetCurrency(value, "EUR");
        AB_Transaction_SetValue(abTransaction, AB_Value_dup(value));
        AB_Value_free(value);

        const auto transaction = new Transaction(abTransaction);
        AB_Transaction_free(abTransaction);

        return transaction;
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
    void toMapCarriesAHashOfTheEndToEndReferenceOnly();
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
    const QScopedPointer<Transaction> transaction(createTypedTransaction(AB_Transaction_TypeNone));

    QVERIFY(!transaction->isValid());
}

void TransactionTest::transactionIsValidAcceptsKnownType()
{
    const QScopedPointer<Transaction> transaction(
        createTypedTransaction(AB_Transaction_TypeTransaction));

    QCOMPARE(transaction->type(), AB_Transaction_TypeTransaction);
    QVERIFY(transaction->isValid());
}

void TransactionTest::transactionToStringOmitsPersonalData()
{
    const QScopedPointer<Transaction> transaction(
        createTypedTransaction(AB_Transaction_TypeTransaction));

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
    const QScopedPointer<Transaction> written(
        createTypedTransaction(AB_Transaction_TypeTransaction));

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
    const QScopedPointer<Transaction> written(
        createTypedTransaction(AB_Transaction_TypeTransaction));

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
    const QScopedPointer<Transaction> filled(
        createTypedTransaction(AB_Transaction_TypeTransaction));

    QCOMPARE(empty.toMap().keys(), filled->toMap().keys());
}

/**
 * The hash is built over the end to end reference and over nothing else. Two
 * transactions that differ in type, amount and parties therefore share a hash as
 * long as neither carries such a reference, which is the ordinary case for an
 * incoming booking. The column named hash identifies nothing.
 *
 * This test states what the class does today. It fails once the hash covers the
 * content, which is the point at which it has to be rewritten.
 */
void TransactionTest::toMapCarriesAHashOfTheEndToEndReferenceOnly()
{
    const QScopedPointer<Transaction> first(createTypedTransaction(AB_Transaction_TypeTransaction));
    const QScopedPointer<Transaction> second(createTypedTransaction(AB_Transaction_TypeTransfer));
    const QScopedPointer<Transaction> referenced(
        createTypedTransaction(AB_Transaction_TypeTransaction, QStringLiteral("E2E-4711")));

    const auto hashOf = [](const Transaction *transaction) {
        return transaction->toMap().value(QStringLiteral("hash")).toString();
    };

    QVERIFY(!hashOf(first.data()).isEmpty());

    // Different type, different amount, same empty reference.
    QCOMPARE(hashOf(second.data()), hashOf(first.data()));

    QVERIFY(hashOf(referenced.data()) != hashOf(first.data()));
}

void TransactionTest::itemTypeIsTheNameOfTheClass()
{
    const Transaction transaction;

    QCOMPARE(transaction.itemType(), QStringLiteral("Transaction"));
}

} // namespace olbaflinx::core::banking::transaction::tests

QTEST_APPLESS_MAIN(transaction::tests::TransactionTest)

#include "tst_transaction.moc"
