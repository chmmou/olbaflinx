/**
 * Copyright (c) 2021, Alexander Saal <developer@olbaflinx.chm-projects.de>
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

#ifndef OLBAFLINX_TRANSACTION_H
#define OLBAFLINX_TRANSACTION_H

#include <QtCore/QDate>
#include <QtCore/QMap>
#include <QtCore/QMetaType>
#include <QtSql/QSqlQuery>

#include <aqbanking/types/transaction.h>
#include <aqbanking/types/transactionlimits.h>

namespace olbaflinx::core::storage::transaction {

typedef AB_TRANSACTION_TYPE TransactionType;
typedef AB_TRANSACTION_SUBTYPE TransactionSubType;
typedef AB_TRANSACTION_COMMAND TransactionCommand;
typedef AB_TRANSACTION_STATUS TransactionStatus;
typedef AB_TRANSACTION_SEQUENCE TransactionSequence;
typedef AB_TRANSACTION_CHARGE TransactionCharge;
typedef AB_TRANSACTION_PERIOD TransactionPeriod;

class Transaction
{
public:
    explicit Transaction(const AB_TRANSACTION *transaction);
    ~Transaction();

    [[nodiscard]] TransactionType type() const;
    [[nodiscard]] TransactionSubType subType() const;
    [[nodiscard]] TransactionCommand command() const;
    [[nodiscard]] TransactionStatus status() const;
    [[nodiscard]] quint32 uniqueAccountId() const;
    [[nodiscard]] quint32 uniqueId() const;
    [[nodiscard]] quint32 refUniqueId() const;
    [[nodiscard]] quint32 idForApplication() const;
    [[nodiscard]] QString stringIdForApplication() const;
    [[nodiscard]] quint32 sessionId() const;
    [[nodiscard]] quint32 groupId() const;
    [[nodiscard]] QString fiId() const;
    [[nodiscard]] QString localIban() const;
    [[nodiscard]] QString localBic() const;
    [[nodiscard]] QString localCountry() const;
    [[nodiscard]] QString localBankCode() const;
    [[nodiscard]] QString localBranchId() const;
    [[nodiscard]] QString localAccountNumber() const;
    [[nodiscard]] QString localSuffix() const;
    [[nodiscard]] QString localName() const;
    [[nodiscard]] QString remoteCountry() const;
    [[nodiscard]] QString remoteBankCode() const;
    [[nodiscard]] QString remoteBranchId() const;
    [[nodiscard]] QString remoteAccountNumber() const;
    [[nodiscard]] QString remoteSuffix() const;
    [[nodiscard]] QString remoteIban() const;
    [[nodiscard]] QString remoteBic() const;
    [[nodiscard]] QString remoteName() const;
    [[nodiscard]] QDate date() const;
    [[nodiscard]] QDate valutaDate() const;
    [[nodiscard]] qreal value() const;
    [[nodiscard]] QString currency() const;
    [[nodiscard]] qreal fees() const;
    [[nodiscard]] int transactionCode() const;
    [[nodiscard]] QString transactionText() const;
    [[nodiscard]] QString transactionKey() const;
    [[nodiscard]] int textKey() const;
    [[nodiscard]] QString primanota() const;
    [[nodiscard]] QString purpose() const;
    [[nodiscard]] QString category() const;
    [[nodiscard]] QString customerReference() const;
    [[nodiscard]] QString bankReference() const;
    [[nodiscard]] QString endToEndReference() const;
    [[nodiscard]] QString creditorSchemeId() const;
    [[nodiscard]] QString originatorId() const;
    [[nodiscard]] QString mandateId() const;
    [[nodiscard]] QDate mandateDate() const;
    [[nodiscard]] QString mandateDebitorName() const;
    [[nodiscard]] QString originalCreditorSchemeId() const;
    [[nodiscard]] QString originalMandateId() const;
    [[nodiscard]] QString originalCreditorName() const;
    [[nodiscard]] TransactionSequence sequence() const;
    [[nodiscard]] TransactionCharge charge() const;
    [[nodiscard]] QString remoteAddrStreet() const;
    [[nodiscard]] QString remoteAddrZipcode() const;
    [[nodiscard]] QString remoteAddrCity() const;
    [[nodiscard]] QString remoteAddrPhone() const;
    [[nodiscard]] TransactionPeriod period() const;
    [[nodiscard]] quint32 cycle() const;
    [[nodiscard]] quint32 executionDay() const;
    [[nodiscard]] QDate firstDate() const;
    [[nodiscard]] QDate lastDate() const;
    [[nodiscard]] QDate nextDate() const;
    [[nodiscard]] QString unitId() const;
    [[nodiscard]] QString unitIdNameSpace() const;
    [[nodiscard]] QString tickerSymbol() const;
    [[nodiscard]] qreal units() const;
    [[nodiscard]] qreal unitPriceValue() const;
    [[nodiscard]] QDate unitPriceDate() const;
    [[nodiscard]] qreal commissionValue() const;
    [[nodiscard]] QString memo() const;
    [[nodiscard]] QString hash() const;

    [[nodiscard]] QString toString() const;
    [[nodiscard]] bool isStandingOrder() const;
    [[nodiscard]] QString calculateTransactionHash() const;

    [[nodiscard]] QSqlQuery createInsertQuery(const quint32 &accountId, QSqlQuery &query) const;
    [[nodiscard]] static QMap<QString, QVariant> queryToMap(const QSqlQuery &query);
    [[nodiscard]] static Transaction *create(const QMap<QString, QVariant> &row);

private:
    AB_TRANSACTION *abTransaction;
};

} // namespace olbaflinx::core::storage::transaction

Q_DECLARE_METATYPE(olbaflinx::core::storage::transaction::Transaction *)
Q_DECLARE_METATYPE(const olbaflinx::core::storage::transaction::Transaction *)

#endif // OLBAFLINX_TRANSACTION_H
