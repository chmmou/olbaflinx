/**
 * Copyright (c) 2021, Alexander Saal <alexander.saal@chm-projects.de>
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

#include <iostream>
#include <sstream>

#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QPointer>
#include <QtCore/QStringList>
#include <QtCore/QSettings>
#include <QtCore/QStandardPaths>
#include <QtCore/QTextStream>

#include <QtSql/QSqlDriver>
#include <QtSql/QSqlError>
#include <QtSql/QSqlField>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlRecord>
#include <QtSql/QSqlResult>

#include "core/SingleApplication/SingleApplication.h"

#include "Storage.h"

using namespace olbaflinx::core::storage;

QSettings *mSettings;

QPointer<StorageUser> mStorageUser;

StorageConnection *mStorageConnection;

bool mIsUserChanged;

Storage::Storage()
    : QObject(nullptr)
{

    mSettings = new QSettings(
        QSettings::IniFormat,
        QSettings::UserScope,
        SingleApplication::organizationName(),
        SingleApplication::applicationName(),
        this
    );

    mStorageUser = nullptr;
    mStorageConnection = nullptr;
    mIsUserChanged = false;
}

Storage::~Storage()
{
    if (!mStorageUser.isNull()) {
        mStorageUser.clear();
        delete mStorageUser;
        mStorageUser = nullptr;
    }

    if (mStorageConnection != nullptr) {
        mStorageConnection->close();
        mStorageConnection->deleteLater();
    }

    if (mSettings != nullptr) {
        mSettings->sync();
        mSettings->deleteLater();
    }

    mIsUserChanged = false;
}

void Storage::setUser(const StorageUser *storageUser)
{
    if (storageUser == nullptr) {
        Q_EMIT errorOccurred(tr("Given user object can't be null!"), Storage::UnknownError);
        return;
    }

    if (storageUser->filePath.isEmpty()) {
        Q_EMIT errorOccurred(tr("Storage file can't be empty!"), Storage::FileNotFoundError);
        return;
    }

    if (storageUser->password.isEmpty()) {
        Q_EMIT errorOccurred(tr("Storage password can't be empty!"), Storage::PasswordError);
        return;
    }

    mIsUserChanged = false;

    if (!mStorageUser.isNull()) {
        mIsUserChanged = (
            mStorageUser->filePath != storageUser->filePath && mStorageUser->password != storageUser->password
        );
        mStorageUser.clear();
        delete mStorageUser;
        mStorageUser = nullptr;
    }

    QPointer<StorageUser> storageUserPtr(const_cast<StorageUser *>(storageUser));
    storageUserPtr.swap(mStorageUser);
    storageUserPtr.clear();

    delete storageUserPtr;
}

bool Storage::initialize()
{
    const auto conn = connection();
    if (!conn->isOpen()) {
        Q_EMIT errorOccurred(conn->lastErrorMessage(), Storage::ConnectionError);
        return false;
    }

    QFile storageFile(":/app/olbaflinx-storage");
    if (!storageFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        Q_EMIT errorOccurred(storageFile.errorString(), Storage::StorageNotOpen);
        return false;
    }

    QStringList sqlStatements = QTextStream(&storageFile).readAll().split(';');

    bool ok = setupTables(conn, sqlStatements);

    sqlStatements.clear();

    return ok;
}

bool Storage::isInitialized()
{
    QFile storageFile(mStorageUser->filePath);
    if (storageFile.size() == 0 || !storageFile.exists()) {
        return false;
    }

    const auto conn = connection();

    QStringList queries;
    queries << pragmaKey();
    queries << "SELECT migrated FROM migrations;";

    QSqlQuery dbQuery(conn->database());

    QStringList tables;
    bool success = true;
    for (const auto &query: qAsConst(queries)) {
        if (query.trimmed().isEmpty()) {
            continue;
        }

        success &= dbQuery.exec(query);
        if (query.contains("migrations")) {
            while (dbQuery.next()) {
                success &= dbQuery.value(0).toBool();
            }

        }
        dbQuery.finish();
    }

    return success;
}

bool Storage::checkIntegrity()
{
    const auto conn = connection();
    QSqlQuery dbQuery(conn->database());

    bool success = dbQuery.exec(pragmaKey());

    success &= dbQuery.exec("PRAGMA integrity_check;");
    if (!success) {
        Q_EMIT errorOccurred(dbQuery.lastError().text(), Storage::IntegrityError);
        return false;
    }

    success &= dbQuery.exec("PRAGMA foreign_key_check;");
    if (!success) {
        Q_EMIT errorOccurred(dbQuery.lastError().text(), Storage::ForeignKeyError);
        return false;
    }

    return success;
}

bool Storage::compress()
{
    const auto conn = connection();
    QSqlQuery dbQuery(conn->database());

    bool success = dbQuery.exec(pragmaKey());
    success &= dbQuery.exec("VACUUM;");

    if (!success) {
        Q_EMIT errorOccurred(dbQuery.lastError().text(), Storage::IntegrityError);
        return false;
    }

    return success;
}

bool Storage::changePassword(const QString &newPassword)
{
    const auto conn = connection();
    bool ok = checkPassword(conn, mStorageUser->password);

    if (!ok) {
        Q_EMIT errorOccurred(tr("The entered password is not correct!"), Storage::PasswordError);
        return false;
    }

    if (mStorageUser->password == newPassword) {
        Q_EMIT errorOccurred(tr("The new password cannot be the same as the old one."), Storage::PasswordError);
        return false;
    }

    // The old password is valid

    return false;
}

void Storage::storeSetting(const QString &group, const QString &key, const QVariant &value)
{
    if (!group.isEmpty()) {
        mSettings->beginGroup(group);
    }

    mSettings->setValue(key, value);

    if (!group.isEmpty()) {
        mSettings->endGroup();
    }

    mSettings->sync();
}

QVariant Storage::setting(const QString &group, const QString &key, const QVariant &defaultValue) const
{
    if (!group.isEmpty()) {
        mSettings->beginGroup(group);
    }

    QVariant value = mSettings->value(key, defaultValue);

    if (!group.isEmpty()) {
        mSettings->endGroup();
    }

    return value;
}

StorageConnection *Storage::connection()
{
    if (mIsUserChanged) {
        if (mStorageConnection != nullptr) {
            mStorageConnection->close();
            delete mStorageConnection;
            mStorageConnection = nullptr;
        }

        mStorageConnection = new StorageConnection(mStorageUser->filePath);
        return mStorageConnection;
    }

    if (mStorageConnection == nullptr) {
        mStorageConnection = new StorageConnection(mStorageUser->filePath);
    }

    return mStorageConnection;
}

const QString Storage::pragmaKey() const
{
    return QString("PRAGMA key='%1';").arg(quote(mStorageUser->password));
}

const QString Storage::quote(const QString &string) const
{
    QString result = {};

    const int stringLength = string.length();
    for (int a = 0; a < stringLength; ++a) {
        const QChar strPart = string.at(a);
        const int accii = (int) strPart.toLatin1();

        if (strPart != "'" && strPart != "\"" && strPart != '\\' && accii >= 32 && accii <= 126) {
            result += strPart;
        }
        else {
            std::stringstream ss;
            ss << std::hex << std::uppercase << accii;

            result += "\\x" + QString::fromStdString(ss.str());
        }
    }

    return result;
}

bool Storage::setupTables(StorageConnection *connection, QStringList &sqlStatements) const
{
    QStringList queries = defaultQueries();

    for (auto &query: sqlStatements) {
        queries << query.replace("#", ";").trimmed();
    }
    queries << "VACUUM;";

    QSqlQuery dbQuery(connection->database());

    bool success = true;
    for (const auto &query: qAsConst(queries)) {
        if (query.trimmed().isEmpty()) {
            continue;
        }

        success &= dbQuery.exec(query);
        dbQuery.finish();
    }

    return success;
}

const QStringList Storage::defaultQueries() const
{
    QStringList queries;
    queries << pragmaKey();
    queries << "PRAGMA auto_vacuum = FULL;";
    queries << "PRAGMA foreign_keys = ON;";
    queries << "PRAGMA encoding = 'UTF-8';";

    return queries;
}

const bool Storage::checkPassword(StorageConnection *connection, const QString &password) const
{
    QSqlQuery dbQuery(connection->database());

    bool success = dbQuery.exec(pragmaKey());

    // A select statement is not possible if we have a wrong password.
    success &= dbQuery.exec("SELECT COUNT(id) AS ID_COUNT FROM migrations;");

    while (dbQuery.next()) {
        const int count = dbQuery.value("ID_COUNT").toInt();
        success &= (count > 0);
    }

    return success;
}
