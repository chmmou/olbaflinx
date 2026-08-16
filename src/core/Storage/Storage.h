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
 * @brief The encrypted storage of the application, settings included.
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
     * @param applicationInfo Details for the settings and the storage path.
     * @param parent Optional owner.
     */
    explicit Storage(ApplicationInfo applicationInfo, QObject *parent = nullptr);
    ~Storage() override;

    /**
     * @brief Storage type enumeration
     */
    enum Type : int {
        StorageAccount = 1,
        StorageReferenceAccount,
        StorageTransaction,
        StorageCategories,
        StorageContacts,
    };
    Q_ENUM(Type);

    /**
     * @brief The widest window a single read may open.
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
     * @brief The column a read may order by.
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
     * @brief Which way a booking goes.
     *
     * The value of a transaction carries the sign, so a booking of nought is
     * neither of the two and no restriction lets it through.
     */
    enum class Direction { Any, Incoming, Outgoing };
    Q_ENUM(Direction)

    /**
     * @brief What a single read asks for.
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
     * @brief Set absolute path with file name
     *
     * @param storageFileName Storage file
     */
    void setStorageFile(const QString &storageFileName);

    /**
     * @brief Set the key for the storage file
     *
     * The key is checked against minPasswordGuidelines before it is kept. A key
     * that does not meet them is refused and the storage keeps the one it had.
     * The check lives here rather than in the user interface alone, where a
     * second caller of the core could walk past it.
     *
     * @param key Storage Key
     *
     * @return An error if the key does not meet the guidelines.
     */
    Error setKey(const QString &key);

    /**
     * @brief Change a storage key
     *
     * Both keys stand under the same length check setKey applies. A new key
     * below the lower bound would rekey the file to something the public
     * interface refuses afterwards, which puts the holding out of reach.
     *
     * @param oldKey Old storage key
     * @param newKey New storage key
     *
     * @return A default constructed Error on success, otherwise the reason. The
     *  caller has to check it, the return type is [[nodiscard]].
     */
    Error changeKey(const QString &oldKey, const QString &newKey);

    /**
     * @Brief Initializing the storage backend
     *
     * @param withSchema If we do not want to initialize the storage space with the database default
     *  schema, then set it to false; otherwise, it is safe to set it to true since we only initialize
     *  the database default schema once.
     *
     * @return A default constructed Error on success, otherwise the reason. The
     *  caller has to check it, the return type is [[nodiscard]]. Success means
     *  an open connection: a connection that already stands on the same file is
     *  asked whether it is open rather than taken for one.
     */
    Error initialize(bool withSchema = false);

    /**
     * @brief Checks whether the storage has been initialized correctly and is ready for use.
     *
     * @return true on success; otherwise false.
     */
    [[nodiscard]] bool isValid();

    /**
     * @brief Whether a connection to the storage file stands.
     *
     * Answers from the connection alone and reads nothing, which is the
     * difference from isValid: it says whether a call has any prospect of being
     * answered, not whether the file behind it is sound. A caller that reaches
     * the storage after it was closed uses this to tell that apart from a
     * failure worth showing to the user.
     */
    [[nodiscard]] bool isOpen() const;

    /**
     * @brief Get a user storage configuration path
     *
     * @return User storage configuration path
     */
    [[nodiscard]] QString storagePath() const;

    /**
     * @brief Close the storage backend and free all associated resources
     */
    void close();

    /**
     * @brief Store every setting as kay & value and / or group
     *
     * @param key Setting key
     * @param value Setting value for the associated key
     * @param group Optional setting group
     */
    void storeSetting(const QString &key, const QVariant &value, const QString &group = QString());

    /**
     * @brief Get setting from a key
     *
     * @param key Settings key
     * @param group Optional setting group
     * @param defaultValue Optional default value
     *
     * @return If no setting founds for the key and / or group te default value returned;
     *  otherwise the associated setting for the key and or group
     */
    [[nodiscard]] QVariant setting(const QString &key,
                                   const QString &group = QString(),
                                   const QVariant &defaultValue = QVariant()) const;

    /**
     * @brief Gets the min. password guidelines.
     *
     * @return QRegularExpression with minimum password guidelines
     */
    [[nodiscard]] QRegularExpression minPasswordGuidelines() const;

    /**
     * @brief Gets the smallest length minPasswordGuidelines accepts.
     *
     * The guideline carries the number inside its pattern, where a caller
     * cannot read it without taking the pattern apart. Whoever has to name the
     * rule to the user would otherwise write the number down a second time,
     * which is how the dialog came to promise six where the core asks for
     * twelve.
     *
     * @return Minimum length of a pass phrase in characters
     */
    [[nodiscard]] int minPasswordLength() const;

    /**
     * @brief Stores a banking item into the database.
     *
     * @param bankingItem A pointer to the BankingItem object to be stored.
     *                     The item must be valid to proceed.
     * @return A default constructed Error on success, otherwise the reason. An
     *  invalid or unsupported item is a failure, not a silent no-op.
     */
    Error storeItem(const BankingItem *bankingItem);

    /**
     * @brief Reads one window of records from the storage.
     *
     * The call returns at once and the reading happens in a thread of its own,
     * on a second connection to the same file. The calling thread stays
     * responsive: a window of a thousand accounts would otherwise hold it for
     * the length of the read, and each account costs a second query for its
     * balance and its reference accounts.
     *
     * Every signal reaches the caller in the thread it called from. Nothing is
     * emitted from the worker.
     *
     * A call that starts no run answers through the return value and emits
     * nothing. That is the only way to tell the caller apart from whoever is
     * waiting for the read that is already going: the signals belong to a run,
     * and a run that was never started has none to give. Wrong arguments end
     * here, and so does a second call while a read is running.
     *
     * The order is unambiguous whatever is asked for: every read orders by the
     * row id as well. Without it a record could fall between two windows or
     * appear in both.
     *
     * @param query What to read. See ItemQuery.
     *
     * @return A default constructed Error once the run is under way, otherwise
     *  the reason it was not started. Nothing was emitted in that case, and
     *  neither readFailed nor readFinished is to be expected.
     */
    [[nodiscard]] Error receiveItems(const ItemQuery &query);

    /**
     * @brief The day the stored holding of one account ends on.
     *
     * What a fetch builds its starting point from. The lead time belongs to the
     * order and is not subtracted here, so what comes back is the date that
     * stands in the row and nothing else.
     *
     * The booking date decides. A booking that carries none counts through its
     * valuta date: the first is optional in the format a bank delivers, the
     * second is not, and a read over the booking date alone would look past
     * such a row.
     *
     * One row is read, not the holding. The call does not go through
     * receiveItems, so it neither counts the records nor emits a signal, and a
     * view that is reading at the same time is not in its way.
     *
     * @param uniqueAccountId The account, as the institution assigns it.
     *
     * @return The date, or an invalid one for an account without a single
     *  stored booking. An account nobody has fetched yet is not a failure; a
     *  failure is what keeps the read from running.
     */
    [[nodiscard]] Result<QDate> latestTransactionDate(quint32 uniqueAccountId);

    /**
     * @brief Stores a run of records without holding the calling thread.
     *
     * The call returns at once and the writing happens in a thread of its own,
     * on a second connection to the same file, the same way receiveItems reads.
     * storeItem stays what it is and keeps its immediate Error; a run of records
     * cannot report that way, which is why this is a second entry point rather
     * than a change to the first.
     *
     * The bracket sits around the run. A run that fails at one record leaves no
     * row of that run behind, so a fetch is either stored whole or not at all;
     * half a holding would move the starting point of the next fetch past
     * bookings that nobody holds. The one exception is a run that carries an
     * account: the account path brackets each account for itself and SQLite does
     * not nest transactions, so that guarantee stays the one it already had.
     *
     * A balance is stored through here as well, without the account it belongs
     * to being written again. It names its account by the identifier the
     * institution assigns, and the storage translates that into the row it keys
     * on. The account it names has to be stored already.
     *
     * itemsStored reports how many rows were added. A booking that is already
     * there adds none and is no failure, and a run that was rolled back reports
     * nought. It stays out on the one path where the number would say nothing:
     * a run whose storage was closed while it went speaks of a file that is no
     * longer open. writeFailed names the failure, writeFinished ends the run on
     * every path. All of them reach the caller in the thread it called from.
     *
     * A call that starts no run answers through the return value and emits
     * nothing before it returns, the way receiveItems does and for the same
     * reason. A second run while one is going ends there: a fetch therefore
     * stores its bookings first and its balance after the end of that run.
     *
     * @param items The records to store. An empty run is not an error and is
     *  reported as a run of nought records rather than refused. Its two signals
     *  go out through the event loop, so a caller that connects after the call
     *  receives them.
     *
     * @return A default constructed Error once the run is under way, otherwise
     *  the reason it was not started.
     */
    [[nodiscard]] Error storeItems(const BankingItems &items);

Q_SIGNALS:
    /**
     * @brief This signal is emitted when a run of receiveItems failed.
     *
     * Told apart from the write by the signal and not by a flag the receiver
     * keeps: a read and a write may be going at the same time, and a receiver
     * that takes the failure of the other one for its own ends a run that is
     * still going.
     *
     * Synchronous calls report through their return value instead. Nothing on
     * setKey, changeKey, initialize or storeItem reaches a signal.
     *
     * @param errorCode @ref olbaflinx::core::ErrorCode
     * @param reason Technical message, meant for the log. The presentation
     *  layer decides what the user gets to see.
     */
    void readFailed(olbaflinx::core::ErrorCode errorCode, const QString &reason);

    /**
     * @brief This signal is emitted when a run of storeItems failed.
     *
     * The counterpart of readFailed. See there.
     *
     * @param errorCode @ref olbaflinx::core::ErrorCode
     * @param reason Technical message, meant for the log.
     */
    void writeFailed(olbaflinx::core::ErrorCode errorCode, const QString &reason);

    /**
     * @brief This signal is emitted when we have received one or more entries.
     *
     * @param items The records that were read. The receiver takes them over.
     */
    void itemsReceived(const BankingItems &items);

    /**
     * @brief This signal is emitted once per read that reached the table.
     *
     * It arrives before itemsReceived and before readFailed, so that whoever
     * shows the number already holds it when the empty result is handled. A run
     * that fails before the query does not report it at all.
     *
     * The number cannot come from the window: fifty rows say nothing about three
     * thousand. It travels with the read rather than through a call of its own,
     * because a second entry point would need a second worker and would meet the
     * same refusal a second read meets today.
     *
     * @param count How many records match the condition of the query, the whole
     *  holding rather than the window. Zero when none match.
     */
    void itemsCounted(int count);

    /**
     * @brief This signal is emitted when a run of storeItems has ended.
     *
     * It arrives on every path of a run that started, after a failure as well.
     * Whoever tells the user what happened needs the count in both cases, and
     * writeProgressChanged cannot carry it: QFutureWatcher limits the rate of
     * its progress reports, so a receiver is not told every value.
     *
     * @param count The number of rows the run added. Not the number of records
     *  it was handed: a booking that is already stored adds none. A run that
     *  failed and was rolled back reports nought.
     */
    void itemsStored(int count);

    /**
     * @brief How far the running read has got.
     *
     * @param progress Progress value in percent, from 0 to 100.
     */
    void readProgressChanged(int progress);

    /**
     * @brief How far the running write has got.
     *
     * @param progress Progress value in percent, from 0 to 100.
     */
    void writeProgressChanged(int progress);

    /**
     * @brief This signal is emitted when a run of receiveItems has ended.
     *
     * It arrives on every path, after a failure as well, and it is what frees
     * the storage for the next read.
     *
     * A read and a write may be going at the same time; they stand under
     * watchers of their own and do not lock against each other. Each therefore
     * ends with a signal of its own, so that a receiver waiting for one of them
     * is not answered by the other.
     */
    void readFinished();

    /**
     * @brief This signal is emitted when a run of storeItems has ended.
     *
     * The counterpart of readFinished. See there.
     */
    void writeFinished();

private:
    class Private;
    Private *d_ptr = nullptr;

    Q_DISABLE_COPY(Storage)
};

} // namespace olbaflinx::core::storage
