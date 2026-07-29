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
     * @brief Error enumeration
     */
    enum Error {
        /** Error on storing item */
        NoError = 0,
        /** Error on storing item */
        StoreItem = 100,
        /** Error on schema setup */
        SchemaSetup,
        /** Error on password changing */
        PasswordChanged,
        /** Unknown error */
        Unknown = 1000,
    };
    Q_ENUM(Error);

    /**
     * @brief Storage type enumeration
     */
    enum Type {
        StorageAccount = 1,
        StorageReferenceAccount,
        StorageTransaction,
        StorageCategories,
        StorageContacts,
    };
    Q_ENUM(Type);

    /**
     * @brief Set absolute path with file name
     *
     * @param storageFileName Storage file
     */
    void setStorageFile(const QString &storageFileName);

    /**
     * @brief Set the key for the storage file
     *
     * @param key Storage Key
     */
    void setKey(const QString &key);

    /**
     * @brief Change a storage key
     *
     * @param oldKey Old storage key
     * @param newKey New storage key
     *
     * @return If a error occurred false returned and the errorOccurred signal is emitted;
     *  otherwise true
     */
    [[nodiscard]] bool changeKey(const QString &oldKey, const QString &newKey);

    /**
     * @Brief Initializing the storage backend
     *
     * @param withSchema If we do not want to initialize the storage space with the database default
     *  schema, then set it to false; otherwise, it is safe to set it to true since we only initialize
     *  the database default schema once.
     *
     * @return If a error occurred false returned and the errorOccurred signal is emitted;
     *  otherwise true
     */
    [[nodiscard]] bool initialize(bool withSchema = false);

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
     * @brief Stores a banking item into the database.
     *
     * @param bankingItem A pointer to the BankingItem object to be stored.
     *                     The item must be valid to proceed.
     * @return True if the item is successfully stored; false if an error occurs during the operation.
     */
    bool storeItem(const BankingItem *bankingItem);

    /**
     * @brief Retrieves a list of items from a database based on the specified type, offset, and limit.
     *
     * @param type The type of storage item to retrieve, corresponding to a specific database table.
     * @param offset The starting point of the records to retrieve in the query.
     * @param limit The maximum number of records to retrieve in the query.
     */
    void receiveItems(Type type, int offset = 0, int limit = 50);

Q_SIGNALS:
    /**
     * @brief This signal is emitted if any error occurred
     *
     * @param errorCode @ref Error
     * @param reason Error message
     */
    void errorOccurred(Error errorCode, const QString &reason);

    /**
     * @brief This signal is emitted when we have received one or more entries.
     *
     * @param items The records that were read. The receiver takes them over.
     */
    void itemsReceived(const BankingItems &items);

    /**
     * @brief The signal that is emitted if any progress changed
     *
     * @param progress Progress value
     */
    void progressChanged(int progress);

    /**
     * @brief This signal is emitted when we have received any resources or an error has occurred.
     */
    void finished();

private:
    class Private;
    Private *d_ptr;

    Q_DISABLE_COPY(Storage)
};

} // namespace olbaflinx::core::storage
