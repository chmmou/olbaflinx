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

#include "ui/Models/TransactionTableModel.h"

#include "core/Banking/Account/Account.h"

#include <QtTest/QtTest>

using namespace olbaflinx::core::banking;
using namespace olbaflinx::core::banking::account;
using namespace olbaflinx::core::banking::transaction;
using namespace olbaflinx::ui::models;

namespace olbaflinx::ui::models::tests {

class TransactionTableModelTest final : public QObject
{
    Q_OBJECT

private:
    static QDate bookingDate() { return QDate(2026, 2, 17); }

    static QMap<QString, QVariant> transactionMap(const QString &purpose, double value)
    {
        QMap<QString, QVariant> map = {};

        map[QStringLiteral("type")] = 1;
        map[QStringLiteral("unique_id")] = 4711;
        map[QStringLiteral("date")] = bookingDate();
        map[QStringLiteral("purpose")] = purpose;
        map[QStringLiteral("value")] = value;
        map[QStringLiteral("currency")] = QStringLiteral("EUR");
        map[QStringLiteral("remote_name")] = QStringLiteral("Erika Musterfrau");
        map[QStringLiteral("remote_iban")] = QStringLiteral("DE02120300000000202051");

        return map;
    }

    /**
     * A model holding a single transaction, so that a column can be asked what
     * it shows.
     */
    static void fill(TransactionTableModel &model, const QString &purpose, double value)
    {
        BankingItems items;
        items << Transaction::fromMap(transactionMap(purpose, value));

        model.setItems(items);
    }

    static QString shownAt(const TransactionTableModel &model, int column)
    {
        return model.data(model.index(0, column), Qt::DisplayRole).toString();
    }

private Q_SLOTS:
    void emptyModelHasNoRows();
    void setItemsCountsOnlyTransactions();
    void setItemsWithEmptyListClearsTheModel();
    void dataReturnsTheMappedRoles();
    void dataReturnsTheMappedRoles_data();
    void dataOutsideTheModelIsInvalid();
    void roleNamesCoverEveryRole();
    void theModelCarriesFourColumnsEachWithAHeader();
    void everyColumnShowsWhatItsHeaderPromises();
    void anAmountInDebitCarriesItsSignInTheText();
};

void TransactionTableModelTest::emptyModelHasNoRows()
{
    const TransactionTableModel model;

    QCOMPARE(model.rowCount(), 0);
}

void TransactionTableModelTest::setItemsCountsOnlyTransactions()
{
    TransactionTableModel model;

    BankingItems items;
    items << Transaction::fromMap(transactionMap("Miete", -750.0));
    items << Account::fromMap({});
    items << Transaction::fromMap(transactionMap("Gehalt", 2500.0));

    model.setItems(items);

    // Account::fromMap returns an empty pointer for an empty map, which the
    // model skips.
    QCOMPARE(model.rowCount(), 2);
}

void TransactionTableModelTest::setItemsWithEmptyListClearsTheModel()
{
    TransactionTableModel model;

    BankingItems items;
    items << Transaction::fromMap(transactionMap("Miete", -750.0));
    model.setItems(items);
    QCOMPARE(model.rowCount(), 1);

    model.setItems({});

    QCOMPARE(model.rowCount(), 0);
}

void TransactionTableModelTest::dataReturnsTheMappedRoles_data()
{
    QTest::addColumn<QString>("purpose");
    QTest::addColumn<double>("value");

    QTest::newRow("ascii") << QStringLiteral("Rent") << -750.0;
    QTest::newRow("umlaute") << QStringLiteral("Rückzahlung Möbelkauf") << 42.5;
    QTest::newRow("nicht-latin") << QStringLiteral("振込 テスト") << 1.0;
}

void TransactionTableModelTest::dataReturnsTheMappedRoles()
{
    QFETCH(QString, purpose);
    QFETCH(double, value);

    TransactionTableModel model;

    BankingItems items;
    items << Transaction::fromMap(transactionMap(purpose, value));
    model.setItems(items);

    const QModelIndex index = model.index(0, 0);

    QCOMPARE(model.data(index, TransactionTableModel::PurposeRole).toString(), purpose);
    QCOMPARE(model.data(index, TransactionTableModel::ValueRole).toDouble(), value);
    QCOMPARE(model.data(index, TransactionTableModel::RemoteNameRole).toString(),
             QStringLiteral("Erika Musterfrau"));
    QVERIFY(!model.data(index, Qt::DisplayRole).toString().isEmpty());
}

void TransactionTableModelTest::dataOutsideTheModelIsInvalid()
{
    TransactionTableModel model;

    QVERIFY(!model.data(model.index(0, 0), TransactionTableModel::PurposeRole).isValid());
    QVERIFY(!model.data(QModelIndex(), TransactionTableModel::PurposeRole).isValid());
}

void TransactionTableModelTest::roleNamesCoverEveryRole()
{
    const TransactionTableModel model;
    const auto roles = model.roleNames();

    QCOMPARE(roles.value(TransactionTableModel::UniqueIdRole), QByteArrayLiteral("uniqueId"));
    QCOMPARE(roles.value(TransactionTableModel::PurposeRole), QByteArrayLiteral("purpose"));
    QCOMPARE(roles.size(), 9);
}

/**
 * Four columns, no more. A fifth would be one the view has no place for, and
 * every one of them names itself in the header.
 */
void TransactionTableModelTest::theModelCarriesFourColumnsEachWithAHeader()
{
    TransactionTableModel model;
    fill(model, QStringLiteral("Miete"), -750.0);

    QCOMPARE(model.columnCount(), 4);

    auto headers = QStringList();
    for (int column = 0; column < model.columnCount(); ++column) {
        const auto header = model.headerData(column, Qt::Horizontal, Qt::DisplayRole).toString();
        QVERIFY(!header.isEmpty());
        headers << header;
    }

    // Four distinct names. Two columns under one heading would leave the reader
    // guessing which is which.
    QCOMPARE(QSet<QString>(headers.cbegin(), headers.cend()).size(), 4);

    // A row has no more columns than the header does.
    QVERIFY(!model.index(0, 4).isValid());
}

/**
 * The order is fixed: date, the other party, purpose, amount. The amount carries
 * its currency, because a number without one says nothing.
 */
void TransactionTableModelTest::everyColumnShowsWhatItsHeaderPromises()
{
    TransactionTableModel model;
    fill(model, QStringLiteral("Rückzahlung Möbelkauf"), 42.5);

    QCOMPARE(shownAt(model, 0), QLocale().toString(bookingDate(), QLocale::ShortFormat));
    QCOMPARE(shownAt(model, 1), QStringLiteral("Erika Musterfrau"));
    QCOMPARE(shownAt(model, 2), QStringLiteral("Rückzahlung Möbelkauf"));
    QVERIFY(shownAt(model, 3).contains(QStringLiteral("EUR")));
}

/**
 * A debit is told from a credit by the text, not by a colour. Whoever cannot
 * make out the colour still reads the sign.
 *
 * The sign, not a bracket: QLocale::toCurrencyString writes a negative amount in
 * accounting style in several locales, and this pins down that the column does
 * not take that route.
 */
void TransactionTableModelTest::anAmountInDebitCarriesItsSignInTheText()
{
    const auto negativeSign = QLocale().negativeSign();

    TransactionTableModel debit;
    fill(debit, QStringLiteral("Miete"), -750.0);
    QVERIFY(shownAt(debit, 3).contains(negativeSign));

    TransactionTableModel credit;
    fill(credit, QStringLiteral("Gehalt"), 2500.0);
    QVERIFY(!shownAt(credit, 3).contains(negativeSign));

    QVERIFY(shownAt(debit, 3) != shownAt(credit, 3));
}

} // namespace olbaflinx::ui::models::tests

QTEST_MAIN(olbaflinx::ui::models::tests::TransactionTableModelTest)

#include "tst_transactiontablemodel.moc"
