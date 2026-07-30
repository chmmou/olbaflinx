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

#include "ui/Models/AccountListModel.h"

#include "core/Banking/Transaction/Transaction.h"

#include <QtTest/QtTest>

using namespace olbaflinx::core::banking;
using namespace olbaflinx::core::banking::account;
using namespace olbaflinx::core::banking::transaction;
using namespace olbaflinx::ui::models;

namespace olbaflinx::ui::models::tests {

class AccountListModelTest final : public QObject
{
    Q_OBJECT

private:
    static QMap<QString, QVariant> accountMap(const QString &accountName)
    {
        QMap<QString, QVariant> map = {};

        map[QStringLiteral("type")] = 1;
        map[QStringLiteral("unique_id")] = 4711;
        map[QStringLiteral("backend_name")] = QStringLiteral("aqhbci");
        map[QStringLiteral("owner_name")] = QStringLiteral("Max Mustermann");
        map[QStringLiteral("account_name")] = accountName;
        map[QStringLiteral("currency")] = QStringLiteral("EUR");
        map[QStringLiteral("iban")] = QStringLiteral("DE02500105170137075030");
        map[QStringLiteral("bic")] = QStringLiteral("INGDDEFF");
        map[QStringLiteral("bank_code")] = QStringLiteral("50010517");
        map[QStringLiteral("bank_name")] = QStringLiteral("ING-DiBa");
        map[QStringLiteral("account_number")] = QStringLiteral("0137075030");
        map[QStringLiteral("balance")] = 12.5;

        return map;
    }

private Q_SLOTS:
    void emptyModelHasNoRows();
    void setItemsCountsOnlyAccounts();
    void setItemsWithEmptyListClearsTheModel();
    void dataReturnsTheMappedRoles();
    void dataOutsideTheModelIsInvalid();
    void roleNamesCoverEveryRole();
};

void AccountListModelTest::emptyModelHasNoRows()
{
    const AccountListModel model;

    QCOMPARE(model.rowCount(), 0);
}

void AccountListModelTest::setItemsCountsOnlyAccounts()
{
    AccountListModel model;

    BankingItems items;
    items << Account::fromMap(accountMap("Girokonto"));
    items << Transaction::fromMap({});
    items << Account::fromMap(accountMap("Sparkonto"));

    model.setItems(items);

    QCOMPARE(model.rowCount(), 2);
}

void AccountListModelTest::setItemsWithEmptyListClearsTheModel()
{
    AccountListModel model;

    BankingItems items;
    items << Account::fromMap(accountMap("Girokonto"));
    model.setItems(items);
    QCOMPARE(model.rowCount(), 1);

    model.setItems({});

    QCOMPARE(model.rowCount(), 0);
}

void AccountListModelTest::dataReturnsTheMappedRoles()
{
    AccountListModel model;

    BankingItems items;
    items << Account::fromMap(accountMap("Girokonto"));
    model.setItems(items);

    const QModelIndex index = model.index(0, 0);

    QCOMPARE(model.data(index, AccountListModel::AccountNameRole).toString(),
             QStringLiteral("Girokonto"));
    QCOMPARE(model.data(index, AccountListModel::OwnerNameRole).toString(),
             QStringLiteral("Max Mustermann"));
    QCOMPARE(model.data(index, AccountListModel::IbanRole).toString(),
             QStringLiteral("DE02500105170137075030"));
    QCOMPARE(model.data(index, AccountListModel::BalanceRole).toDouble(), 12.5);
    QVERIFY(!model.data(index, Qt::DisplayRole).toString().isEmpty());
}

void AccountListModelTest::dataOutsideTheModelIsInvalid()
{
    AccountListModel model;

    QVERIFY(!model.data(model.index(0, 0), AccountListModel::AccountNameRole).isValid());
    QVERIFY(!model.data(QModelIndex(), AccountListModel::AccountNameRole).isValid());
}

void AccountListModelTest::roleNamesCoverEveryRole()
{
    const AccountListModel model;
    const auto roles = model.roleNames();

    QCOMPARE(roles.value(AccountListModel::UniqueIdRole), QByteArrayLiteral("uniqueId"));
    QCOMPARE(roles.value(AccountListModel::BalanceRole), QByteArrayLiteral("balance"));
    QCOMPARE(roles.size(), 9);
}

} // namespace olbaflinx::ui::models::tests

QTEST_MAIN(olbaflinx::ui::models::tests::AccountListModelTest)

#include "tst_accountlistmodel.moc"
