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
#ifndef OLBAFLINX_BANKINGPRIVATE_H
#define OLBAFLINX_BANKINGPRIVATE_H

#include <QtCore/QObject>

#include "Container.h"

namespace olbaflinx::core::banking
{

class Banking;
class BankingPrivate: public QObject
{
    Q_DISABLE_COPY(BankingPrivate)
    Q_DECLARE_PUBLIC(Banking)

Q_OBJECT

public:
    explicit BankingPrivate(Banking *banking);
    ~BankingPrivate() override;

    bool initializeAqBanking(const QString &name, const QString &key, const QString &version);
    bool finalizeAqBanking();

    [[nodiscard]] Account *account(quint32 uniqueId) const;
    void receiveAccounts();
    void receiveAccountIds();

private:
    Banking *const q_ptr;
    bool mIsInitialized;

};

}


#endif // OLBAFLINX_BANKINGPRIVATE_H
