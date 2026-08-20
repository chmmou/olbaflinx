/**
 * Copyright (C) 2021-2026, Alexander Saal <developer@olbaflinx.chm-projects.de>
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

#include "core/Banking/StandingOrder/StandingOrder.h"

#include <aqbanking/types/imexporter_context.h>
#include <aqbanking/types/transaction.h>
#include <aqbanking/types/value.h>

#include <gwenhywfar/gwendate.h>

#include <QtCore/QDate>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QVariant>

#include <memory>

using namespace olbaflinx::core::banking::standingorder;

namespace olbaflinx::core::tests {

/**
 * What a standing order of a test carries. Every field has a value that stands
 * on its own, so a caller names the two or three that matter to it and leaves
 * the rest.
 */
struct StandingOrderSpec
{
    quint32 uniqueId = 4711;
    quint32 uniqueAccountId = 815;

    QString fiId = {};

    QString localName = QStringLiteral("Max Mustermann");
    QString localIban = QStringLiteral("DE02500105170137075030");
    QString localBic = QStringLiteral("INGDDEFF");

    QString remoteName = QStringLiteral("Erika Musterfrau");
    QString remoteIban = QStringLiteral("DE02120300000000202051");
    QString remoteBic = QStringLiteral("BYLADEM1001");

    double value = 42.5;
    QString currency = QStringLiteral("EUR");

    QString purpose = QStringLiteral("Miete");
    QString endToEndReference = {};

    AB_TRANSACTION_PERIOD period = AB_Transaction_PeriodMonthly;
    quint32 cycle = 1;
    quint32 executionDay = 1;

    QDate firstDate = QDate(2026, 1, 1);
    QDate lastDate = {};
    QDate nextDate = QDate(2026, 3, 1);

    AB_TRANSACTION_STATUS status = AB_Transaction_StatusAccepted;
    QString memo = {};

    /**
     * Left empty, the core forms the fingerprint while it writes. A value here
     * is what tells an order that already carries one.
     */
    QString fingerprint = {};
};

/**
 * The two ways a test needs a standing order: as the banking backend reports
 * one, and as the storage holds one.
 */
class StandingOrderHelpers
{
public:
    /**
     * A standing order the way the banking backend reports one.
     *
     * The type is what tells it from a booking, and nothing else does. An
     * AB_TRANSACTION without it is not a standing order to any path of the
     * application.
     */
    static std::shared_ptr<StandingOrder> fromBackend(const StandingOrderSpec &spec = {})
    {
        AB_TRANSACTION *abTransaction = transactionFromBackend(spec);

        auto order = std::make_shared<StandingOrder>(abTransaction);
        AB_Transaction_free(abTransaction);

        return order;
    }

    /**
     * The record itself, as the banking backend reports one. The caller owns it
     * and either releases it or hands it to a container that takes it over.
     */
    static AB_TRANSACTION *transactionFromBackend(const StandingOrderSpec &spec = {})
    {
        AB_TRANSACTION *abTransaction = AB_Transaction_new();

        // No command is set. The backend job that brings a standing order in
        // sets the type and nothing else, so a record that carried one would be
        // a container no bank ever sends.
        AB_Transaction_SetType(abTransaction, AB_Transaction_TypeStandingOrder);
        AB_Transaction_SetStatus(abTransaction, spec.status);
        AB_Transaction_SetUniqueId(abTransaction, spec.uniqueId);
        AB_Transaction_SetUniqueAccountId(abTransaction, spec.uniqueAccountId);

        setText(abTransaction, &AB_Transaction_SetFiId, spec.fiId);
        setText(abTransaction, &AB_Transaction_SetLocalName, spec.localName);
        setText(abTransaction, &AB_Transaction_SetLocalIban, spec.localIban);
        setText(abTransaction, &AB_Transaction_SetLocalBic, spec.localBic);
        setText(abTransaction, &AB_Transaction_SetRemoteName, spec.remoteName);
        setText(abTransaction, &AB_Transaction_SetRemoteIban, spec.remoteIban);
        setText(abTransaction, &AB_Transaction_SetRemoteBic, spec.remoteBic);
        setText(abTransaction, &AB_Transaction_SetPurpose, spec.purpose);
        setText(abTransaction, &AB_Transaction_SetEndToEndReference, spec.endToEndReference);
        setText(abTransaction, &AB_Transaction_SetMemo, spec.memo);
        setText(abTransaction, &AB_Transaction_SetHash, spec.fingerprint);

        AB_Transaction_SetPeriod(abTransaction, spec.period);
        AB_Transaction_SetCycle(abTransaction, spec.cycle);
        AB_Transaction_SetExecutionDay(abTransaction, spec.executionDay);

        setDate(abTransaction, &AB_Transaction_SetFirstDate, spec.firstDate);
        setDate(abTransaction, &AB_Transaction_SetLastDate, spec.lastDate);
        setDate(abTransaction, &AB_Transaction_SetNextDate, spec.nextDate);

        AB_VALUE *value = AB_Value_new();
        AB_Value_SetValueFromDouble(value, spec.value);
        AB_Value_SetCurrency(value, spec.currency.toUtf8().constData());
        AB_Transaction_SetValue(abTransaction, value);
        AB_Value_free(value);

        return abTransaction;
    }

    /**
     * The property map of a standing order, the way fromMap expects it and the
     * way the storage writes it.
     *
     * Built without going through the backend, so that a map without a
     * fingerprint is possible at all: toMap forms the value on every call.
     */
    static QMap<QString, QVariant> standingOrderMap(const StandingOrderSpec &spec = {})
    {
        return {
            {QStringLiteral("unique_id"), spec.uniqueId},
            {QStringLiteral("unique_account_id"), spec.uniqueAccountId},
            {QStringLiteral("fi_id"), spec.fiId},
            {QStringLiteral("local_name"), spec.localName},
            {QStringLiteral("local_iban"), spec.localIban},
            {QStringLiteral("local_bic"), spec.localBic},
            {QStringLiteral("remote_name"), spec.remoteName},
            {QStringLiteral("remote_iban"), spec.remoteIban},
            {QStringLiteral("remote_bic"), spec.remoteBic},
            {QStringLiteral("value"), spec.value},
            {QStringLiteral("currency"), spec.currency},
            {QStringLiteral("purpose"), spec.purpose},
            {QStringLiteral("end_to_end_reference"), spec.endToEndReference},
            {QStringLiteral("period"), static_cast<int>(spec.period)},
            {QStringLiteral("cycle"), spec.cycle},
            {QStringLiteral("execution_day"), spec.executionDay},
            {QStringLiteral("first_date"), spec.firstDate},
            {QStringLiteral("last_date"), spec.lastDate},
            {QStringLiteral("next_date"), spec.nextDate},
            {QStringLiteral("status"), static_cast<int>(spec.status)},
            {QStringLiteral("memo"), spec.memo},
            {QStringLiteral("fingerprint"), spec.fingerprint},
        };
    }

    /**
     * Puts standing orders of one account into a container a session would have
     * filled.
     *
     * The entry is looked up the way BankingHelpers builds it, so a container
     * may carry bookings and standing orders of the same account side by side.
     * The account number and the IBAN have to agree with the ones used there:
     * the lookup of the backend falls through to them, and two entries that
     * share them would be one.
     *
     * Every order carries the type, and that is what tells it from a booking.
     * The account stays empty on the order itself, as it does on a booking:
     * only the entry it sits in names it.
     */
    static void addStandingOrdersToContext(AB_IMEXPORTER_CONTEXT *context,
                                           quint32 uniqueAccountId,
                                           int count,
                                           const QString &fiIdPrefix = {})
    {
        const QString accountNumber = QString::number(uniqueAccountId).rightJustified(10, u'0');
        const QByteArray localAccountNumber = accountNumber.toLatin1();
        const QByteArray localIban = (QStringLiteral("DE0212030000") + accountNumber).toLatin1();

        AB_IMEXPORTER_ACCOUNTINFO *info
            = AB_ImExporterContext_GetOrAddAccountInfo(context,
                                                       uniqueAccountId,
                                                       localIban.constData(),
                                                       "12030000",
                                                       localAccountNumber.constData(),
                                                       AB_AccountType_Checking);

        for (int index = 0; index < count; ++index) {
            auto spec = StandingOrderSpec{};
            spec.uniqueAccountId = uniqueAccountId;
            spec.uniqueId = static_cast<quint32>(index + 1);
            spec.value = 10.0 + index;
            spec.purpose = QStringLiteral("Order %1").arg(index);
            spec.remoteName = QStringLiteral("Partner %1").arg(index);

            if (!fiIdPrefix.isEmpty()) {
                spec.fiId = fiIdPrefix + QString::number(index);
            }

            AB_ImExporterAccountInfo_AddTransaction(info, transactionFromBackend(spec));
        }
    }

    /**
     * A run of orders under one account, each one different from the others in
     * the fields the fingerprint is formed over.
     */
    static BankingItems orderRun(quint32 uniqueAccountId, int count, const QString &fiIdPrefix = {})
    {
        auto items = BankingItems();
        items.reserve(count);

        for (int index = 0; index < count; ++index) {
            auto spec = StandingOrderSpec{};
            spec.uniqueAccountId = uniqueAccountId;
            spec.uniqueId = static_cast<quint32>(index + 1);
            spec.value = 10.0 + index;
            spec.purpose = QStringLiteral("Order %1").arg(index);
            spec.remoteName = QStringLiteral("Partner %1").arg(index);

            if (!fiIdPrefix.isEmpty()) {
                spec.fiId = fiIdPrefix + QString::number(index);
            }

            items << fromBackend(spec);
        }

        return items;
    }

private:
    /**
     * Hands a string to a setter of the backend, or leaves the field alone when
     * there is nothing to set. An empty QString would reach the setter as an
     * empty C string, which is not the same as a field that was never filled.
     */
    template<typename Setter>
    static void setText(AB_TRANSACTION *transaction, Setter setter, const QString &text)
    {
        if (text.isEmpty()) {
            return;
        }

        setter(transaction, text.toUtf8().constData());
    }

    /**
     * The same for a date. GWEN_Date_fromGregorian takes a day of its own, and
     * an invalid QDate has none to give.
     */
    template<typename Setter>
    static void setDate(AB_TRANSACTION *transaction, Setter setter, const QDate &date)
    {
        if (!date.isValid()) {
            return;
        }

        GWEN_DATE *gwenDate = GWEN_Date_fromGregorian(date.year(), date.month(), date.day());
        setter(transaction, gwenDate);
        GWEN_Date_free(gwenDate);
    }
};

} // namespace olbaflinx::core::tests
