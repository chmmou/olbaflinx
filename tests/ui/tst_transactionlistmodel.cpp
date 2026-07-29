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

#include "ui/Models/TransactionListModel.h"

#include "core/Banking/Account/Account.h"

#include <QtTest/QtTest>

using namespace olbaflinx::core::banking;
using namespace olbaflinx::core::banking::account;
using namespace olbaflinx::core::banking::transaction;
using namespace olbaflinx::ui::models;

namespace olbaflinx::ui::models::tests {

class TransactionListModelTest final : public QObject
{
    Q_OBJECT

private:
    static QMap<QString, QVariant> transactionMap(const QString &purpose, double value)
    {
        QMap<QString, QVariant> map = {};

        // Die Spaltennamen tragen hier kein Praefix, siehe Transaction::fromMap.
        map[QStringLiteral("type")] = 1;
        map[QStringLiteral("unique_id")] = 4711;
        map[QStringLiteral("purpose")] = purpose;
        map[QStringLiteral("value")] = value;
        map[QStringLiteral("currency")] = QStringLiteral("EUR");
        map[QStringLiteral("remote_name")] = QStringLiteral("Erika Musterfrau");
        map[QStringLiteral("remote_iban")] = QStringLiteral("DE02120300000000202051");

        return map;
    }

private Q_SLOTS:
    void emptyModelHasNoRows();
    void setItemsCountsOnlyTransactions();
    void setItemsWithEmptyListClearsTheModel();
    void dataReturnsTheMappedRoles();
    void dataReturnsTheMappedRoles_data();
    void dataOutsideTheModelIsInvalid();
    void roleNamesCoverEveryRole();
};

void TransactionListModelTest::emptyModelHasNoRows()
{
    const TransactionListModel model;

    QCOMPARE(model.rowCount(), 0);
}

void TransactionListModelTest::setItemsCountsOnlyTransactions()
{
    TransactionListModel model;

    BankingItems items;
    items << Transaction::fromMap(transactionMap("Miete", -750.0));
    items << Account::fromMap({});
    items << Transaction::fromMap(transactionMap("Gehalt", 2500.0));

    model.setItems(items);

    // Account::fromMap liefert bei leerer Tabelle einen leeren Zeiger, der beim
    // Umsetzen uebergangen wird.
    QCOMPARE(model.rowCount(), 2);
}

void TransactionListModelTest::setItemsWithEmptyListClearsTheModel()
{
    TransactionListModel model;

    BankingItems items;
    items << Transaction::fromMap(transactionMap("Miete", -750.0));
    model.setItems(items);
    QCOMPARE(model.rowCount(), 1);

    model.setItems({});

    QCOMPARE(model.rowCount(), 0);
}

void TransactionListModelTest::dataReturnsTheMappedRoles_data()
{
    QTest::addColumn<QString>("purpose");
    QTest::addColumn<double>("value");

    QTest::newRow("ascii") << QStringLiteral("Rent") << -750.0;
    QTest::newRow("umlaute") << QStringLiteral("Rückzahlung Möbelkauf") << 42.5;
    QTest::newRow("nicht-latin") << QStringLiteral("振込 テスト") << 1.0;
}

void TransactionListModelTest::dataReturnsTheMappedRoles()
{
    QFETCH(QString, purpose);
    QFETCH(double, value);

    TransactionListModel model;

    BankingItems items;
    items << Transaction::fromMap(transactionMap(purpose, value));
    model.setItems(items);

    const QModelIndex index = model.index(0, 0);

    QCOMPARE(model.data(index, TransactionListModel::PurposeRole).toString(), purpose);
    QCOMPARE(model.data(index, TransactionListModel::ValueRole).toDouble(), value);
    QCOMPARE(model.data(index, TransactionListModel::RemoteNameRole).toString(),
             QStringLiteral("Erika Musterfrau"));
    QVERIFY(!model.data(index, Qt::DisplayRole).toString().isEmpty());
}

void TransactionListModelTest::dataOutsideTheModelIsInvalid()
{
    TransactionListModel model;

    QVERIFY(!model.data(model.index(0, 0), TransactionListModel::PurposeRole).isValid());
    QVERIFY(!model.data(QModelIndex(), TransactionListModel::PurposeRole).isValid());
}

void TransactionListModelTest::roleNamesCoverEveryRole()
{
    const TransactionListModel model;
    const auto roles = model.roleNames();

    QCOMPARE(roles.value(TransactionListModel::UniqueIdRole), QByteArrayLiteral("uniqueId"));
    QCOMPARE(roles.value(TransactionListModel::PurposeRole), QByteArrayLiteral("purpose"));
    QCOMPARE(roles.size(), 9);
}

} // namespace olbaflinx::ui::models::tests

QTEST_MAIN(olbaflinx::ui::models::tests::TransactionListModelTest)

#include "tst_transactionlistmodel.moc"
