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

#include "ui/Models/AccountTreeModel.h"

#include "core/Banking/Transaction/Transaction.h"

#include <QtCore/QLocale>
#include <QtTest/QtTest>

using namespace olbaflinx::core::banking;
using namespace olbaflinx::core::banking::account;
using namespace olbaflinx::core::banking::transaction;
using namespace olbaflinx::ui::models;

namespace olbaflinx::ui::models::tests {

class AccountTreeModelTest final : public QObject
{
    Q_OBJECT

private:
    static QMap<QString, QVariant> accountMap(const QString &accountName,
                                              const QString &bankName = QStringLiteral("ING-DiBa"),
                                              quint32 uniqueId = 4711)
    {
        QMap<QString, QVariant> map = {};

        map[QStringLiteral("type")] = 1;
        map[QStringLiteral("unique_id")] = uniqueId;
        map[QStringLiteral("backend_name")] = QStringLiteral("aqhbci");
        map[QStringLiteral("owner_name")] = QStringLiteral("Max Mustermann");
        map[QStringLiteral("account_name")] = accountName;
        map[QStringLiteral("currency")] = QStringLiteral("EUR");
        map[QStringLiteral("iban")] = QStringLiteral("DE02500105170137075030");
        map[QStringLiteral("bic")] = QStringLiteral("INGDDEFF");
        map[QStringLiteral("bank_code")] = QStringLiteral("50010517");
        map[QStringLiteral("bank_name")] = bankName;
        map[QStringLiteral("account_number")] = QStringLiteral("0137075030");
        map[QStringLiteral("balance")] = 12.5;

        return map;
    }

private Q_SLOTS:
    void initTestCase();

    void emptyModelHasNoRows();
    void setItemsCountsOnlyAccounts();
    void setItemsWithEmptyListClearsTheModel();
    void dataReturnsTheMappedRoles();
    void dataOutsideTheModelIsInvalid();
    void roleNamesCoverEveryRole();
    void threeAccountsAtTwoBanksBecomeTwoBankNodes();
    void anInactiveAccountAndItsLoneBankStayAway();
    void aBankNodeCarriesNoAccountId();
    void banksAndAccountsFollowTheOrderOfTheLanguage();
    void twoAccountsOfTheSameBankShareOneNode();
    void anAccountWithoutAnIbanStaysInTheTree();
};

void AccountTreeModelTest::initTestCase()
{
    // The order of banks and accounts is the one of the language in use, not the
    // one of the character values. Without a language of its own the test would
    // measure whatever the machine happens to be set to.
    QLocale::setDefault(QLocale(QLocale::German, QLocale::Germany));
}

void AccountTreeModelTest::emptyModelHasNoRows()
{
    const AccountTreeModel model;

    QCOMPARE(model.rowCount(), 0);
}

void AccountTreeModelTest::setItemsCountsOnlyAccounts()
{
    AccountTreeModel model;

    BankingItems items;
    items << Account::fromMap(accountMap("Girokonto"));
    items << Transaction::fromMap({});
    items << Account::fromMap(accountMap("Sparkonto", QStringLiteral("ING-DiBa"), 4712));

    model.setItems(items);

    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.rowCount(model.index(0, 0)), 2);
}

void AccountTreeModelTest::setItemsWithEmptyListClearsTheModel()
{
    AccountTreeModel model;

    BankingItems items;
    items << Account::fromMap(accountMap("Girokonto"));
    model.setItems(items);
    QCOMPARE(model.rowCount(), 1);

    model.setItems({});

    QCOMPARE(model.rowCount(), 0);
}

void AccountTreeModelTest::dataReturnsTheMappedRoles()
{
    AccountTreeModel model;

    BankingItems items;
    items << Account::fromMap(accountMap("Girokonto"));
    model.setItems(items);

    const QModelIndex index = model.index(0, 0, model.index(0, 0));

    QCOMPARE(model.data(index, AccountTreeModel::AccountNameRole).toString(),
             QStringLiteral("Girokonto"));
    QCOMPARE(model.data(index, AccountTreeModel::OwnerNameRole).toString(),
             QStringLiteral("Max Mustermann"));
    QCOMPARE(model.data(index, AccountTreeModel::IbanRole).toString(),
             QStringLiteral("DE02500105170137075030"));
    QCOMPARE(model.data(index, AccountTreeModel::BalanceRole).toDouble(), 12.5);
    QVERIFY(!model.data(index, Qt::DisplayRole).toString().isEmpty());
}

void AccountTreeModelTest::dataOutsideTheModelIsInvalid()
{
    AccountTreeModel model;

    QVERIFY(!model.data(model.index(0, 0), AccountTreeModel::AccountNameRole).isValid());
    QVERIFY(!model.data(QModelIndex(), AccountTreeModel::AccountNameRole).isValid());
}

void AccountTreeModelTest::roleNamesCoverEveryRole()
{
    const AccountTreeModel model;
    const auto roles = model.roleNames();

    QCOMPARE(roles.value(AccountTreeModel::UniqueIdRole), QByteArrayLiteral("uniqueId"));
    QCOMPARE(roles.value(AccountTreeModel::BalanceRole), QByteArrayLiteral("balance"));
    QCOMPARE(roles.size(), 9);
}

void AccountTreeModelTest::threeAccountsAtTwoBanksBecomeTwoBankNodes()
{
    AccountTreeModel model;

    BankingItems items;
    items << Account::fromMap(accountMap("Girokonto", QStringLiteral("ING-DiBa"), 4711));
    items << Account::fromMap(accountMap("Sparkonto", QStringLiteral("ING-DiBa"), 4712));
    items << Account::fromMap(accountMap("Tagesgeld", QStringLiteral("Postbank"), 4713));

    model.setItems(items);

    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.data(model.index(0, 0), Qt::DisplayRole).toString(), QStringLiteral("ING-DiBa"));
    QCOMPARE(model.rowCount(model.index(0, 0)), 2);
    QCOMPARE(model.data(model.index(1, 0), Qt::DisplayRole).toString(), QStringLiteral("Postbank"));
    QCOMPARE(model.rowCount(model.index(1, 0)), 1);
}

void AccountTreeModelTest::anInactiveAccountAndItsLoneBankStayAway()
{
    AccountTreeModel model;

    auto inactive = accountMap("Altkonto", QStringLiteral("Postbank"), 4713);
    inactive[QStringLiteral("active")] = false;

    BankingItems items;
    items << Account::fromMap(accountMap("Girokonto", QStringLiteral("ING-DiBa"), 4711));
    items << Account::fromMap(inactive);

    model.setItems(items);

    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0, 0), Qt::DisplayRole).toString(), QStringLiteral("ING-DiBa"));
    QCOMPARE(model.rowCount(model.index(0, 0)), 1);
}

void AccountTreeModelTest::aBankNodeCarriesNoAccountId()
{
    AccountTreeModel model;

    BankingItems items;
    items << Account::fromMap(accountMap("Girokonto"));
    model.setItems(items);

    const QModelIndex bank = model.index(0, 0);

    QVERIFY(!model.data(bank, AccountTreeModel::UniqueIdRole).isValid());
    QVERIFY(model.data(model.index(0, 0, bank), AccountTreeModel::UniqueIdRole).isValid());
}

void AccountTreeModelTest::banksAndAccountsFollowTheOrderOfTheLanguage()
{
    AccountTreeModel model;

    BankingItems items;
    items << Account::fromMap(accountMap("Bankhaus Nord", QStringLiteral("Bankhaus Nord"), 4711));
    items << Account::fromMap(accountMap("Sparkonto", QStringLiteral("Ärztebank"), 4712));
    items << Account::fromMap(accountMap("Ölkonto", QStringLiteral("Ärztebank"), 4713));

    model.setItems(items);

    // Compared by character value "Bankhaus Nord" would come first and "Sparkonto"
    // before "Ölkonto", because the umlauts sit above every plain letter there.
    QCOMPARE(model.data(model.index(0, 0), Qt::DisplayRole).toString(), QStringLiteral("Ärztebank"));
    QCOMPARE(model.data(model.index(1, 0), Qt::DisplayRole).toString(),
             QStringLiteral("Bankhaus Nord"));

    const QModelIndex bank = model.index(0, 0);
    QCOMPARE(model.data(model.index(0, 0, bank), AccountTreeModel::AccountNameRole).toString(),
             QStringLiteral("Ölkonto"));
    QCOMPARE(model.data(model.index(1, 0, bank), AccountTreeModel::AccountNameRole).toString(),
             QStringLiteral("Sparkonto"));
}

void AccountTreeModelTest::twoAccountsOfTheSameBankShareOneNode()
{
    AccountTreeModel model;

    BankingItems items;
    items << Account::fromMap(accountMap("Girokonto", QStringLiteral("Sparkasse"), 4711));
    items << Account::fromMap(accountMap("Sparkonto", QStringLiteral("Sparkasse"), 4712));

    model.setItems(items);

    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.rowCount(model.index(0, 0)), 2);
}

void AccountTreeModelTest::anAccountWithoutAnIbanStaysInTheTree()
{
    AccountTreeModel model;

    auto withoutIban = accountMap("Girokonto");
    withoutIban[QStringLiteral("iban")] = QString();

    BankingItems items;
    items << Account::fromMap(withoutIban);

    model.setItems(items);

    const QModelIndex account = model.index(0, 0, model.index(0, 0));

    QCOMPARE(model.rowCount(model.index(0, 0)), 1);
    QVERIFY(model.data(account, AccountTreeModel::IbanRole).toString().isEmpty());
    QCOMPARE(model.data(account, AccountTreeModel::AccountNameRole).toString(),
             QStringLiteral("Girokonto"));
}

} // namespace olbaflinx::ui::models::tests

QTEST_MAIN(olbaflinx::ui::models::tests::AccountTreeModelTest)

#include "tst_accounttreemodel.moc"
