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

#include <core/Banking/Private/BankingPrivate.h>
#include <QtCore/QObject>
#include <QtWidgets/QWidget>

#include "core/Container.h"
#include "core/Singleton.h"

namespace olbaflinx::core::banking {

using namespace olbaflinx::core;

class BankingPrivate;
class Banking : public QObject, public Singleton<Banking>
{
    Q_OBJECT
    friend class Singleton<Banking>;

public:
    enum ErrorType {
        InitializedError,
        AccountError,
        TransactionError,

        // Last
        UnknownError = -1
    };
    Q_ENUM(ErrorType)

    ~Banking() override;

    [[nodiscard]] bool initialize(const QString &name,
                                  const QString &key,
                                  const QString &version) const;
    void deInitialize();

    int showSetupDialog(QWidget *widget) const;

    [[nodiscard]] Account *account(quint32 uniqueId) const;

    ImExportProfileList importExportProfiles(bool import = true);
    TransactionList import(const QString &importerName,
                           const QString &profileName,
                           const QString &fileName);

public Q_SLOTS:
    void receiveAccounts();
    void receiveAccountIds();

    void receiveTransactions(const Account *account, const QDate &from, const QDate &to);
    void receiveStandingOrders(const Account *account);

Q_SIGNALS:
    void accountsReceived(const AccountList &accountList);
    void accountIdsReceived(const AccountIds &accountIds);
    void transactionsReceived(const TransactionList &transactionList,
                              const QString &balance = QString());

    void errorOccurred(const QString &message, const Banking::ErrorType errorType);
    void progressStatus(qreal current, qreal total);

private:
    QScopedPointer<BankingPrivate> const d_ptr;

private Q_SLOTS:

protected:
    Banking();
    Q_DISABLE_COPY(Banking)
};

} // namespace olbaflinx::core::banking

#endif //OLBAFLINX_BANKING_H
