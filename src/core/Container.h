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

#include "Storage/Account/Account.h"

using namespace olbaflinx::core::storage::account;

namespace olbaflinx::core
{

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

#endif // OLBAFLINX_CONTAINER_H
