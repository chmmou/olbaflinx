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

#include <QtCore/QDate>
#include <QtCore/QMap>
#include <QtCore/QMetaType>

using namespace olbaflinx::core::banking;

namespace olbaflinx::core::banking::standingorder {

/**
 * How a standing order says which of the two ways carries it in the storage.
 */
enum IdentifiedBy : int {
    /** The identifier the institution assigns. */
    InstitutionId = 1,
    /** The fingerprint over the fields that identify the order. */
    Fingerprint = 2,
};

/**
 * A standing order of an account: the instruction to transfer an amount to a
 * payee in a fixed cycle, until it is revoked.
 *
 * It arrives as an AB_TRANSACTION of the standing order type, which is the only
 * thing that tells it from a booking. An instance whose record carries another
 * type answers false to isValid and is not written.
 *
 * Ownership: the record of the library stays with its owner; a duplicate of it
 * is kept here and released with the instance. Orders read from the database are
 * created through fromMap and handed on as a BankingItemPtr.
 *
 * Concurrency: none. An instance is used in the thread it came into being in.
 */
class OLBAFLINX_CORE_EXPORT StandingOrder : public BankingItem
{
public:
    explicit StandingOrder(const AB_TRANSACTION *transaction = nullptr);

    /**
     * An order that is told which account it belongs to.
     *
     * An order that came over the wire names no account: the importer of the
     * backend fills the fields of the order and leaves that one empty, and only
     * the entry of the response container it sits in carries it. Without it the
     * order is stored under no account and is never read again.
     *
     * The account is used only where the record names none of its own; an order
     * that carries one keeps it.
     */
    StandingOrder(quint32 uniqueAccountId, const AB_TRANSACTION *transaction);

    ~StandingOrder() override;

    // The instance owns a C structure and frees it. A copy would hand the same
    // pointer to two destructors, so the compiler generated ones are withdrawn
    // rather than left to be called by accident.
    StandingOrder(const StandingOrder &) = delete;
    StandingOrder &operator=(const StandingOrder &) = delete;
    StandingOrder(StandingOrder &&) = delete;
    StandingOrder &operator=(StandingOrder &&) = delete;

    /**
     * Creates a standing order from the column values of a database row.
     *
     * The type is set here rather than read from the row: the table holds
     * standing orders alone and carries no column for it, while every path of
     * the application tells an order from a booking by exactly that field.
     */
    [[nodiscard]] static std::shared_ptr<StandingOrder> fromMap(const QMap<QString, QVariant> &map);

    [[nodiscard]] quint32 uniqueAccountId() const;
    [[nodiscard]] quint32 uniqueId() const;

    /**
     * The identifier the institution assigns to the order.
     *
     * May be empty. That is no failure but the case the fingerprint is there
     * for, and not every institution assigns one.
     */
    [[nodiscard]] QString fiId() const;

    [[nodiscard]] QString localIban() const;
    [[nodiscard]] QString localBic() const;
    [[nodiscard]] QString localName() const;
    [[nodiscard]] QString remoteIban() const;
    [[nodiscard]] QString remoteBic() const;
    [[nodiscard]] QString remoteName() const;
    [[nodiscard]] qreal value() const;
    [[nodiscard]] QString currency() const;
    [[nodiscard]] QString purpose() const;
    [[nodiscard]] QString endToEndReference() const;

    /**
     * The span between two executions. The backend knows monthly and weekly and
     * nothing else, so a quarterly order is a monthly one of cycle three.
     */
    [[nodiscard]] AB_TRANSACTION_PERIOD period() const;

    [[nodiscard]] quint32 cycle() const;

    /** The day of the execution, in the month or in the week. */
    [[nodiscard]] quint32 executionDay() const;

    /**
     * The three dates of an order. A date the backend does not report is an
     * invalid QDate and never today.
     */
    [[nodiscard]] QDate firstDate() const;
    [[nodiscard]] QDate lastDate() const;
    [[nodiscard]] QDate nextDate() const;

    /**
     * When the order runs next, seen from the given day.
     *
     * The date the institution reports where it lies on or after that day,
     * otherwise the execution worked out from the first one and the cycle.
     *
     * An order whose period or cycle is unknown answers with its first
     * execution, the only date it then carries. An order past its last
     * execution answers with an invalid date, and so does one that carries no
     * date at all.
     */
    [[nodiscard]] QDate nextExecution(const QDate &from) const;

    /**
     * The state the institution reports. Carried through the storage without
     * being read anywhere yet.
     */
    [[nodiscard]] AB_TRANSACTION_STATUS status() const;

    [[nodiscard]] QString memo() const;

    /** The fingerprint the record already carries, empty where it carries none. */
    [[nodiscard]] QString fingerprint() const;

    /**
     * The fingerprint the order is recognised by where the institution assigns
     * no identifier.
     *
     * Formed over the ten fields that tell one order of an account from another:
     * the account, the payee by IBAN and by name, the amount and its currency,
     * the purpose, the period with its cycle and execution day, and the first
     * execution. A field the bank does not report goes in as the empty value it
     * is rather than dropping out.
     *
     * The next execution, the last one and the state are deliberately left out.
     * The first two move over the life of an order and the third is no property
     * of the order at all, so an order carrying them would be a new one on every
     * fetch.
     *
     * Formed on the first call and held afterwards. A record that already
     * carries a fingerprint keeps it.
     */
    [[nodiscard]] QString calculateFingerprint() const;

    /**
     * True where the record is a standing order. A record of another type is
     * refused by the storage rather than written into a table it does not belong
     * in.
     */
    [[nodiscard]] bool isValid() const override;

    [[nodiscard]] QString toString() const override;

    /**
     * The keys are the column names of the standing order table. The row id, the
     * account row and the mark that says a fetch no longer reports the order
     * belong to the storage and are not among them.
     */
    [[nodiscard]] QMap<QString, QVariant> toMap() const override;

    [[nodiscard]] QString itemType() const override;

private:
    class Private;
    Private *d_ptr = nullptr;
};

} // namespace olbaflinx::core::banking::standingorder

Q_DECLARE_METATYPE(olbaflinx::core::banking::standingorder::StandingOrder *)
