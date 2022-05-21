/**
* Copyright (c) 2022, Alexander Saal <alexander.saal@chm-projects.de>
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

#ifndef OLBAFLINX_ONLINEBANKING_H
#define OLBAFLINX_ONLINEBANKING_H

#include <QtCore/QDate>
#include <QtCore/QObject>
#include <QtCore/QScopedPointer>

#include "core/Container.h"
#include "core/Singleton.h"

namespace olbaflinx::core::banking {

using namespace olbaflinx::core;

class OnlineBanking : public QObject, public Singleton<OnlineBanking>
{
    Q_OBJECT
    friend class Singleton<OnlineBanking>;

public:
    enum TransactionListType {
        BalanceType = 1,
        TransactionsType,
        StandingOrdersType,
        DatedTransfersType,
        SEPAStandingOrdersType,
        EStatementsType
    };
    Q_ENUM(TransactionListType);

    ~OnlineBanking() override;

    bool initialize(const QString &name, const QString &key, const QString &version);
    void finalize();

    int setupAccounts(const QWidget *widget);
    ImExportProfileList importExportProfiles(bool import = true);

    Account *account(quint32 uniqueId);
    AccountIds accountIds();
    AccountList accounts();
    AccountBalanceList accountBalances(const Account *account,
                                       const QDate &from = QDate::currentDate().addDays(-28),
                                       const QDate &to = QDate::currentDate());

    TransactionList transactions(const Account *account,
                                 const QDate &from = QDate::currentDate().addDays(-28),
                                 const QDate &to = QDate::currentDate(),
                                 const TransactionListType &type = OnlineBanking::TransactionsType);

    TransactionList importTransactionsFromFile(const QString &importerName,
                                               const QString &profileName,
                                               const QString &fileName);

Q_SIGNALS:
    void progress(qreal progress);
    void error(const QString &message);

protected:
    OnlineBanking();
    Q_DISABLE_COPY(OnlineBanking)

private:
    class Private;
    QScopedPointer<Private> d_ptr;
};

} // namespace olbaflinx::core::banking

#endif //OLBAFLINX_ONLINEBANKING_H
