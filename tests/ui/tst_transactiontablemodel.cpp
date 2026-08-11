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

#include "core/ApplicationInfo.h"
#include "core/Banking/Account/Account.h"
#include "core/Storage/Storage.h"

#include "TestHelpers.h"

#include <QtTest/QtTest>

#include <QtCore/QTemporaryDir>

#include <QtSql/QSqlDatabase>

#include <memory>

using namespace olbaflinx::core;
using namespace olbaflinx::core::banking;
using namespace olbaflinx::core::banking::account;
using namespace olbaflinx::core::banking::transaction;
using namespace olbaflinx::core::storage;
using namespace olbaflinx::core::tests;
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

    std::unique_ptr<QTemporaryDir> workingDirectory;

    static QString password() { return QStringLiteral("M'yF13\"stP\\$44W0$3d/"); }

    /**
     * The upper bound a wait may take before the test counts as failed. It is
     * not a wait: every use returns the moment the condition holds.
     */
    static constexpr int workerTimeoutMs = 30000;

    /**
     * How many rows the view asks for in one go. The tests name it because the
     * end of a page is where a record falls out or turns up twice.
     */
    static constexpr int pageSize = 100;

    static constexpr quint32 firstAccount = 815;
    static constexpr quint32 secondAccount = 4711;
    static constexpr quint32 thirdAccount = 2026;

    [[nodiscard]] QString storageFile() const
    {
        return workingDirectory->filePath(QStringLiteral("storage.obfx"));
    }

    static ApplicationInfo applicationInfo()
    {
        return {QStringLiteral("de.chm-projects.olbaflinx.test"),
                QStringLiteral("OlbaFlinxTransactionTableModelTest"),
                QStringLiteral("1.0.0")};
    }

    [[nodiscard]] bool openStorage(Storage &storage) const
    {
        if (storage.setKey(password()).isError()) {
            return false;
        }

        storage.setStorageFile(storageFile());

        return !storage.initialize(true).isError();
    }

    [[nodiscard]] bool putTransactions(quint32 uniqueAccountId,
                                       int count,
                                       const QString &purpose = QStringLiteral("Buchung")) const
    {
        return TestHelpers::putTransactions(storageFile(),
                                            password(),
                                            uniqueAccountId,
                                            count,
                                            purpose);
    }

    [[nodiscard]] bool putOrderedTransactions(
        quint32 uniqueAccountId,
        int count,
        const QString &firstDate = QStringLiteral("2026-01-01"),
        int dayStep = 1) const
    {
        return TestHelpers::putOrderedTransactions(storageFile(),
                                                   password(),
                                                   uniqueAccountId,
                                                   count,
                                                   firstDate,
                                                   dayStep);
    }

    /**
     * Fetches page after page until the model says there is nothing left. Every
     * round waits for the request to come back; without a view nobody else asks.
     */
    static void loadEverything(TransactionTableModel &model)
    {
        // Nothing can be fetched while a request is running, so the first one
        // has to be through before the paging starts.
        QTRY_VERIFY_WITH_TIMEOUT(!model.isReading(), workerTimeoutMs);

        while (model.canFetchMore(QModelIndex())) {
            const int before = model.rowCount();

            model.fetchMore(QModelIndex());

            QTRY_VERIFY_WITH_TIMEOUT(!model.isReading(), workerTimeoutMs);
            QVERIFY(model.rowCount() > before || model.atEnd());
        }
    }

    static QSet<quint32> identifiersOf(const TransactionTableModel &model)
    {
        auto identifiers = QSet<quint32>();

        for (int row = 0; row < model.rowCount(); ++row) {
            identifiers.insert(
                model.data(model.index(row, 0), TransactionTableModel::UniqueIdRole).toUInt());
        }

        return identifiers;
    }

    static QStringList purposesOf(const TransactionTableModel &model)
    {
        auto purposes = QStringList();

        for (int row = 0; row < model.rowCount(); ++row) {
            purposes << model.data(model.index(row, 0), TransactionTableModel::PurposeRole)
                            .toString();
        }

        return purposes;
    }

    static QVariantList valuesOf(const TransactionTableModel &model, int role)
    {
        auto values = QVariantList();

        for (int row = 0; row < model.rowCount(); ++row) {
            values << model.data(model.index(row, 0), role);
        }

        return values;
    }

    /**
     * Whether the left value may stand above the right one. Compared in the type
     * the role carries and not in the text the column shows: an amount orders
     * numerically, a date chronologically, a name by its characters.
     */
    static bool notAfter(const QVariant &left, const QVariant &right)
    {
        if (left.typeId() == QMetaType::QDate) {
            return left.toDate() <= right.toDate();
        }

        if (left.typeId() == QMetaType::Double) {
            return left.toDouble() <= right.toDouble();
        }

        return left.toString() <= right.toString();
    }

    static bool isOrdered(const QVariantList &values, Qt::SortOrder order)
    {
        for (int index = 1; index < values.size(); ++index) {
            const auto &previous = values.at(index - 1);
            const auto &current = values.at(index);

            const bool inOrder = order == Qt::AscendingOrder ? notAfter(previous, current)
                                                             : notAfter(current, previous);
            if (!inOrder) {
                return false;
            }
        }

        return true;
    }

private Q_SLOTS:
    void init();
    void cleanup();

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

    void aChangeOfAccountReplacesTheContentCompletely();
    void aChangeOfAccountReturnsBeforeTheTransactionsArrive();
    void manyChangesOfAccountLeaveNoConnectionBehind();
    void aChangeDuringARunningReadRaisesNoErrorAndTheLastOneWins();
    void aChangeOfFilterDropsTheRowsAndStartsOver();
    void aFilterOutlivesAChangeOfAccountAndAppliesToTheNextOne();
    void theCountUnderTheFilterIsTheOneOfTheWholeHolding();

    void everyColumnOrdersUpAndDown();
    void everyColumnOrdersUpAndDown_data();
    void amountsOrderNumericallyAndDatesChronologically();
    void theOrderReachesTheWholeHoldingAndNotTheLoadedPage();
    void theOrderOutlivesAChangeOfAccountButNotTheStorage();

    void pagingDeliversEveryTransactionExactlyOnce();
    void aChangeOfOrderDuringARunningRequestDiscardsItsResult();
    void aHoldingThatFitsOnePageEndsWithIt();
    void aHoldingThatFitsOnePageEndsWithIt_data();
    void aFailureWhileLoadingMoreLeavesTheRowsAndStopsAsking();
};

void TransactionTableModelTest::init()
{
    workingDirectory = std::make_unique<QTemporaryDir>();
    QVERIFY(workingDirectory->isValid());
}

void TransactionTableModelTest::cleanup()
{
    workingDirectory.reset();
}

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

/**
 * A change of account swaps the content, it does not add to it. The rows of the
 * account that was shown go at once, before the new ones are anywhere near, so
 * that a holding never stands under the wrong name.
 */
void TransactionTableModelTest::aChangeOfAccountReplacesTheContentCompletely()
{
    Storage storage(applicationInfo());
    QVERIFY(openStorage(storage));

    QVERIFY(putTransactions(firstAccount, 3));
    QVERIFY(putTransactions(secondAccount, 5));

    TransactionTableModel model;
    model.setStorage(&storage);

    model.setAccountId(firstAccount);
    QTRY_COMPARE_WITH_TIMEOUT(model.rowCount(), 3, workerTimeoutMs);

    model.setAccountId(secondAccount);

    // Straight after the call, before anything was read.
    QCOMPARE(model.rowCount(), 0);

    QTRY_COMPARE_WITH_TIMEOUT(model.rowCount(), 5, workerTimeoutMs);
    QCOMPARE(model.accountId(), secondAccount);

    storage.close();
}

/**
 * The change hands the reading to a thread of its own. The call comes back
 * before the transactions are there, and they arrive in the thread that asked,
 * which is the only one allowed to touch a view.
 */
void TransactionTableModelTest::aChangeOfAccountReturnsBeforeTheTransactionsArrive()
{
    Storage storage(applicationInfo());
    QVERIFY(openStorage(storage));

    // More than one window holds, so that the read has work to do.
    QVERIFY(putTransactions(firstAccount, 500));
    QVERIFY(putTransactions(secondAccount, 500));

    TransactionTableModel model;
    model.setStorage(&storage);

    QThread *const callingThread = QThread::currentThread();
    QThread *deliveryThread = nullptr;

    // The rows are appended, so their arrival is an insert and not a reset. The
    // reset is what empties the model when the account changes.
    connect(&model, &QAbstractItemModel::rowsInserted, &model, [&] {
        deliveryThread = QThread::currentThread();
    });

    model.setAccountId(firstAccount);
    QCOMPARE(model.rowCount(), 0);
    QTRY_VERIFY_WITH_TIMEOUT(model.rowCount() > 0, workerTimeoutMs);

    model.setAccountId(secondAccount);
    QCOMPARE(model.rowCount(), 0);
    QTRY_VERIFY_WITH_TIMEOUT(model.rowCount() > 0, workerTimeoutMs);

    QCOMPARE(deliveryThread, callingThread);

    storage.close();
}

/**
 * A change does not only drop the result of the run that was going, it lets that
 * run end. The second connection it reads on is closed and unregistered, so a
 * hundred changes leave as many connections behind as none.
 */
void TransactionTableModelTest::manyChangesOfAccountLeaveNoConnectionBehind()
{
    Storage storage(applicationInfo());
    QVERIFY(openStorage(storage));

    QVERIFY(putTransactions(firstAccount, 3));
    QVERIFY(putTransactions(secondAccount, 3));

    TransactionTableModel model;
    model.setStorage(&storage);

    const auto connectionsWithoutARead = QSqlDatabase::connectionNames().size();

    for (int i = 0; i < 20; ++i) {
        model.setAccountId(i % 2 == 0 ? firstAccount : secondAccount);
    }

    QTRY_COMPARE_WITH_TIMEOUT(model.rowCount(), 3, workerTimeoutMs);
    QTRY_COMPARE_WITH_TIMEOUT(QSqlDatabase::connectionNames().size(),
                              connectionsWithoutARead,
                              workerTimeoutMs);

    storage.close();
}

/**
 * Changing the account while a read is running is an everyday move and not a
 * failure. The storage takes no second read, so the request waits for the end of
 * the one that is going; of three changes made in a row the last one is what the
 * user gets to see, and no message reaches the status bar.
 */
void TransactionTableModelTest::aChangeDuringARunningReadRaisesNoErrorAndTheLastOneWins()
{
    Storage storage(applicationInfo());
    QVERIFY(openStorage(storage));

    QVERIFY(putTransactions(firstAccount, 3));
    QVERIFY(putTransactions(secondAccount, 5));
    QVERIFY(putTransactions(thirdAccount, 7));

    TransactionTableModel model;
    model.setStorage(&storage);

    QSignalSpy errorSpy(&storage, &Storage::errorOccurred);

    // Three in a row, none of them waiting for the one before.
    model.setAccountId(firstAccount);
    model.setAccountId(secondAccount);
    model.setAccountId(thirdAccount);

    QTRY_COMPARE_WITH_TIMEOUT(model.rowCount(), 7, workerTimeoutMs);
    QCOMPARE(model.accountId(), thirdAccount);
    QCOMPARE(errorSpy.count(), 0);

    storage.close();
}

/**
 * The filter is the third thing that makes a running request stale, next to the
 * account and the order. It drops what stands and asks again, so that the list
 * never mixes two conditions.
 */
void TransactionTableModelTest::aChangeOfFilterDropsTheRowsAndStartsOver()
{
    Storage storage(applicationInfo());
    QVERIFY(openStorage(storage));

    QVERIFY(putTransactions(firstAccount, 5, QStringLiteral("Miete")));
    QVERIFY(putTransactions(firstAccount, 3, QStringLiteral("Gehalt")));

    TransactionTableModel model;
    model.setStorage(&storage);

    model.setAccountId(firstAccount);
    QTRY_COMPARE_WITH_TIMEOUT(model.rowCount(), 8, workerTimeoutMs);

    model.setFilter({.text = QStringLiteral("Gehalt")});

    // Straight after the call, before the narrower read has run.
    QCOMPARE(model.rowCount(), 0);

    QTRY_COMPARE_WITH_TIMEOUT(model.rowCount(), 3, workerTimeoutMs);

    for (const auto &purpose : purposesOf(model)) {
        QVERIFY(purpose.startsWith(QStringLiteral("Gehalt")));
    }

    storage.close();
}

/**
 * A user who is looking for something keeps looking for it when he changes the
 * account. The filter therefore stays and applies to the account he picks next.
 */
void TransactionTableModelTest::aFilterOutlivesAChangeOfAccountAndAppliesToTheNextOne()
{
    Storage storage(applicationInfo());
    QVERIFY(openStorage(storage));

    QVERIFY(putTransactions(firstAccount, 5, QStringLiteral("Miete")));
    QVERIFY(putTransactions(firstAccount, 3, QStringLiteral("Gehalt")));
    QVERIFY(putTransactions(secondAccount, 4, QStringLiteral("Miete")));
    QVERIFY(putTransactions(secondAccount, 2, QStringLiteral("Gehalt")));

    TransactionTableModel model;
    model.setStorage(&storage);

    model.setAccountId(firstAccount);
    model.setFilter({.text = QStringLiteral("Gehalt")});

    QTRY_COMPARE_WITH_TIMEOUT(model.rowCount(), 3, workerTimeoutMs);

    model.setAccountId(secondAccount);

    QCOMPARE(model.filter().text, QStringLiteral("Gehalt"));
    QTRY_COMPARE_WITH_TIMEOUT(model.rowCount(), 2, workerTimeoutMs);

    for (const auto &purpose : purposesOf(model)) {
        QVERIFY(purpose.startsWith(QStringLiteral("Gehalt")));
    }

    storage.close();
}

/**
 * The number stands for the whole holding under the filter, not for the page
 * that was read. A holding larger than one window is what tells the two apart.
 */
void TransactionTableModelTest::theCountUnderTheFilterIsTheOneOfTheWholeHolding()
{
    Storage storage(applicationInfo());
    QVERIFY(openStorage(storage));

    QVERIFY(putTransactions(firstAccount, 120, QStringLiteral("Miete")));
    QVERIFY(putTransactions(firstAccount, 80, QStringLiteral("Gehalt")));

    TransactionTableModel model;
    model.setStorage(&storage);

    QSignalSpy totalSpy(&model, &TransactionTableModel::totalRowsChanged);

    model.setAccountId(firstAccount);
    QTRY_COMPARE_WITH_TIMEOUT(model.totalRows(), 200, workerTimeoutMs);

    // One page holds a hundred, so the loaded rows say nothing about the number.
    QCOMPARE(model.rowCount(), pageSize);
    QVERIFY(!totalSpy.isEmpty());

    // Eighty fit on one page, and there the two numbers do meet.
    model.setFilter({.text = QStringLiteral("Gehalt")});
    QTRY_COMPARE_WITH_TIMEOUT(model.totalRows(), 80, workerTimeoutMs);
    QTRY_COMPARE_WITH_TIMEOUT(model.rowCount(), 80, workerTimeoutMs);

    // A filter that leaves nothing answers with nought rather than with the
    // number of the account.
    model.setFilter({.text = QStringLiteral("Versicherung")});
    QTRY_COMPARE_WITH_TIMEOUT(model.totalRows(), 0, workerTimeoutMs);
    QCOMPARE(model.rowCount(), 0);

    storage.close();
}

void TransactionTableModelTest::everyColumnOrdersUpAndDown_data()
{
    QTest::addColumn<int>("column");
    QTest::addColumn<int>("role");

    QTest::newRow("date") << static_cast<int>(TransactionTableModel::DateColumn)
                          << static_cast<int>(TransactionTableModel::DateRole);
    QTest::newRow("counterparty") << static_cast<int>(TransactionTableModel::RemoteNameColumn)
                                  << static_cast<int>(TransactionTableModel::RemoteNameRole);
    QTest::newRow("purpose") << static_cast<int>(TransactionTableModel::PurposeColumn)
                             << static_cast<int>(TransactionTableModel::PurposeRole);
    QTest::newRow("amount") << static_cast<int>(TransactionTableModel::ValueColumn)
                            << static_cast<int>(TransactionTableModel::ValueRole);
}

/**
 * Every column of the view orders, and it orders both ways. Ordering drops what
 * stands and reads again, so the rows are gone the moment the order changes and
 * come back under the new one.
 */
void TransactionTableModelTest::everyColumnOrdersUpAndDown()
{
    QFETCH(int, column);
    QFETCH(int, role);

    Storage storage(applicationInfo());
    QVERIFY(openStorage(storage));

    QVERIFY(putOrderedTransactions(firstAccount, 12));

    TransactionTableModel model;
    model.setStorage(&storage);

    model.setAccountId(firstAccount);
    QTRY_COMPARE_WITH_TIMEOUT(model.rowCount(), 12, workerTimeoutMs);

    model.sort(column, Qt::AscendingOrder);
    QCOMPARE(model.rowCount(), 0);
    QTRY_COMPARE_WITH_TIMEOUT(model.rowCount(), 12, workerTimeoutMs);
    QVERIFY(isOrdered(valuesOf(model, role), Qt::AscendingOrder));

    model.sort(column, Qt::DescendingOrder);
    QCOMPARE(model.rowCount(), 0);
    QTRY_COMPARE_WITH_TIMEOUT(model.rowCount(), 12, workerTimeoutMs);
    QVERIFY(isOrdered(valuesOf(model, role), Qt::DescendingOrder));

    storage.close();
}

/**
 * The order follows the value, not the text that is made of it. Nine before ten
 * is what tells the two apart, because as text ten stands first. The dates cross
 * a turn of the year for the same reason.
 */
void TransactionTableModelTest::amountsOrderNumericallyAndDatesChronologically()
{
    Storage storage(applicationInfo());
    QVERIFY(openStorage(storage));

    QVERIFY(putOrderedTransactions(firstAccount, 12, QStringLiteral("2025-12-27")));

    TransactionTableModel model;
    model.setStorage(&storage);

    model.setAccountId(firstAccount);
    QTRY_COMPARE_WITH_TIMEOUT(model.rowCount(), 12, workerTimeoutMs);

    model.sort(TransactionTableModel::ValueColumn, Qt::AscendingOrder);
    QTRY_COMPARE_WITH_TIMEOUT(model.rowCount(), 12, workerTimeoutMs);

    const auto amounts = valuesOf(model, TransactionTableModel::ValueRole);
    QCOMPARE(amounts.at(8).toDouble(), 9.0);
    QCOMPARE(amounts.at(9).toDouble(), 10.0);
    QCOMPARE(amounts.last().toDouble(), 12.0);

    model.sort(TransactionTableModel::DateColumn, Qt::AscendingOrder);
    QTRY_COMPARE_WITH_TIMEOUT(model.rowCount(), 12, workerTimeoutMs);

    const auto dates = valuesOf(model, TransactionTableModel::DateRole);
    QCOMPARE(dates.first().toDate(), QDate(2025, 12, 27));
    QCOMPARE(dates.last().toDate(), QDate(2026, 1, 7));

    storage.close();
}

/**
 * The order reaches the whole holding of the account and not the page that was
 * read. Three thousand records, of which one page stands: after ordering by
 * amount the largest of all three thousand is at the top, not the largest of the
 * page.
 */
void TransactionTableModelTest::theOrderReachesTheWholeHoldingAndNotTheLoadedPage()
{
    Storage storage(applicationInfo());
    QVERIFY(openStorage(storage));

    QVERIFY(putOrderedTransactions(firstAccount, 3000));

    TransactionTableModel model;
    model.setStorage(&storage);

    model.setAccountId(firstAccount);
    QTRY_COMPARE_WITH_TIMEOUT(model.totalRows(), 3000, workerTimeoutMs);

    // One page of them, whatever a page holds. Far short of the holding is what
    // the test needs, not a particular number.
    QVERIFY(model.rowCount() > 0);
    QVERIFY(model.rowCount() < model.totalRows());

    model.sort(TransactionTableModel::ValueColumn, Qt::DescendingOrder);
    QCOMPARE(model.rowCount(), 0);
    QTRY_VERIFY_WITH_TIMEOUT(model.rowCount() > 0, workerTimeoutMs);

    QCOMPARE(model.data(model.index(0, 0), TransactionTableModel::ValueRole).toDouble(), 3000.0);

    // The other way round as well. The largest amount stands at the top of the
    // order the view opens with, so descending alone would also be answered by a
    // view that never ordered.
    model.sort(TransactionTableModel::ValueColumn, Qt::AscendingOrder);
    QCOMPARE(model.rowCount(), 0);
    QTRY_VERIFY_WITH_TIMEOUT(model.rowCount() > 0, workerTimeoutMs);

    QCOMPARE(model.data(model.index(0, 0), TransactionTableModel::ValueRole).toDouble(), 1.0);

    storage.close();
}

/**
 * Whoever ordered by amount is still after the largest one when he picks the
 * next account, so the order outlives the change. It does not outlive the
 * storage: giving up the account is how the window says that the storage was
 * closed, and what comes next opens under the order the view starts with.
 */
void TransactionTableModelTest::theOrderOutlivesAChangeOfAccountButNotTheStorage()
{
    Storage storage(applicationInfo());
    QVERIFY(openStorage(storage));

    QVERIFY(putOrderedTransactions(firstAccount, 12));
    QVERIFY(putOrderedTransactions(secondAccount, 12));

    TransactionTableModel model;
    model.setStorage(&storage);

    model.setAccountId(firstAccount);
    QTRY_COMPARE_WITH_TIMEOUT(model.rowCount(), 12, workerTimeoutMs);

    model.sort(TransactionTableModel::ValueColumn, Qt::AscendingOrder);
    QTRY_COMPARE_WITH_TIMEOUT(model.rowCount(), 12, workerTimeoutMs);
    QVERIFY(isOrdered(valuesOf(model, TransactionTableModel::ValueRole), Qt::AscendingOrder));

    model.setAccountId(secondAccount);
    QTRY_COMPARE_WITH_TIMEOUT(model.rowCount(), 12, workerTimeoutMs);
    QVERIFY(isOrdered(valuesOf(model, TransactionTableModel::ValueRole), Qt::AscendingOrder));
    QCOMPARE(model.data(model.index(0, 0), TransactionTableModel::ValueRole).toDouble(), 1.0);

    model.setAccountId(0);
    QCOMPARE(model.rowCount(), 0);

    model.setAccountId(firstAccount);
    QTRY_COMPARE_WITH_TIMEOUT(model.rowCount(), 12, workerTimeoutMs);

    // The order the view opens with: the youngest booking at the top.
    const auto dates = valuesOf(model, TransactionTableModel::DateRole);
    QVERIFY(isOrdered(dates, Qt::DescendingOrder));
    QCOMPARE(dates.first().toDate(), QDate(2026, 1, 12));

    storage.close();
}

/**
 * Paging through three thousand records delivers each of them once. Checked over
 * the set of identifiers and not over their number: a record that went missing
 * and one that came twice cancel each other out in a count.
 */
void TransactionTableModelTest::pagingDeliversEveryTransactionExactlyOnce()
{
    Storage storage(applicationInfo());
    QVERIFY(openStorage(storage));

    // All of them on one day. The chosen column cannot tell two records apart
    // then, so the order rests on the second criterion alone, and that is where
    // a page boundary loses one.
    QVERIFY(putOrderedTransactions(firstAccount, 3000, QStringLiteral("2026-01-01"), 0));

    TransactionTableModel model;
    model.setStorage(&storage);

    model.setAccountId(firstAccount);
    QTRY_COMPARE_WITH_TIMEOUT(model.totalRows(), 3000, workerTimeoutMs);

    loadEverything(model);

    QCOMPARE(model.rowCount(), 3000);
    QVERIFY(model.atEnd());

    auto expected = QSet<quint32>();
    for (quint32 identifier = 1; identifier <= 3000; ++identifier) {
        expected.insert(identifier);
    }

    QCOMPARE(identifiersOf(model), expected);

    storage.close();
}

/**
 * The order changes while the first read is still going. Its rows belong to the
 * order that was, and they must not turn up under the one that is.
 */
void TransactionTableModelTest::aChangeOfOrderDuringARunningRequestDiscardsItsResult()
{
    Storage storage(applicationInfo());
    QVERIFY(openStorage(storage));

    QVERIFY(putOrderedTransactions(firstAccount, 300));

    TransactionTableModel model;
    model.setStorage(&storage);

    QSignalSpy errorSpy(&storage, &Storage::errorOccurred);

    model.setAccountId(firstAccount);
    model.sort(TransactionTableModel::ValueColumn, Qt::AscendingOrder);

    QTRY_COMPARE_WITH_TIMEOUT(model.rowCount(), pageSize, workerTimeoutMs);

    QVERIFY(isOrdered(valuesOf(model, TransactionTableModel::ValueRole), Qt::AscendingOrder));
    QCOMPARE(model.data(model.index(0, 0), TransactionTableModel::ValueRole).toDouble(), 1.0);

    // Changing while a read is running is an everyday move, not a failure.
    QCOMPARE(errorSpy.count(), 0);

    storage.close();
}

void TransactionTableModelTest::aHoldingThatFitsOnePageEndsWithIt_data()
{
    QTest::addColumn<int>("count");

    // Exactly one full page. The loaded number reaches the counted one, and no
    // second request is needed to find that out.
    QTest::newRow("one full page") << pageSize;
    // Short of a page. This is what answers when the count did not come through
    // and the first way has no number to compare against.
    QTest::newRow("less than a page") << 40;
    // Nothing at all. The storage reports that as "nothing found", which ends
    // the paging and is no failure.
    QTest::newRow("nothing") << 0;
}

/**
 * The holding is through without an empty page behind it. Whichever of the three
 * ways gets there, the view is told that there is nothing left to ask for.
 */
void TransactionTableModelTest::aHoldingThatFitsOnePageEndsWithIt()
{
    QFETCH(int, count);

    Storage storage(applicationInfo());
    QVERIFY(openStorage(storage));

    if (count > 0) {
        QVERIFY(putOrderedTransactions(firstAccount, count));
    }

    TransactionTableModel model;
    model.setStorage(&storage);

    QSignalSpy finishedSpy(&storage, &Storage::finished);

    model.setAccountId(firstAccount);
    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, workerTimeoutMs);

    QCOMPARE(model.rowCount(), count);
    QCOMPARE(model.totalRows(), count);
    QVERIFY(model.atEnd());
    QVERIFY(!model.canFetchMore(QModelIndex()));

    storage.close();
}

/**
 * A failure while loading further rows leaves what stands and stops the asking.
 * Without the block the view would ask again at once, meet the same failure and
 * report it once more for as long as the cause lasts.
 *
 * The holding does not count as complete either. Told as an end, a part of it
 * would read as the whole.
 */
void TransactionTableModelTest::aFailureWhileLoadingMoreLeavesTheRowsAndStopsAsking()
{
    Storage storage(applicationInfo());
    QVERIFY(openStorage(storage));

    QVERIFY(putOrderedTransactions(firstAccount, 500));
    QVERIFY(putOrderedTransactions(secondAccount, 30));

    TransactionTableModel model;
    model.setStorage(&storage);

    // Ordered by amount, so the read names that column. Taking the column away
    // is what makes the next page fail and nothing else.
    model.setAccountId(firstAccount);
    model.sort(TransactionTableModel::ValueColumn, Qt::AscendingOrder);

    QTRY_COMPARE_WITH_TIMEOUT(model.rowCount(), pageSize, workerTimeoutMs);
    QCOMPARE(model.totalRows(), 500);
    QVERIFY(model.canFetchMore(QModelIndex()));

    QVERIFY(TestHelpers::runStatement(storageFile(),
                                      password(),
                                      QStringLiteral("ALTER TABLE transactions RENAME COLUMN "
                                                     "`value` TO amount;")));

    QSignalSpy finishedSpy(&storage, &Storage::finished);
    model.fetchMore(QModelIndex());
    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, workerTimeoutMs);

    QCOMPARE(model.rowCount(), pageSize);
    QVERIFY(!model.canFetchMore(QModelIndex()));

    // A failure is not an end. An implementation that reads every error as the
    // end of the holding would answer the line above just the same.
    QVERIFY(!model.atEnd());
    QVERIFY(model.rowCount() < model.totalRows());

    QVERIFY(TestHelpers::runStatement(storageFile(),
                                      password(),
                                      QStringLiteral("ALTER TABLE transactions RENAME COLUMN "
                                                     "amount TO `value`;")));

    // A change of account lifts the block, and so do a change of order and of
    // filter: all three start over.
    model.setAccountId(secondAccount);
    QTRY_COMPARE_WITH_TIMEOUT(model.rowCount(), 30, workerTimeoutMs);
    QCOMPARE(model.totalRows(), 30);
    QVERIFY(model.atEnd());

    storage.close();
}

} // namespace olbaflinx::ui::models::tests

QTEST_MAIN(olbaflinx::ui::models::tests::TransactionTableModelTest)

#include "tst_transactiontablemodel.moc"
