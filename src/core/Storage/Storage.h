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
     * The three parameters receiveItems used to take grew to ten, four of them
     * integral. A swapped pair would have compiled.
     */
    struct ItemQuery
    {
        Type type = StorageAccount;

        // The identifier the institution assigns, transactions.unique_account_id.
        // Not the row id of the accounts table: neither Account nor Transaction
        // carries that one, so the caller could not name it. 0 means no filter,
        // and a value is only meaningful for StorageTransaction.
        quint32 accountId = 0;

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
     * The check used to live in the user interface alone, where a second caller
     * of the core could walk past it.
     *
     * @param key Storage Key
     *
     * @return An error if the key does not meet the guidelines.
     */
    Error setKey(const QString &key);

    /**
     * @brief Change a storage key
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
     *  caller has to check it, the return type is [[nodiscard]].
     */
    Error initialize(bool withSchema = false);

    /**
     * @brief Checks whether the storage has been initialized correctly and is ready for use.
     *
     * @return true on success; otherwise false.
     */
    [[nodiscard]] bool isValid();

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
     * responsive; a window of a thousand accounts used to hold it for as long as
     * the read took, and each account costs a second query for its balance and
     * its reference accounts.
     *
     * Every signal reaches the caller in the thread it called from. Nothing is
     * emitted from the worker.
     *
     * Wrong arguments are still answered before anything is started, in the
     * calling thread, because they are a programming error and not worth a
     * detour. A second call while a read is running is refused the same way.
     *
     * The order is unambiguous whatever is asked for: every read orders by the
     * row id as well. Without it a record could fall between two windows or
     * appear in both.
     *
     * @param query What to read. See ItemQuery.
     */
    void receiveItems(const ItemQuery &query);

    /**
     * @brief Stores a run of records without holding the calling thread.
     *
     * The call returns at once and the writing happens in a thread of its own,
     * on a second connection to the same file, the same way receiveItems reads.
     * storeItem stays what it is and keeps its immediate Error; a run of records
     * cannot report that way, which is why this is a second entry point rather
     * than a change to the first.
     *
     * The bracket sits around the single record, not around the run. What went
     * in before a failure stays in, the failing one does not, and the run ends
     * there rather than carrying on over a record that may be the cause.
     *
     * itemsStored reports how many records were written, on every path.
     * errorOccurred names the failure, finished ends the run either way. All of
     * them reach the caller in the thread it called from.
     *
     * @param items The records to store. An empty run is not an error.
     */
    void storeItems(const BankingItems &items);

Q_SIGNALS:
    /**
     * @brief This signal is emitted if an error occurred on an asynchronous path.
     *
     * Synchronous calls report through their return value instead.
     *
     * @param errorCode @ref olbaflinx::core::ErrorCode
     * @param reason Technical message, meant for the log. The presentation
     *  layer decides what the user gets to see.
     */
    void errorOccurred(olbaflinx::core::ErrorCode errorCode, const QString &reason);

    /**
     * @brief This signal is emitted when we have received one or more entries.
     *
     * @param items The records that were read. The receiver takes them over.
     */
    void itemsReceived(const BankingItems &items);

    /**
     * @brief This signal is emitted once per read that reached the table.
     *
     * It arrives before itemsReceived and before errorOccurred, so that whoever
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
     * It arrives on every path, after a failure as well. Whoever tells the user
     * what happened needs the count in both cases, and progressChanged cannot
     * carry it: QFutureWatcher limits the rate of its progress reports, so a
     * receiver is not told every value.
     *
     * @param count The number of records that reached the storage.
     */
    void itemsStored(int count);

    /**
     * @brief The signal that is emitted if any progress changed
     *
     * @param progress Progress value in percent, from 0 to 100.
     */
    void progressChanged(int progress);

    /**
     * @brief This signal is emitted when we have received any resources or an error has occurred.
     */
    void finished();

private:
    class Private;
    Private *d_ptr = nullptr;

    Q_DISABLE_COPY(Storage)
};

} // namespace olbaflinx::core::storage
