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
#ifndef OLBAFLINX_BANKING_H
#define OLBAFLINX_BANKING_H

#include <QtCore/QObject>
#include <Banking/Private/BankingPrivate.h>


#include "core/Singleton.h"
#include "core/Container.h"

namespace olbaflinx::core::banking
{

class BankingPrivate;
class Banking: public QObject, public Singleton<Banking>
{

Q_OBJECT
    friend class Singleton<Banking>;

public:
    ~Banking() override;

    [[nodiscard]] bool initialize(
        const QString &name,
        const QString &key,
        const QString &version
    ) const;
    [[nodiscard]] bool deInitialize() const;

    [[nodiscard]] bool createAccount() const;
    [[nodiscard]] Account *account(quint32 uniqueId) const;
    void receiveAccounts();
    void receiveAccountIds();

Q_SIGNALS:
    void accountsReceived(const AccountList &accountList);
    void accountIdsReceived(const AccountIds &accountIds);

private:
    QScopedPointer<BankingPrivate> const d_ptr;

private Q_SLOTS:

protected:
    Banking();
    Q_DISABLE_COPY(Banking)
};

}

#endif //OLBAFLINX_BANKING_H
