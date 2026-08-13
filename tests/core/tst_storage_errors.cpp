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

#include "core/ApplicationInfo.h"
#include "core/Banking/Account/Account.h"
#include "core/Banking/Account/ReferenceAccount.h"
#include "core/Banking/Transaction/Transaction.h"
#include "core/Error.h"
#include "core/Storage/Storage.h"

#include "TestHelpers.h"
#include "TransactionHelpers.h"

#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtTest/QtTest>

using namespace olbaflinx::core;
using namespace olbaflinx::core::banking;
using namespace olbaflinx::core::banking::account;
using namespace olbaflinx::core::banking::transaction;
using namespace olbaflinx::core::storage;

namespace olbaflinx::core::storage::tests {

using namespace olbaflinx::core::tests;

/**
 * A record of a type the storage has no table for. Account, Transaction and
 * ReferenceAccount all have one, so the rejecting branch needs a type of its own
 * to stay reachable.
 */
class UnsupportedItem final : public BankingItem
{
public:
    [[nodiscard]] bool isValid() const override { return true; }
    [[nodiscard]] QString toString() const override { return itemType(); }
    [[nodiscard]] QMap<QString, QVariant> toMap() const override
    {
        return {{QStringLiteral("name"), QStringLiteral("Groceries")}};
    }
    [[nodiscard]] QString itemType() const override { return QStringLiteral("Category"); }
};

/**
 * Every error path of Storage. The class used to answer with a bool and to send
 * the reason through a signal that had no receiver anywhere in the project.
 */
class StorageErrorTest final : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir workingDirectory;

    static ApplicationInfo applicationInfo()
    {
        return TestHelpers::applicationInfo(QStringLiteral("OlbaFlinxStorageErrorTest"));
    }

    static QString password() { return TestHelpers::password(); }

    QString storageFile(const QString &name) const
    {
        return workingDirectory.filePath(name + QStringLiteral(".obfx"));
    }

private Q_SLOTS:
    void initTestCase();

    void storeItemRejectsInvalidItem();
    void storeItemRejectsNullItem();
    void storeItemRejectsUnsupportedType();
    void initializeReportsFailureOnUnwritablePath();
    void initializeNamesTheColumnsAnOlderStoreDoesNotHave();
    void errorOccurredCarriesMatchingCode();
    void receiveItemsRejectsAnAccountFilterOnAnotherType_data();
    void receiveItemsRejectsAnAccountFilterOnAnotherType();
    void receiveItemsEmitsProgressWithinRange();
    void receiveItemsFillsTransactionFields();
    void storeItemsEndsAtTheFailingAccountAndKeepsWhatWentIn();
};

void StorageErrorTest::initTestCase()
{
    // Keeps QSettings out of the real user configuration, see QStandardPaths docs.
    QStandardPaths::setTestModeEnabled(true);

    QVERIFY(workingDirectory.isValid());
}

/**
 * An invalid item used to be skipped without a word while the call still
 * answered with success. The caller had no way to tell the two apart.
 */
void StorageErrorTest::storeItemRejectsInvalidItem()
{
    Storage storage(applicationInfo());
    QVERIFY(!storage.setKey(password()).isError());
    storage.setStorageFile(storageFile("invalidItem"));

    QVERIFY(!storage.initialize(true).isError());

    const Transaction transaction;
    QVERIFY(!transaction.isValid());

    const auto error = storage.storeItem(&transaction);

    QVERIFY(error.isError());
    QCOMPARE(error.code(), ErrorCode::InvalidInput);
    QVERIFY(error.message().contains(transaction.itemType()));

    storage.close();
}

void StorageErrorTest::storeItemRejectsNullItem()
{
    Storage storage(applicationInfo());
    QVERIFY(!storage.setKey(password()).isError());
    storage.setStorageFile(storageFile("nullItem"));

    QVERIFY(!storage.initialize(true).isError());

    const auto error = storage.storeItem(nullptr);

    QCOMPARE(error.code(), ErrorCode::InvalidInput);

    storage.close();
}

/**
 * The branch for a type without a table used to be empty. No statement was
 * prepared, and the bind and exec that followed failed with a message that did
 * not name the cause. It used to be reached with a reference account, which now
 * has a table of its own.
 */
void StorageErrorTest::storeItemRejectsUnsupportedType()
{
    Storage storage(applicationInfo());
    QVERIFY(!storage.setKey(password()).isError());
    storage.setStorageFile(storageFile("unsupportedType"));

    QVERIFY(!storage.initialize(true).isError());

    const UnsupportedItem item;
    QVERIFY(item.isValid());

    const auto error = storage.storeItem(&item);

    QVERIFY(error.isError());
    QCOMPARE(error.code(), ErrorCode::NotImplemented);
    QVERIFY(error.message().contains(item.itemType()));

    storage.close();
}

void StorageErrorTest::initializeReportsFailureOnUnwritablePath()
{
    Storage storage(applicationInfo());
    QVERIFY(!storage.setKey(password()).isError());
    storage.setStorageFile(workingDirectory.filePath(QStringLiteral("no/such/directory.obfx")));

    const auto error = storage.initialize(true);

    QVERIFY(error.isError());
    QCOMPARE(error.code(), ErrorCode::DatabaseFailure);
    QVERIFY(!storage.isValid());

    storage.close();
}

/**
 * Version 3 gave accounts two columns of their own. They come into being with
 * the table, so a store written before that carries neither, and the schema run
 * cannot add them: setupTables replays the whole resource on every version step,
 * and an ALTER TABLE would fail the second time round.
 *
 * Such a store is therefore refused, and the message names what is missing. The
 * alternative is a query failing somewhere later on a column nobody mentioned.
 */
void StorageErrorTest::initializeNamesTheColumnsAnOlderStoreDoesNotHave()
{
    const auto file = storageFile("olderStore");

    {
        auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLCIPHER"),
                                                  QStringLiteral("StorageErrorTestOlder"));
        database.setDatabaseName(file);

        QVERIFY(database.open());

        auto key = password();
        key.replace(QLatin1Char('\''), QLatin1StringView("''"));

        QSqlQuery query(database);
        QVERIFY(query.exec(QStringLiteral("PRAGMA key='%1';").arg(key)));

        // The table as version 2 left it: no active, no changed_at.
        QVERIFY(query.exec(QStringLiteral("CREATE TABLE accounts (id integer not null constraint "
                                          "accounts_id_pk primary key autoincrement, `type` "
                                          "integer, unique_id integer, backend_name varchar, "
                                          "owner_name varchar, account_name varchar, currency "
                                          "varchar, memo varchar, iban varchar, bic varchar, "
                                          "country varchar, bank_code varchar, bank_name varchar, "
                                          "branch_id varchar, account_number varchar, "
                                          "sub_account_number varchar, balance double);")));

        database.close();
    }
    QSqlDatabase::removeDatabase(QStringLiteral("StorageErrorTestOlder"));

    Storage storage(applicationInfo());
    QVERIFY(!storage.setKey(password()).isError());
    storage.setStorageFile(file);

    const auto error = storage.initialize(true);

    QVERIFY(error.isError());
    QCOMPARE(error.code(), ErrorCode::SchemaMismatch);
    QVERIFY(error.message().contains(QStringLiteral("active")));
    QVERIFY(error.message().contains(QStringLiteral("changed_at")));

    storage.close();
}

/**
 * The code that went out with the signal used to be PasswordChanged whatever the
 * cause. Reading a table that does not exist has nothing to do with a password.
 */
void StorageErrorTest::errorOccurredCarriesMatchingCode()
{
    Storage storage(applicationInfo());
    QVERIFY(!storage.setKey(password()).isError());
    storage.setStorageFile(storageFile("matchingCode"));

    QVERIFY(!storage.initialize(true).isError());

    QSignalSpy errorSpy(&storage, &Storage::errorOccurred);
    QSignalSpy finishedSpy(&storage, &Storage::finished);

    // A type without a table of its own is answered before anything is started,
    // in this thread, so there is nothing to wait for here.
    storage.receiveItems({.type = Storage::StorageContacts});

    QCOMPARE(errorSpy.count(), 1);
    QCOMPARE(finishedSpy.count(), 1);

    const auto arguments = errorSpy.takeFirst();
    QCOMPARE(arguments.at(0).value<ErrorCode>(), ErrorCode::NotImplemented);
    QVERIFY(!arguments.at(1).toString().isEmpty());

    // An empty table is not the same cause and has to carry its own code. This
    // one is found by the reading thread, so the signal is waited for.
    storage.receiveItems({.type = Storage::StorageAccount});

    QVERIFY(errorSpy.wait());
    QCOMPARE(errorSpy.count(), 1);
    QCOMPARE(errorSpy.takeFirst().at(0).value<ErrorCode>(), ErrorCode::NotFound);

    storage.close();
}

/**
 * The account filter goes over transactions.unique_account_id, the identifier
 * the institution assigns. No other table carries it: refaccounts hangs on the
 * row id of accounts, which no type of the core hands out. A filter on another
 * type would therefore mean a different identifier, and both are quint32, so
 * nothing but this refusal tells them apart.
 */
void StorageErrorTest::receiveItemsRejectsAnAccountFilterOnAnotherType_data()
{
    QTest::addColumn<Storage::Type>("type");

    QTest::newRow("account") << Storage::StorageAccount;
    QTest::newRow("referenceAccount") << Storage::StorageReferenceAccount;
}

void StorageErrorTest::receiveItemsRejectsAnAccountFilterOnAnotherType()
{
    QFETCH(Storage::Type, type);

    Storage storage(applicationInfo());
    QVERIFY(!storage.setKey(password()).isError());
    storage.setStorageFile(storageFile(QTest::currentDataTag()));

    QVERIFY(!storage.initialize(true).isError());

    QSignalSpy errorSpy(&storage, &Storage::errorOccurred);
    QSignalSpy finishedSpy(&storage, &Storage::finished);

    // Answered before anything is started, in this thread, so there is nothing
    // to wait for here.
    storage.receiveItems({.type = type, .accountId = 815});

    QCOMPARE(errorSpy.count(), 1);
    QCOMPARE(finishedSpy.count(), 1);
    QCOMPARE(errorSpy.takeFirst().at(0).value<ErrorCode>(), ErrorCode::InvalidInput);

    storage.close();
}

/**
 * The progress used to come from numRowsAffected(), which is undefined for a
 * SELECT. SQLite answers -1 there, so every value went negative.
 */
void StorageErrorTest::receiveItemsEmitsProgressWithinRange()
{
    Storage storage(applicationInfo());
    QVERIFY(!storage.setKey(password()).isError());
    storage.setStorageFile(storageFile("progressRange"));

    QVERIFY(!storage.initialize(true).isError());

    for (int i = 0; i < 3; ++i) {
        const auto account = TestHelpers::createFakeAccount();
        QVERIFY(!storage.storeItem(account.get()).isError());
    }

    QSignalSpy progressSpy(&storage, &Storage::progressChanged);
    QSignalSpy itemsSpy(&storage, &Storage::itemsReceived);

    storage.receiveItems({.type = Storage::StorageAccount});

    QVERIFY(itemsSpy.wait());
    QCOMPARE(itemsSpy.count(), 1);
    QCOMPARE(progressSpy.count(), 3);

    for (const auto &arguments : std::as_const(progressSpy)) {
        const int progress = arguments.at(0).toInt();
        QVERIFY2(progress >= 0 && progress <= 100,
                 qPrintable(QStringLiteral("progress out of range: %1").arg(progress)));
    }

    QCOMPARE(progressSpy.last().at(0).toInt(), 100);

    storage.close();
}

/**
 * The write side of the property map used to prefix every key with a colon while
 * Transaction read without one. Every transaction read back therefore carried
 * nothing but default values, and no error came of it.
 */
void StorageErrorTest::receiveItemsFillsTransactionFields()
{
    Storage storage(applicationInfo());
    QVERIFY(!storage.setKey(password()).isError());
    storage.setStorageFile(storageFile("transactionFields"));

    QVERIFY(!storage.initialize(true).isError());

    const auto written = TransactionHelpers::transactionFromBackend();
    QVERIFY(written->isValid());
    QVERIFY(!storage.storeItem(written.get()).isError());

    QSignalSpy itemsSpy(&storage, &Storage::itemsReceived);

    storage.receiveItems({.type = Storage::StorageTransaction});

    QVERIFY(itemsSpy.wait());
    QCOMPARE(itemsSpy.count(), 1);

    const auto items = qvariant_cast<BankingItems>(itemsSpy.takeFirst().at(0));
    QCOMPARE(items.size(), 1);

    const auto read = std::dynamic_pointer_cast<Transaction>(items.at(0));
    QVERIFY(read != nullptr);

    QCOMPARE(read->type(), written->type());
    QCOMPARE(read->subType(), written->subType());
    QCOMPARE(read->command(), written->command());
    QCOMPARE(read->uniqueId(), written->uniqueId());
    QCOMPARE(read->uniqueAccountId(), written->uniqueAccountId());
    QCOMPARE(read->localIban(), written->localIban());
    QCOMPARE(read->remoteIban(), written->remoteIban());
    QCOMPARE(read->remoteName(), written->remoteName());
    QCOMPARE(read->purpose(), written->purpose());
    QCOMPARE(read->value(), written->value());
    QCOMPARE(read->currency(), written->currency());

    storage.close();
}

/**
 * The bracket sits around the single account, not around the run. Three accounts
 * go in, the second one cannot be stored. The first stays, the second does not,
 * and the third was never attempted, because an account that fails may be the
 * reason the ones behind it would fail too.
 *
 * A second run of the wizard picks the rest up. That is what the upsert is for.
 */
void StorageErrorTest::storeItemsEndsAtTheFailingAccountAndKeepsWhatWentIn()
{
    const auto file = storageFile("runWithAFailure");

    Storage storage(applicationInfo());
    QVERIFY(!storage.setKey(password()).isError());
    storage.setStorageFile(file);
    QVERIFY(!storage.initialize(true).isError());

    const auto first = TestHelpers::createFakeAccount();
    // AB_AccountType_Invalid is -1, and it is one of the two types isValid()
    // rejects. The store answers such an account with InvalidInput.
    const auto second = TestHelpers::createFakeAccount(AB_AccountType_Invalid);
    const auto third = TestHelpers::createFakeAccount();

    QVERIFY(first->isValid());
    QVERIFY(!second->isValid());
    QVERIFY(third->isValid());

    QSignalSpy errorSpy(&storage, &Storage::errorOccurred);
    QSignalSpy storedSpy(&storage, &Storage::itemsStored);
    QSignalSpy finishedSpy(&storage, &Storage::finished);

    storage.storeItems(BankingItems{first, second, third});

    QVERIFY(finishedSpy.wait());

    QCOMPARE(errorSpy.count(), 1);
    QCOMPARE(errorSpy.takeFirst().at(0).value<ErrorCode>(), ErrorCode::InvalidInput);

    QCOMPARE(storedSpy.count(), 1);
    QCOMPARE(storedSpy.takeFirst().at(0).toInt(), 1);

    storage.close();

    // One row, and it belongs to the first account. The third was not attempted.
    QCOMPARE(TestHelpers::rowCount(file, password(), QStringLiteral("accounts")), 1);

    // The first account went in whole. Its balance belongs to the same bracket,
    // so a row there is what tells a complete write from a half one.
    QCOMPARE(TestHelpers::rowCount(file, password(), QStringLiteral("balances")), 1);
}

} // namespace olbaflinx::core::storage::tests

QTEST_MAIN(olbaflinx::core::storage::tests::StorageErrorTest)

#include "tst_storage_errors.moc"
