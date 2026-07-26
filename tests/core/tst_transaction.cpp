/**
 * Copyright (C) 2021-2025, Alexander Saal <developer@olbaflinx.chm-projects.de>
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
    static Transaction *createTypedTransaction(TransactionType type)
    {
        auto abTransaction = AB_Transaction_new();
        AB_Transaction_SetType(abTransaction, type);
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

} // namespace olbaflinx::core::banking::transaction::tests

QTEST_APPLESS_MAIN(transaction::tests::TransactionTest)

#include "tst_transaction.moc"
