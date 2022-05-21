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
#include <QtCore/QRegularExpression>
#include <QtCore/QVector>
#include <QtGui/QImage>

#include "core/Constant.h"
#include "core/Storage/Account/Account.h"
#include "core/Storage/Account/AccountBalance.h"
#include "core/Storage/Transaction/Transaction.h"

namespace olbaflinx::core {
using namespace storage::account;
using namespace storage::transaction;

typedef QVector<const Account *> AccountList;
typedef QVector<const AccountBalance *> AccountBalanceList;

typedef QVector<const Transaction *> TransactionList;
typedef QVector<quint32> AccountIds;

template<class T> class SignalBlocker
{
    T *blocked;
    bool previous;

public:
    SignalBlocker(T *blocked)
        : blocked(blocked)
        , previous(blocked->blockSignals(true))
    { }

    ~SignalBlocker() { blocked->blockSignals(previous); }

    T *operator->() { return blocked; }
};

template<class T> inline SignalBlocker<T> whileSignalBlocking(T *blocked)
{
    return SignalBlocker<T>(blocked);
}

struct AccountItem
{
    QString title = "";
    QImage image = {};
    double balance = 0.0;
    quint32 id = 0;
};

struct ImExportProfileData
{
    QString name = "";
    QString longDescr = "";
    QString shortDescr = "";
    QString type = "";
    int import = 0;
    int exp = 0;
};
typedef QVector<const ImExportProfileData *> ImExportProfileDataList;

struct ImExportProfile
{
    QString name = "";
    QString type = "";
    QString shortDescr = "";
    QString author = "";
    QString version = "";
    QString longDescr = "";
    QString fileName = "";
    ImExportProfileDataList profiles = {};
};
typedef QVector<const ImExportProfile *> ImExportProfileList;

} // namespace olbaflinx::core

Q_DECLARE_METATYPE(olbaflinx::core::AccountList)
Q_DECLARE_METATYPE(olbaflinx::core::AccountBalanceList)

Q_DECLARE_METATYPE(olbaflinx::core::AccountIds)
Q_DECLARE_METATYPE(olbaflinx::core::AccountItem *)
Q_DECLARE_METATYPE(const olbaflinx::core::AccountItem *)
Q_DECLARE_METATYPE(olbaflinx::core::TransactionList)

Q_DECLARE_METATYPE(olbaflinx::core::ImExportProfileData *)
Q_DECLARE_METATYPE(const olbaflinx::core::ImExportProfileData *)

Q_DECLARE_METATYPE(olbaflinx::core::ImExportProfile *)
Q_DECLARE_METATYPE(const olbaflinx::core::ImExportProfile *)

Q_DECLARE_METATYPE(olbaflinx::core::ImExportProfileList)
Q_DECLARE_METATYPE(olbaflinx::core::ImExportProfileDataList)

#endif // OLBAFLINX_CONTAINER_H
