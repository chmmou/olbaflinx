/**
 * Copyright (C) 2021, Alexander Saal <developer@olbaflinx.chm-projects.de>
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

#ifndef OLBAFLINX_CONTAINER_H
#define OLBAFLINX_CONTAINER_H

#include <QtCore/QObject>
#include <QtCore/QList>
#include <QtCore/QRegularExpression>

#include "Storage/Account/Account.h"

/**
 * Password regular expression
 *
 * /^(?=.*[a-z])(?=.*[A-Z])(?=.*\d)(?=.*[!"§$%&\/()=?´`{}\[\]\\ß@€~’*'+#-_.:,;µöäüÖÄÜ<|>])[A-Za-z\d!"§$%&\/()=?´`{}\[\]\\ß@€~’*'+#-_.:,;µöäüÖÄÜ<|>]{6,}$/g
 *
 * At least one lower case English letter, a-z
 * At least one upper case English letter, A-Z
 * At least one lower case letter, öäü
 * At least one upper case letter, ÖÄÜ
 * At least one digit, 0-9
 * At least one of special character, !"§$%&/()=?´`{}[]\ß@€~’*'+#-_.:,;µöäüÖÄÜ<|>
 * Minimum six in length 6 (with the anchors)
 */
#define MinPasswordReqEx QRegularExpression("^(?=.*[a-z])(?=.*[A-Z])(?=.*\\d)(?=.*[!\"§$%&/()=?´`{}\\[\\]\\ß@€~’*'+#-_.:,;µöäüÖÄÜ<|>])[A-Za-z\\d!\"§$%&/()=?´`{}\\[\\]\\ß@€~’*'+#-_.:,;µöäüÖÄÜ<|>]{6,}$")

/**
 * Group & Key for settings
 */
#define StorageSettingGroup "Vaults"
#define StorageSettingGroupKey "Paths"

namespace olbaflinx::core
{

using namespace storage::account;

template<class T>
class SignalBlocker
{
    T *blocked;
    bool previous;

public:
    SignalBlocker(T *blocked)
        : blocked(blocked),
          previous(blocked->blockSignals(true))
    {}

    ~SignalBlocker()
    { blocked->blockSignals(previous); }

    T *operator->()
    { return blocked; }
};

template<class T>
inline SignalBlocker<T> whileSignalBlocking(T *blocked)
{ return SignalBlocker<T>(blocked); }

struct StorageUser: public QObject
{
public:
    void setPassword(const QString &password)
    { mPassword = password; }

    [[nodiscard]] QString password() const
    { return mPassword; }

    void setFilePath(const QString &filePath)
    { mFilePath = filePath; }

    [[nodiscard]] QString filePath() const
    { return mFilePath; }

private:
    QString mPassword;
    QString mFilePath;
};

typedef QList<const Account *> AccountList;

typedef QList<quint32> AccountIds;

}

Q_DECLARE_METATYPE(olbaflinx::core::AccountList)
Q_DECLARE_METATYPE(olbaflinx::core::AccountIds)
Q_DECLARE_METATYPE(olbaflinx::core::StorageUser *)
Q_DECLARE_METATYPE(const olbaflinx::core::StorageUser *)

#endif // OLBAFLINX_CONTAINER_H
