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
#include <QtWidgets/QWidget>

#include <aqbanking/types/imexporter_accountinfo.h>
#include <aqbanking/types/transaction.h>
#include <gwenhywfar/gwendate.h>

#include "core/Container.h"

namespace olbaflinx::core::banking {

using namespace olbaflinx::core;

class Banking;
class BankingPrivate : public QObject
{
    Q_DISABLE_COPY(BankingPrivate)
    Q_DECLARE_PUBLIC(Banking)

    Q_OBJECT

public:
    explicit BankingPrivate(Banking *banking);
    ~BankingPrivate() override;

    bool initializeAqBanking(const QString &name, const QString &key, const QString &version);
    void finalizeAqBanking();

    [[nodiscard]] Account *account(quint32 uniqueId) const;
    void receiveAccounts();
    void receiveAccountIds();

    void receiveTransactions(const Account *account, const QDate &from, const QDate &to);

    int showSetupDialog(QWidget *parentWidget) const;

    ImExportProfileList importExportProfiles(bool import = true);
    TransactionList import(const QString &importerName,
                           const QString &profileName,
                           const QString &fileName);

private:
    Banking *const q_ptr;
    bool mIsInitialized;

    AB_IMEXPORTER_ACCOUNTINFO *abImExporterAccountInfo(AB_TRANSACTION_COMMAND cmd,
                                                       quint32 uniqueId,
                                                       GWEN_DATE *from,
                                                       GWEN_DATE *to);
};
} // namespace olbaflinx::core::banking

#endif // OLBAFLINX_BANKINGPRIVATE_H
