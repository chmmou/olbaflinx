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

#include "core/Banking/Transaction/Transaction.h"

#include <gwenhywfar/buffer.h>
#include <gwenhywfar/gwendate.h>

#include <QtCore/QCryptographicHash>
#include <QtCore/QIODevice>

#include <memory>

using namespace olbaflinx::core::banking::transaction;

namespace {

/**
 * The C structures of the backend, held so that every path out of a function
 * releases them.
 */
using GwenDatePtr = std::unique_ptr<GWEN_DATE, decltype(&GWEN_Date_free)>;
using GwenBufferPtr = std::unique_ptr<GWEN_BUFFER, decltype(&GWEN_Buffer_free)>;

} // namespace

class Transaction::Private
{
public:
    // A duplicate is made only of what the caller handed in. The fallback used to
    // build a transaction and duplicate that one as well, so the structure it had
    // just created was never released.
    explicit Private(Transaction *transaction, const AB_TRANSACTION *abTT)
        : abTransaction(abTT ? AB_Transaction_dup(abTT) : AB_Transaction_new())
        , q_ptr(transaction)
    {}

    ~Private()
    {
        AB_Transaction_free(abTransaction);
        abTransaction = nullptr;
    }

    /**
     * The fingerprint a booking is recognised by.
     *
     * It used to be taken over the end to end reference alone, which most
     * bookings do not carry. Every one of those shared the hash of an empty
     * string with every other, so they could not be told apart. The fields below
     * are the ones that together identify a booking.
     */
    QString calculateTransactionHash()
    {
        if (!q_ptr->hash().isEmpty()) {
            return q_ptr->hash();
        }

        QByteArray buffer;
        QDataStream out(&buffer, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_DefaultCompiledVersion);

        out << q_ptr->uniqueAccountId() << q_ptr->uniqueId() << q_ptr->date() << q_ptr->valutaDate()
            << q_ptr->value() << q_ptr->currency() << q_ptr->localIban() << q_ptr->remoteIban()
            << q_ptr->remoteAccountNumber() << q_ptr->remoteName() << q_ptr->purpose()
            << q_ptr->customerReference() << q_ptr->bankReference() << q_ptr->endToEndReference();

        return {QCryptographicHash::hash(buffer, QCryptographicHash::Sha256).toHex()};
    }

    /**
     * Reads a date out of the banking backend.
     *
     * A GWEN_DATE carries a year, a month and a day and nothing else. The
     * template used to ask for a time as well, which the source cannot fill.
     *
     * A date that cannot be read answers with an invalid QDate. It used to
     * answer with today, which put an invented day into booking data.
     */
    static QDate toDate(const GWEN_DATE *gwenDate)
    {
        if (gwenDate == nullptr) {
            return {};
        }

        // The early return below used to leak this buffer.
        const GwenBufferPtr buffer(GWEN_Buffer_new(nullptr, 16, 0, 1), &GWEN_Buffer_free);

        if (GWEN_Date_toStringWithTemplate(gwenDate, "DD.MM.YYYY", buffer.get()) != GWEN_SUCCESS) {
            return {};
        }

        return QDate::fromString(QString::fromUtf8(GWEN_Buffer_GetStart(buffer.get())),
                                 QStringLiteral("dd.MM.yyyy"));
    }

    /**
     * Hands a date to the banking backend.
     *
     * Ownership stays here. Every setter of AB_TRANSACTION duplicates what it is
     * given, so the handle has to be released again; it used to be dropped at all
     * seven call sites.
     *
     * A date that is not set answers with an empty handle, which the setters read
     * as "no date". It used to answer with today.
     */
    static GwenDatePtr fromDate(const QDate &date)
    {
        if (!date.isValid() || date.isNull()) {
            return {nullptr, &GWEN_Date_free};
        }

        const auto text = date.toString(QStringLiteral("yyyyMMdd")).toLatin1();

        return {GWEN_Date_fromString(text.constData()), &GWEN_Date_free};
    }

    AB_TRANSACTION *abTransaction;

private:
    Transaction *q_ptr;
};

Transaction::Transaction(const AB_TRANSACTION *transaction)
    : BankingItem()
    , d_ptr(new Private(this, transaction))
{}

Transaction::~Transaction()
{
    delete d_ptr;
}

TransactionType Transaction::type() const
{
    return AB_Transaction_GetType(d_ptr->abTransaction);
}

TransactionSubType Transaction::subType() const
{
    return AB_Transaction_GetSubType(d_ptr->abTransaction);
}

TransactionCommand Transaction::command() const
{
    return AB_Transaction_GetCommand(d_ptr->abTransaction);
}

TransactionStatus Transaction::status() const
{
    return AB_Transaction_GetStatus(d_ptr->abTransaction);
}

quint32 Transaction::uniqueAccountId() const
{
    return AB_Transaction_GetUniqueAccountId(d_ptr->abTransaction);
}

quint32 Transaction::uniqueId() const
{
    return AB_Transaction_GetUniqueId(d_ptr->abTransaction);
}

quint32 Transaction::refUniqueId() const
{
    return AB_Transaction_GetRefUniqueId(d_ptr->abTransaction);
}

quint32 Transaction::idForApplication() const
{
    return AB_Transaction_GetIdForApplication(d_ptr->abTransaction);
}

QString Transaction::stringIdForApplication() const
{
    return QString::fromUtf8(AB_Transaction_GetStringIdForApplication(d_ptr->abTransaction));
}

quint32 Transaction::sessionId() const
{
    return AB_Transaction_GetSessionId(d_ptr->abTransaction);
}

quint32 Transaction::groupId() const
{
    return AB_Transaction_GetGroupId(d_ptr->abTransaction);
}

QString Transaction::fiId() const
{
    return QString::fromUtf8(AB_Transaction_GetFiId(d_ptr->abTransaction));
}

QString Transaction::localIban() const
{
    return QString::fromUtf8(AB_Transaction_GetLocalIban(d_ptr->abTransaction));
}

QString Transaction::localBic() const
{
    return QString::fromUtf8(AB_Transaction_GetLocalBic(d_ptr->abTransaction));
}

QString Transaction::localCountry() const
{
    return QString::fromUtf8(AB_Transaction_GetLocalCountry(d_ptr->abTransaction));
}

QString Transaction::localBankCode() const
{
    return QString::fromUtf8(AB_Transaction_GetLocalBankCode(d_ptr->abTransaction));
}

QString Transaction::localBranchId() const
{
    return QString::fromUtf8(AB_Transaction_GetLocalBranchId(d_ptr->abTransaction));
}

QString Transaction::localAccountNumber() const
{
    return QString::fromUtf8(AB_Transaction_GetLocalAccountNumber(d_ptr->abTransaction));
}

QString Transaction::localSuffix() const
{
    return QString::fromUtf8(AB_Transaction_GetLocalSuffix(d_ptr->abTransaction));
}

QString Transaction::localName() const
{
    return QString::fromUtf8(AB_Transaction_GetLocalName(d_ptr->abTransaction));
}

QString Transaction::remoteCountry() const
{
    return QString::fromUtf8(AB_Transaction_GetRemoteCountry(d_ptr->abTransaction));
}

QString Transaction::remoteBankCode() const
{
    return QString::fromUtf8(AB_Transaction_GetRemoteBankCode(d_ptr->abTransaction));
}

QString Transaction::remoteBranchId() const
{
    return QString::fromUtf8(AB_Transaction_GetRemoteBranchId(d_ptr->abTransaction));
}

QString Transaction::remoteAccountNumber() const
{
    return QString::fromUtf8(AB_Transaction_GetRemoteAccountNumber(d_ptr->abTransaction));
}

QString Transaction::remoteSuffix() const
{
    return QString::fromUtf8(AB_Transaction_GetRemoteSuffix(d_ptr->abTransaction));
}

QString Transaction::remoteIban() const
{
    return QString::fromUtf8(AB_Transaction_GetRemoteIban(d_ptr->abTransaction));
}

QString Transaction::remoteBic() const
{
    return QString::fromUtf8(AB_Transaction_GetRemoteBic(d_ptr->abTransaction));
}

QString Transaction::remoteName() const
{
    return QString::fromUtf8(AB_Transaction_GetRemoteName(d_ptr->abTransaction));
}

QDate Transaction::date() const
{
    return Private::toDate(AB_Transaction_GetDate(d_ptr->abTransaction));
}

QDate Transaction::valutaDate() const
{
    return Private::toDate(AB_Transaction_GetValutaDate(d_ptr->abTransaction));
}

qreal Transaction::value() const
{
    const auto value = AB_Transaction_GetValue(d_ptr->abTransaction);
    if (value == nullptr) {
        return 0.0;
    }

    return AB_Value_GetValueAsDouble(value);
}

QString Transaction::currency() const
{
    return QString::fromUtf8(AB_Value_GetCurrency(AB_Transaction_GetValue(d_ptr->abTransaction)));
}

qreal Transaction::fees() const
{
    const auto fees = AB_Transaction_GetFees(d_ptr->abTransaction);
    if (fees == nullptr) {
        return 0.0;
    }

    return AB_Value_GetValueAsDouble(fees);
}

int Transaction::transactionCode() const
{
    return AB_Transaction_GetTransactionCode(d_ptr->abTransaction);
}

QString Transaction::transactionText() const
{
    return QString::fromUtf8(AB_Transaction_GetTransactionText(d_ptr->abTransaction));
}

QString Transaction::transactionKey() const
{
    return QString::fromUtf8(AB_Transaction_GetTransactionKey(d_ptr->abTransaction));
}

int Transaction::textKey() const
{
    return AB_Transaction_GetTextKey(d_ptr->abTransaction);
}

QString Transaction::primanota() const
{
    return QString::fromUtf8(AB_Transaction_GetPrimanota(d_ptr->abTransaction));
}

QString Transaction::purpose() const
{
    return QString::fromUtf8(AB_Transaction_GetPurpose(d_ptr->abTransaction));
}

QString Transaction::category() const
{
    return QString::fromUtf8(AB_Transaction_GetCategory(d_ptr->abTransaction));
}

QString Transaction::customerReference() const
{
    return QString::fromUtf8(AB_Transaction_GetCustomerReference(d_ptr->abTransaction));
}

QString Transaction::bankReference() const
{
    return QString::fromUtf8(AB_Transaction_GetBankReference(d_ptr->abTransaction));
}

QString Transaction::endToEndReference() const
{
    return QString::fromUtf8(AB_Transaction_GetEndToEndReference(d_ptr->abTransaction));
}

QString Transaction::creditorSchemeId() const
{
    return QString::fromUtf8(AB_Transaction_GetCreditorSchemeId(d_ptr->abTransaction));
}

QString Transaction::originatorId() const
{
    return QString::fromUtf8(AB_Transaction_GetOriginatorId(d_ptr->abTransaction));
}

QString Transaction::mandateId() const
{
    return QString::fromUtf8(AB_Transaction_GetMandateId(d_ptr->abTransaction));
}

QDate Transaction::mandateDate() const
{
    return Private::toDate(AB_Transaction_GetMandateDate(d_ptr->abTransaction));
}

QString Transaction::mandateDebitorName() const
{
    return QString::fromUtf8(AB_Transaction_GetMandateDebitorName(d_ptr->abTransaction));
}

QString Transaction::originalCreditorSchemeId() const
{
    return QString::fromUtf8(AB_Transaction_GetOriginalCreditorSchemeId(d_ptr->abTransaction));
}

QString Transaction::originalMandateId() const
{
    return QString::fromUtf8(AB_Transaction_GetOriginalMandateId(d_ptr->abTransaction));
}

QString Transaction::originalCreditorName() const
{
    return QString::fromUtf8(AB_Transaction_GetOriginalCreditorName(d_ptr->abTransaction));
}

TransactionSequence Transaction::sequence() const
{
    return AB_Transaction_GetSequence(d_ptr->abTransaction);
}

TransactionCharge Transaction::charge() const
{
    return AB_Transaction_GetCharge(d_ptr->abTransaction);
}

QString Transaction::remoteAddrStreet() const
{
    return QString::fromUtf8(AB_Transaction_GetRemoteAddrStreet(d_ptr->abTransaction));
}

QString Transaction::remoteAddrZipcode() const
{
    return QString::fromUtf8(AB_Transaction_GetRemoteAddrZipcode(d_ptr->abTransaction));
}

QString Transaction::remoteAddrCity() const
{
    return QString::fromUtf8(AB_Transaction_GetRemoteAddrCity(d_ptr->abTransaction));
}

QString Transaction::remoteAddrPhone() const
{
    return QString::fromUtf8(AB_Transaction_GetRemoteAddrPhone(d_ptr->abTransaction));
}

TransactionPeriod Transaction::period() const
{
    return AB_Transaction_GetPeriod(d_ptr->abTransaction);
}

quint32 Transaction::cycle() const
{
    return AB_Transaction_GetCycle(d_ptr->abTransaction);
}

quint32 Transaction::executionDay() const
{
    return AB_Transaction_GetExecutionDay(d_ptr->abTransaction);
}

QDate Transaction::firstDate() const
{
    return Private::toDate(AB_Transaction_GetFirstDate(d_ptr->abTransaction));
}

QDate Transaction::lastDate() const
{
    return Private::toDate(AB_Transaction_GetLastDate(d_ptr->abTransaction));
}

QDate Transaction::nextDate() const
{
    return Private::toDate(AB_Transaction_GetNextDate(d_ptr->abTransaction));
}

QString Transaction::unitId() const
{
    return QString::fromUtf8(AB_Transaction_GetUnitId(d_ptr->abTransaction));
}

QString Transaction::unitIdNameSpace() const
{
    return QString::fromUtf8(AB_Transaction_GetUnitIdNameSpace(d_ptr->abTransaction));
}

QString Transaction::tickerSymbol() const
{
    return QString::fromUtf8(AB_Transaction_GetTickerSymbol(d_ptr->abTransaction));
}

qreal Transaction::units() const
{
    const auto units = AB_Transaction_GetUnits(d_ptr->abTransaction);
    if (units == nullptr) {
        return 0.0;
    }

    return AB_Value_GetValueAsDouble(units);
}

qreal Transaction::unitPriceValue() const
{
    const auto unitPriceValue = AB_Transaction_GetUnitPriceValue(d_ptr->abTransaction);
    if (unitPriceValue == nullptr) {
        return 0.0;
    }

    return AB_Value_GetValueAsDouble(unitPriceValue);
}

QDate Transaction::unitPriceDate() const
{
    return Private::toDate(AB_Transaction_GetUnitPriceDate(d_ptr->abTransaction));
}

qreal Transaction::commissionValue() const
{
    const auto commissionValue = AB_Transaction_GetCommissionValue(d_ptr->abTransaction);
    if (commissionValue == nullptr) {
        return 0.0;
    }

    return AB_Value_GetValueAsDouble(commissionValue);
}

QString Transaction::memo() const
{
    return QString::fromUtf8(AB_Transaction_GetMemo(d_ptr->abTransaction));
}

QString Transaction::hash() const
{
    return QString::fromUtf8(AB_Transaction_GetHash(d_ptr->abTransaction));
}

QString Transaction::calculateTransactionHash() const
{
    return d_ptr->calculateTransactionHash();
}

std::shared_ptr<Transaction> Transaction::fromMap(const QMap<QString, QVariant> &map)
{
    auto abTransaction = AB_Transaction_new();

    AB_Transaction_SetType(abTransaction,
                           (TransactionType) map.value(QStringLiteral("type")).toInt());
    AB_Transaction_SetSubType(abTransaction,
                              (TransactionSubType) map.value(QStringLiteral("sub_type")).toInt());
    AB_Transaction_SetCommand(abTransaction,
                              (TransactionCommand) map.value(QStringLiteral("command")).toInt());
    AB_Transaction_SetStatus(abTransaction,
                             (TransactionStatus) map.value(QStringLiteral("status")).toInt());
    AB_Transaction_SetUniqueAccountId(abTransaction,
                                      map.value(QStringLiteral("unique_account_id")).toUInt());
    AB_Transaction_SetUniqueId(abTransaction, map.value(QStringLiteral("unique_id")).toUInt());
    AB_Transaction_SetRefUniqueId(abTransaction,
                                  map.value(QStringLiteral("ref_unique_id")).toUInt());
    AB_Transaction_SetIdForApplication(abTransaction,
                                       map.value(QStringLiteral("id_for_application")).toUInt());
    // The string id an application may assign is deliberately not restored. Of
    // the 44 character pointers an AB_TRANSACTION holds, it is the only one the
    // backend never releases: its declaration lacks the flag that makes the
    // generator emit the free, so every path that fills it allocates while
    // AB_Transaction_free walks past it. Setting it here would cost one
    // allocation per transaction read from the store, and releasing it here
    // would turn into a double free the day the backend is fixed. The column
    // keeps its place in toMap and in the table and carries an empty value.
    AB_Transaction_SetSessionId(abTransaction, map.value(QStringLiteral("session_id")).toUInt());
    AB_Transaction_SetGroupId(abTransaction, map.value(QStringLiteral("group_id")).toUInt());
    AB_Transaction_SetFiId(abTransaction,
                           map.value(QStringLiteral("fi_id")).toString().toLocal8Bit().constData());
    AB_Transaction_SetLocalIban(
        abTransaction, map.value(QStringLiteral("local_iban")).toString().toLocal8Bit().constData());
    AB_Transaction_SetLocalBic(
        abTransaction, map.value(QStringLiteral("local_bic")).toString().toLocal8Bit().constData());
    AB_Transaction_SetLocalCountry(abTransaction,
                                   map.value(QStringLiteral("local_country"))
                                       .toString()
                                       .toLocal8Bit()
                                       .constData());
    AB_Transaction_SetLocalBankCode(abTransaction,
                                    map.value(QStringLiteral("local_bank_code"))
                                        .toString()
                                        .toLocal8Bit()
                                        .constData());
    AB_Transaction_SetLocalBranchId(abTransaction,
                                    map.value(QStringLiteral("local_branch_id"))
                                        .toString()
                                        .toLocal8Bit()
                                        .constData());
    AB_Transaction_SetLocalAccountNumber(abTransaction,
                                         map.value(QStringLiteral("local_account_number"))
                                             .toString()
                                             .toLocal8Bit()
                                             .constData());
    AB_Transaction_SetLocalSuffix(abTransaction,
                                  map.value(QStringLiteral("local_suffix"))
                                      .toString()
                                      .toLocal8Bit()
                                      .constData());
    AB_Transaction_SetLocalName(
        abTransaction, map.value(QStringLiteral("local_name")).toString().toLocal8Bit().constData());
    AB_Transaction_SetRemoteCountry(abTransaction,
                                    map.value(QStringLiteral("remote_country"))
                                        .toString()
                                        .toLocal8Bit()
                                        .constData());
    AB_Transaction_SetRemoteBankCode(abTransaction,
                                     map.value(QStringLiteral("remote_bank_code"))
                                         .toString()
                                         .toLocal8Bit()
                                         .constData());
    AB_Transaction_SetRemoteBranchId(abTransaction,
                                     map.value(QStringLiteral("remote_branch_id"))
                                         .toString()
                                         .toLocal8Bit()
                                         .constData());
    AB_Transaction_SetRemoteAccountNumber(abTransaction,
                                          map.value(QStringLiteral("remote_account_number"))
                                              .toString()
                                              .toLocal8Bit()
                                              .constData());
    AB_Transaction_SetRemoteSuffix(abTransaction,
                                   map.value(QStringLiteral("remote_suffix"))
                                       .toString()
                                       .toLocal8Bit()
                                       .constData());
    AB_Transaction_SetRemoteIban(abTransaction,
                                 map.value(QStringLiteral("remote_iban"))
                                     .toString()
                                     .toLocal8Bit()
                                     .constData());
    AB_Transaction_SetRemoteBic(
        abTransaction, map.value(QStringLiteral("remote_bic")).toString().toLocal8Bit().constData());
    AB_Transaction_SetRemoteName(abTransaction,
                                 map.value(QStringLiteral("remote_name"))
                                     .toString()
                                     .toLocal8Bit()
                                     .constData());
    AB_Transaction_SetDate(abTransaction,
                           Private::fromDate(map.value(QStringLiteral("date")).toDate()).get());
    AB_Transaction_SetValutaDate(abTransaction,
                                 Private::fromDate(map.value(QStringLiteral("valuta_date")).toDate())
                                     .get());

    auto value = AB_Value_new();
    AB_Value_SetValueFromDouble(value, map.value(QStringLiteral("value")).toDouble());
    AB_Value_SetCurrency(value,
                         map.value(QStringLiteral("currency")).toString().toLocal8Bit().constData());
    AB_Transaction_SetValue(abTransaction, value);
    AB_Value_free(value);
    value = nullptr;

    value = AB_Value_new();
    AB_Value_SetValueFromDouble(value, map.value(QStringLiteral("fees")).toDouble());
    AB_Transaction_SetFees(abTransaction, value);
    AB_Value_free(value);
    value = nullptr;

    AB_Transaction_SetTransactionCode(abTransaction,
                                      map.value(QStringLiteral("transaction_code")).toInt());
    AB_Transaction_SetTransactionText(abTransaction,
                                      map.value(QStringLiteral("transaction_text"))
                                          .toString()
                                          .toLocal8Bit()
                                          .constData());
    AB_Transaction_SetTransactionKey(abTransaction,
                                     map.value(QStringLiteral("transaction_key"))
                                         .toString()
                                         .toLocal8Bit()
                                         .constData());
    AB_Transaction_SetTextKey(abTransaction, map.value(QStringLiteral("text_key")).toInt());
    AB_Transaction_SetPrimanota(
        abTransaction, map.value(QStringLiteral("primanota")).toString().toLocal8Bit().constData());
    AB_Transaction_SetPurpose(
        abTransaction, map.value(QStringLiteral("purpose")).toString().toLocal8Bit().constData());
    AB_Transaction_SetCategory(
        abTransaction, map.value(QStringLiteral("category")).toString().toLocal8Bit().constData());
    AB_Transaction_SetCustomerReference(abTransaction,
                                        map.value(QStringLiteral("customer_reference"))
                                            .toString()
                                            .toLocal8Bit()
                                            .constData());
    AB_Transaction_SetBankReference(abTransaction,
                                    map.value(QStringLiteral("bank_reference"))
                                        .toString()
                                        .toLocal8Bit()
                                        .constData());
    AB_Transaction_SetEndToEndReference(abTransaction,
                                        map.value(QStringLiteral("end_to_end_reference"))
                                            .toString()
                                            .toLocal8Bit()
                                            .constData());
    AB_Transaction_SetCreditorSchemeId(abTransaction,
                                       map.value(QStringLiteral("creditor_scheme_id"))
                                           .toString()
                                           .toLocal8Bit()
                                           .constData());
    AB_Transaction_SetOriginatorId(abTransaction,
                                   map.value(QStringLiteral("originator_id"))
                                       .toString()
                                       .toLocal8Bit()
                                       .constData());
    AB_Transaction_SetMandateId(
        abTransaction, map.value(QStringLiteral("mandate_id")).toString().toLocal8Bit().constData());
    AB_Transaction_SetMandateDate(abTransaction,
                                  Private::fromDate(
                                      map.value(QStringLiteral("mandate_date")).toDate())
                                      .get());
    AB_Transaction_SetMandateDebitorName(abTransaction,
                                         map.value(QStringLiteral("mandate_debitor_name"))
                                             .toString()
                                             .toLocal8Bit()
                                             .constData());
    AB_Transaction_SetOriginalCreditorSchemeId(abTransaction,
                                               map.value(
                                                      QStringLiteral("original_creditor_scheme_id"))
                                                   .toString()
                                                   .toLocal8Bit()
                                                   .constData());
    AB_Transaction_SetOriginalMandateId(abTransaction,
                                        map.value(QStringLiteral("original_mandate_id"))
                                            .toString()
                                            .toLocal8Bit()
                                            .constData());
    AB_Transaction_SetOriginalCreditorName(abTransaction,
                                           map.value(QStringLiteral("original_creditor_name"))
                                               .toString()
                                               .toLocal8Bit()
                                               .constData());
    AB_Transaction_SetSequence(abTransaction,
                               (TransactionSequence) map.value(QStringLiteral("sequence")).toInt());
    AB_Transaction_SetCharge(abTransaction,
                             (TransactionCharge) map.value(QStringLiteral("charge")).toInt());
    AB_Transaction_SetRemoteAddrStreet(abTransaction,
                                       map.value(QStringLiteral("remote_addr_street"))
                                           .toString()
                                           .toLocal8Bit()
                                           .constData());
    AB_Transaction_SetRemoteAddrZipcode(abTransaction,
                                        map.value(QStringLiteral("remote_addr_zipcode"))
                                            .toString()
                                            .toLocal8Bit()
                                            .constData());
    AB_Transaction_SetRemoteAddrCity(abTransaction,
                                     map.value(QStringLiteral("remote_addr_city"))
                                         .toString()
                                         .toLocal8Bit()
                                         .constData());
    AB_Transaction_SetRemoteAddrPhone(abTransaction,
                                      map.value(QStringLiteral("remote_addr_phone"))
                                          .toString()
                                          .toLocal8Bit()
                                          .constData());
    AB_Transaction_SetPeriod(abTransaction,
                             (TransactionPeriod) map.value(QStringLiteral("period")).toInt());
    AB_Transaction_SetCycle(abTransaction, map.value(QStringLiteral("cycle")).toUInt());
    AB_Transaction_SetExecutionDay(abTransaction,
                                   map.value(QStringLiteral("execution_day")).toUInt());
    AB_Transaction_SetFirstDate(abTransaction,
                                Private::fromDate(map.value(QStringLiteral("first_date")).toDate())
                                    .get());
    AB_Transaction_SetLastDate(abTransaction,
                               Private::fromDate(map.value(QStringLiteral("last_date")).toDate())
                                   .get());
    AB_Transaction_SetNextDate(abTransaction,
                               Private::fromDate(map.value(QStringLiteral("next_date")).toDate())
                                   .get());
    AB_Transaction_SetUnitId(
        abTransaction, map.value(QStringLiteral("unit_id")).toString().toLocal8Bit().constData());
    AB_Transaction_SetUnitIdNameSpace(abTransaction,
                                      map.value(QStringLiteral("unit_id_name_space"))
                                          .toString()
                                          .toLocal8Bit()
                                          .constData());
    AB_Transaction_SetTickerSymbol(abTransaction,
                                   map.value(QStringLiteral("ticker_symbol"))
                                       .toString()
                                       .toLocal8Bit()
                                       .constData());

    value = AB_Value_new();
    AB_Value_SetValueFromDouble(value, map.value(QStringLiteral("units")).toDouble());
    AB_Transaction_SetUnits(abTransaction, value);
    AB_Value_free(value);
    value = nullptr;

    value = AB_Value_new();
    AB_Value_SetValueFromDouble(value, map.value(QStringLiteral("unit_price_value")).toDouble());
    AB_Transaction_SetUnitPriceValue(abTransaction, value);
    AB_Value_free(value);
    value = nullptr;

    AB_Transaction_SetUnitPriceDate(abTransaction,
                                    Private::fromDate(
                                        map.value(QStringLiteral("unit_price_date")).toDate())
                                        .get());

    value = AB_Value_new();
    AB_Value_SetValueFromDouble(value, map.value(QStringLiteral("commission_value")).toDouble());
    AB_Transaction_SetCommissionValue(abTransaction, value);
    AB_Value_free(value);
    value = nullptr;

    AB_Transaction_SetMemo(abTransaction,
                           map.value(QStringLiteral("memo")).toString().toLocal8Bit().constData());
    AB_Transaction_SetHash(abTransaction,
                           map.value(QStringLiteral("hash")).toString().toLocal8Bit().constData());

    auto transaction = std::make_shared<Transaction>(abTransaction);

    AB_Transaction_free(abTransaction);
    abTransaction = nullptr;

    return transaction;
}

bool Transaction::isValid() const
{
    return type() != AB_Transaction_TypeUnknown && type() != AB_Transaction_TypeNone;
}

QString Transaction::toString() const
{
    // Deliberately without IBAN, account number or name, this ends up in the log.
    return QObject::tr("Transaction %1 - %2 %3")
        .arg(QString::number(uniqueId()), QString::number(value()), currency());
}

QMap<QString, QVariant> Transaction::toMap() const
{
    QMap<QString, QVariant> map = {};

    map[QStringLiteral("type")] = (qint32) type();
    map[QStringLiteral("sub_type")] = (qint32) subType();
    map[QStringLiteral("command")] = (qint32) command();
    map[QStringLiteral("status")] = (qint32) status();
    map[QStringLiteral("unique_account_id")] = uniqueAccountId();
    map[QStringLiteral("unique_id")] = uniqueId();
    map[QStringLiteral("ref_unique_id")] = refUniqueId();
    map[QStringLiteral("id_for_application")] = idForApplication();
    map[QStringLiteral("string_id_for_application")] = stringIdForApplication();
    map[QStringLiteral("session_id")] = sessionId();
    map[QStringLiteral("group_id")] = groupId();
    map[QStringLiteral("fi_id")] = fiId();
    map[QStringLiteral("local_iban")] = localIban();
    map[QStringLiteral("local_bic")] = localBic();
    map[QStringLiteral("local_country")] = localCountry();
    map[QStringLiteral("local_bank_code")] = localBankCode();
    map[QStringLiteral("local_branch_id")] = localBranchId();
    map[QStringLiteral("local_account_number")] = localAccountNumber();
    map[QStringLiteral("local_suffix")] = localSuffix();
    map[QStringLiteral("local_name")] = localName();
    map[QStringLiteral("remote_country")] = remoteCountry();
    map[QStringLiteral("remote_bank_code")] = remoteBankCode();
    map[QStringLiteral("remote_branch_id")] = remoteBranchId();
    map[QStringLiteral("remote_account_number")] = remoteAccountNumber();
    map[QStringLiteral("remote_suffix")] = remoteSuffix();
    map[QStringLiteral("remote_iban")] = remoteIban();
    map[QStringLiteral("remote_bic")] = remoteBic();
    map[QStringLiteral("remote_name")] = remoteName();
    map[QStringLiteral("date")] = date();
    map[QStringLiteral("valuta_date")] = valutaDate();
    map[QStringLiteral("value")] = value();
    map[QStringLiteral("currency")] = currency();
    map[QStringLiteral("fees")] = fees();
    map[QStringLiteral("transaction_code")] = transactionCode();
    map[QStringLiteral("transaction_text")] = transactionText();
    map[QStringLiteral("transaction_key")] = transactionKey();
    map[QStringLiteral("text_key")] = textKey();
    map[QStringLiteral("primanota")] = primanota();
    map[QStringLiteral("purpose")] = purpose();
    map[QStringLiteral("category")] = category();
    map[QStringLiteral("customer_reference")] = customerReference();
    map[QStringLiteral("bank_reference")] = bankReference();
    map[QStringLiteral("end_to_end_reference")] = endToEndReference();
    map[QStringLiteral("creditor_scheme_id")] = creditorSchemeId();
    map[QStringLiteral("originator_id")] = originatorId();
    map[QStringLiteral("mandate_id")] = mandateId();
    map[QStringLiteral("mandate_date")] = mandateDate();
    map[QStringLiteral("mandate_debitor_name")] = mandateDebitorName();
    map[QStringLiteral("original_creditor_scheme_id")] = originalCreditorSchemeId();
    map[QStringLiteral("original_mandate_id")] = originalMandateId();
    map[QStringLiteral("original_creditor_name")] = originalCreditorName();
    map[QStringLiteral("sequence")] = (qint32) sequence();
    map[QStringLiteral("charge")] = (qint32) charge();
    map[QStringLiteral("remote_addr_street")] = remoteAddrStreet();
    map[QStringLiteral("remote_addr_zipcode")] = remoteAddrZipcode();
    map[QStringLiteral("remote_addr_city")] = remoteAddrCity();
    map[QStringLiteral("remote_addr_phone")] = remoteAddrPhone();
    map[QStringLiteral("period")] = (qint32) period();
    map[QStringLiteral("cycle")] = cycle();
    map[QStringLiteral("execution_day")] = executionDay();
    map[QStringLiteral("first_date")] = firstDate();
    map[QStringLiteral("last_date")] = lastDate();
    map[QStringLiteral("next_date")] = nextDate();
    map[QStringLiteral("unit_id")] = unitId();
    map[QStringLiteral("unit_id_name_space")] = unitIdNameSpace();
    map[QStringLiteral("ticker_symbol")] = tickerSymbol();
    map[QStringLiteral("units")] = units();
    map[QStringLiteral("unit_price_value")] = unitPriceValue();
    map[QStringLiteral("unit_price_date")] = unitPriceDate();
    map[QStringLiteral("commission_value")] = commissionValue();
    map[QStringLiteral("memo")] = memo();
    map[QStringLiteral("hash")] = calculateTransactionHash();

    return map;
}

QString Transaction::itemType() const
{
    return QStringLiteral("Transaction");
}
