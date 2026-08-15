/**
 * Copyright (C) 2022-2026, Alexander Saal <developer@olbaflinx.chm-projects.de>
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
#pragma once

#include "core/OlbaFlinxCore.h"

#include "core/Banking/BankingItem.h"

#include <aqbanking/types/transaction.h>
#include <aqbanking/types/transactionlimits.h>

#include <QtCore/QDate>
#include <QtCore/QMap>
#include <QtCore/QMetaType>

using namespace olbaflinx::core::banking;

namespace olbaflinx::core::banking::transaction {

typedef AB_TRANSACTION_TYPE TransactionType;
typedef AB_TRANSACTION_SUBTYPE TransactionSubType;
typedef AB_TRANSACTION_COMMAND TransactionCommand;
typedef AB_TRANSACTION_STATUS TransactionStatus;
typedef AB_TRANSACTION_SEQUENCE TransactionSequence;
typedef AB_TRANSACTION_CHARGE TransactionCharge;
typedef AB_TRANSACTION_PERIOD TransactionPeriod;

/**
 * @brief A transaction of an account.
 *
 * Ownership: the creator owns the instance. Transactions read from the database
 * are created through fromMap and handed on as a BankingItemPtr.
 */
class OLBAFLINX_CORE_EXPORT Transaction : public BankingItem
{
public:
    explicit Transaction(const AB_TRANSACTION *transaction = nullptr);

    /**
     * @brief A transaction that is told which account it belongs to.
     *
     * A statement that came over the wire names no account: the importer of the
     * backend fills the fields of the booking and leaves that one empty, and
     * only the entry of the response container it sits in carries it. Without
     * it the booking is stored under no account and is never read again.
     *
     * @param uniqueAccountId The account, as the banking backend keeps it. It is
     *  used only where the record names none of its own; a booking that carries
     *  one keeps it.
     * @param transaction The record. It stays with its caller, a copy of it is
     *  kept here.
     */
    Transaction(quint32 uniqueAccountId, const AB_TRANSACTION *transaction);

    ~Transaction() override;

    /**
     * @brief Creates a transaction from the column values of a database row.
     *
     * @param map Column values of the row.
     *
     * @return The new transaction.
     */
    [[nodiscard]] static std::shared_ptr<Transaction> fromMap(const QMap<QString, QVariant> &map);

    [[nodiscard]] TransactionType type() const;
    [[nodiscard]] TransactionSubType subType() const;
    [[nodiscard]] TransactionCommand command() const;
    [[nodiscard]] TransactionStatus status() const;
    [[nodiscard]] quint32 uniqueAccountId() const;
    [[nodiscard]] quint32 uniqueId() const;
    [[nodiscard]] quint32 refUniqueId() const;
    [[nodiscard]] quint32 idForApplication() const;
    /**
     * @brief The string id an application may assign to a booking.
     *
     * Answers empty for every transaction that comes from the database. The
     * value is deliberately not restored, see the comment in fromMap.
     */
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

    [[nodiscard]] QString calculateTransactionHash() const;

    [[nodiscard]] bool isValid() const override;
    [[nodiscard]] QString toString() const override;
    [[nodiscard]] QMap<QString, QVariant> toMap() const override;
    [[nodiscard]] QString itemType() const override;

private:
    class Private;
    Private *d_ptr = nullptr;
};

} // namespace olbaflinx::core::banking::transaction

Q_DECLARE_METATYPE(olbaflinx::core::banking::transaction::Transaction *)
