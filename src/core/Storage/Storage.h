/**
 * Copyright (C) 2022-2026, Alexander Saal <developer@olbaflinx.chm-projects.de>
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

#include "core/OlbaFlinxCore.h"

#include "core/ApplicationInfo.h"
#include "core/Banking/BankingItem.h"
#include "core/Error.h"
#include "core/Result.h"

#include <QtCore/QDate>
#include <QtCore/QObject>
#include <QtCore/QRegularExpression>
#include <QtCore/QVariant>

using namespace olbaflinx::core::banking;

namespace olbaflinx::core::storage {

/**
 * The encrypted storage of the application, settings included.
 *
 * Ownership: the creator owns the instance. If a parent is set, the parent
 * releases it, otherwise the enclosing scope does. The records reported through
 * itemsReceived pass into the ownership of the receiver; Storage does not hold
 * them afterwards.
 */
class OLBAFLINX_CORE_EXPORT Storage : public QObject
{
    Q_OBJECT

public:
    /**
     * The application info decides where the settings and the storage file
     * live.
     */
    explicit Storage(ApplicationInfo applicationInfo, QObject *parent = nullptr);
    ~Storage() override;

    enum Type : int {
        StorageAccount = 1,
        StorageReferenceAccount,
        StorageTransaction,
        StorageCategories,
        StorageContacts,
    };
    Q_ENUM(Type);

    /**
     * The widest window a single read may open.
     *
     * Without a bound a caller could ask for INT_MAX rows and hold a whole table
     * in memory at once. A read that names a wider one is refused.
     *
     * Public because a caller that shows a whole holding at once, the account
     * tree among them, has to know where that stops rather than guess a number
     * of its own.
     */
    static constexpr int MaxItemsPerQuery = 1000;

    /**
     * The column a read may order by.
     *
     * An enumeration rather than a string: SQL binds no identifier, so the column
     * name reaches the statement by interpolation. A closed set cannot carry a
     * value that is not in the mapping.
     *
     * One value per column the view offers. The view has four; None covers the
     * unordered read. A column nobody can click on has no value here.
     */
    enum class SortColumn : int {
        None = 0,
        Date,
        Value,
        RemoteName,
        Purpose,
    };
    Q_ENUM(SortColumn)

    /**
     * Which way a booking goes.
     *
     * The value of a transaction carries the sign, so a booking of nought is
     * neither of the two and no restriction lets it through.
     */
    enum class Direction { Any, Incoming, Outgoing };
    Q_ENUM(Direction)

    /**
     * What a single read asks for.
     *
     * Ten parameters, four of them integral: named rather than positional,
     * because a swapped pair would compile.
     */
    struct ItemQuery
    {
        Type type = StorageAccount;

        // The identifier the institution assigns, transactions.unique_account_id.
        // Not the row id of the accounts table: neither Account nor Transaction
        // carries that one, so the caller could not name it. 0 means no filter,
        // and a value is only meaningful for StorageTransaction.
        quint32 accountId = 0;

        // Every value of the enumeration names a column of the transactions
        // table, so a chosen column is meaningful for StorageTransaction alone
        // and is refused on any other type. None leaves the ordering to the row
        // id and is the one value every type carries.
        SortColumn sort = SortColumn::None;
        Qt::SortOrder order = Qt::AscendingOrder;
        int offset = 0;
        int limit = 50;

        // The filter above the transaction list. Every field is optional; an
        // empty text and an invalid date leave that condition out of the
        // statement. Like accountId, all four are meaningful for
        // StorageTransaction alone.
        //
        // The text is looked for in the name of the other party and in the
        // purpose. It is bound rather than written into the statement, and its
        // own wildcards are escaped, so a percent sign is searched for as a
        // character.
        QString text = {};
        QDate from = {};
        QDate to = {};
        Direction direction = Direction::Any;
    };

    /**
     * The absolute path including the file name.
     */
    void setStorageFile(const QString &storageFileName);

    /**
     * The key is checked against minPasswordGuidelines before it is kept. A key
     * that does not meet them is refused and the storage keeps the one it had.
     * The check lives here rather than in the user interface alone, where a
     * second caller of the core could walk past it.
     */
    Error setKey(const QString &key);

    /**
     * Both keys stand under the same length check setKey applies. A new key
     * below the lower bound would rekey the file to something the public
     * interface refuses afterwards, which puts the holding out of reach.
     */
    Error changeKey(const QString &oldKey, const QString &newKey);

    /**
     * The schema is written once. A later call passes false and finds the
     * schema that is already there.
     *
     * Success means an open connection: a connection that already stands on the
     * same file is asked whether it is open rather than taken for one.
     */
    Error initialize(bool withSchema = false);

    /**
     * Reads from the file to tell whether the storage is sound, where isOpen
     * asks the connection alone.
     */
    [[nodiscard]] bool isValid();

    /**
     * Whether a connection to the storage file stands.
     *
     * Answers from the connection alone and reads nothing, which is the
     * difference from isValid: it says whether a call has any prospect of being
     * answered, not whether the file behind it is sound. A caller that reaches
     * the storage after it was closed uses this to tell that apart from a
     * failure worth showing to the user.
     */
    [[nodiscard]] bool isOpen() const;

    /**
     * The directory the settings and the storage files of this user live in.
     */
    [[nodiscard]] QString storagePath() const;

    void close();

    /**
     * A group of its own keeps a key apart from the same key elsewhere; without
     * one the setting lands in the top level.
     */
    void storeSetting(const QString &key, const QVariant &value, const QString &group = QString());

    /**
     * Answers with the default where neither the key nor the group is held.
     */
    [[nodiscard]] QVariant setting(const QString &key,
                                   const QString &group = QString(),
                                   const QVariant &defaultValue = QVariant()) const;

    /**
     * The pattern a pass phrase has to match before setKey keeps it.
     */
    [[nodiscard]] QRegularExpression minPasswordGuidelines() const;

    /**
     * The smallest length minPasswordGuidelines accepts, in characters.
     *
     * The guideline carries the number inside its pattern, where a caller
     * cannot read it without taking the pattern apart. Whoever has to name the
     * rule to the user would otherwise write the number down a second time and
     * promise something else than the core asks for.
     */
    [[nodiscard]] int minPasswordLength() const;

    /**
     * The item has to be valid. An invalid or unsupported one is a failure, not
     * a silent no-op.
     */
    Error storeItem(const BankingItem *bankingItem);

    /**
     * Reads one window of records and returns at once; the reading happens in a
     * thread of its own, on a second connection to the same file.
     *
     * Every signal reaches the caller in the thread it called from. Nothing is
     * emitted from the worker.
     *
     * A call that starts no run answers through the return value and emits
     * nothing at all: neither readFailed nor readFinished is to be expected.
     * Wrong arguments end here, and so does a second call while a read is
     * running.
     *
     * Every read orders by the row id as well, so that no record falls between
     * two windows or appears in both.
     */
    [[nodiscard]] Error receiveItems(const ItemQuery &query);

    /**
     * The day the stored holding of one account ends on, named by the
     * identifier the institution assigns. The lead time of a fetch belongs to
     * the order and is not subtracted here.
     *
     * The booking date decides, and a booking that carries none counts through
     * its valuta date: the first is optional in the format a bank delivers, the
     * second is not.
     *
     * One row is read rather than the holding. The call does not go through
     * receiveItems, so it neither counts the records nor emits a signal, and a
     * view that is reading at the same time is not in its way.
     *
     * An account without a single stored booking answers with an invalid date
     * and is no failure; a failure is what keeps the read from running.
     */
    [[nodiscard]] Result<QDate> latestTransactionDate(quint32 uniqueAccountId);

    /**
     * Stores a run of records and returns at once; the writing happens in a
     * thread of its own, the same way receiveItems reads. storeItem keeps its
     * immediate Error and is the way to store a single record.
     *
     * The bracket sits around the run: a run that fails at one record leaves no
     * row of that run behind. The one exception is a run that carries an
     * account, where each account is bracketed for itself, because SQLite does
     * not nest transactions.
     *
     * A balance is stored through here without its account being written again.
     * It names the account by the identifier the institution assigns, and that
     * account has to be stored already.
     *
     * itemsStored reports how many rows were added. A booking that is already
     * there adds none and is no failure, and a run that was rolled back reports
     * nought. It stays out where the number would say nothing, on a run whose
     * storage was closed while it went. writeFailed names the failure,
     * writeFinished ends the run on every path. All of them reach the caller in
     * the thread it called from.
     *
     * A call that starts no run answers through the return value and emits
     * nothing. A second run while one is going ends there, so a fetch stores
     * its bookings first and its balance after the end of that run.
     *
     * An empty run is no error. It is reported as a run of nought records, and
     * its two signals go out through the event loop, so a caller that connects
     * after the call still receives them.
     */
    [[nodiscard]] Error storeItems(const BankingItems &items);

Q_SIGNALS:
    /**
     * A run of receiveItems failed. The reason is a technical message meant for
     * the log; the presentation layer decides what the user gets to see.
     *
     * A read and a write may be going at the same time, which is why each has a
     * failure signal of its own. A receiver that takes the failure of the other
     * one for its own ends a run that is still going.
     *
     * Synchronous calls report through their return value instead. Nothing on
     * setKey, changeKey, initialize or storeItem reaches a signal.
     */
    void readFailed(olbaflinx::core::ErrorCode errorCode, const QString &reason);

    /**
     * The counterpart of readFailed for a run of storeItems. See there.
     */
    void writeFailed(olbaflinx::core::ErrorCode errorCode, const QString &reason);

    /**
     * The records that were read. The receiver takes them over.
     */
    void itemsReceived(const BankingItems &items);

    /**
     * How many records match the condition of the query, the whole holding
     * rather than the window, and zero when none match.
     *
     * It arrives before itemsReceived and before readFailed, so that whoever
     * shows the number already holds it when the empty result is handled. A run
     * that fails before the query does not report it at all.
     */
    void itemsCounted(int count);

    /**
     * The number of rows a run of storeItems added. Not the number of records
     * it was handed: a booking that is already stored adds none, and a run that
     * failed and was rolled back reports nought.
     *
     * It arrives on every path of a run that started, after a failure as well.
     * writeProgressChanged cannot carry the count instead: QFutureWatcher
     * limits the rate of its progress reports, so a receiver is not told every
     * value.
     */
    void itemsStored(int count);

    /**
     * How far the running read has got, in percent from 0 to 100.
     */
    void readProgressChanged(int progress);

    /**
     * How far the running write has got, in percent from 0 to 100.
     */
    void writeProgressChanged(int progress);

    /**
     * A run of receiveItems has ended. It arrives on every path, after a
     * failure as well, and it is what frees the storage for the next read.
     *
     * A read and a write stand under watchers of their own and do not lock
     * against each other. Each therefore ends with a signal of its own, so that
     * a receiver waiting for one of them is not answered by the other.
     */
    void readFinished();

    /**
     * The counterpart of readFinished for a run of storeItems. See there.
     */
    void writeFinished();

private:
    class Private;
    Private *d_ptr = nullptr;

    Q_DISABLE_COPY(Storage)
};

} // namespace olbaflinx::core::storage
