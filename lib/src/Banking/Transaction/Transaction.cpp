/**
 * Copyright (C) 2022-2025, Alexander Saal <developer@olbaflinx.chm-projects.de>
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

#include "Transaction.h"

#include <gwenhywfar/gwendate.h>

#include <QtCore/QCryptographicHash>
#include <QtCore/QIODevice>

using namespace olbaflinx::core::banking::transaction;

class Transaction::Private
{
public:
    explicit Private(Transaction *transaction, const AB_TRANSACTION *abTT)
        : abTransaction(AB_Transaction_dup(abTT ?: AB_Transaction_new()))
        , q_ptr(transaction)
    {}

    ~Private()
    {
        AB_Transaction_free(abTransaction);
        abTransaction = nullptr;
    }

    QString calculateTransactionHash()
    {
        if (!q_ptr->hash().isEmpty()) {
            return q_ptr->hash();
        }

        QByteArray buffer;
        QDataStream out(&buffer, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_DefaultCompiledVersion);
        out << q_ptr->endToEndReference();

        const QByteArray hash = QCryptographicHash::hash(buffer, QCryptographicHash::Sha1);

        buffer.clear();

        return {hash.toHex()};
    }

    static QDate toDate(const GWEN_DATE *gwenDate)
    {
        if (gwenDate) {
            auto buffer = GWEN_Buffer_new(Q_NULLPTR, 16, 0, 1);
            int rv = GWEN_Date_toStringWithTemplate(gwenDate, "DD.MM.YYYY HH:mm:ss", buffer);
            if (rv != GWEN_SUCCESS) {
                return QDate::currentDate();
            }

            auto start = GWEN_Buffer_GetStart(buffer);
            QDate qDate = QDate::fromString(QString(start), "dd.MM.yyyy HH:mm:ss");

            GWEN_Buffer_Reset(buffer);
            GWEN_Buffer_free(buffer);

            return qDate;
        }

        return QDate::currentDate();
    }

    static GWEN_DATE *fromDate(const QDate &date)
    {
        if (!date.isValid() || date.isNull()) {
            return GWEN_Date_CurrentDate();
        }

        const auto fd = date.toString(QString("yyyyMMdd")).toLocal8Bit();
        return GWEN_Date_fromString(fd.constData());
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

BankingItem *Transaction::create(QMap<QString, QVariant> &map) const
{
    auto abTransaction = AB_Transaction_new();

    AB_Transaction_SetType(abTransaction, (TransactionType) map["type"].toInt());
    AB_Transaction_SetSubType(abTransaction, (TransactionSubType) map["sub_type"].toInt());
    AB_Transaction_SetCommand(abTransaction, (TransactionCommand) map["command"].toInt());
    AB_Transaction_SetStatus(abTransaction, (TransactionStatus) map["status"].toInt());
    AB_Transaction_SetUniqueAccountId(abTransaction, map["unique_account_id"].toUInt());
    AB_Transaction_SetUniqueId(abTransaction, map["unique_id"].toUInt());
    AB_Transaction_SetRefUniqueId(abTransaction, map["ref_unique_id"].toUInt());
    AB_Transaction_SetIdForApplication(abTransaction, map["id_for_application"].toUInt());
    AB_Transaction_SetStringIdForApplication(
        abTransaction, map["string_id_for_application"].toString().toLocal8Bit().constData());
    AB_Transaction_SetSessionId(abTransaction, map["session_id"].toUInt());
    AB_Transaction_SetGroupId(abTransaction, map["group_id"].toUInt());
    AB_Transaction_SetFiId(abTransaction, map["fi_id"].toString().toLocal8Bit().constData());
    AB_Transaction_SetLocalIban(abTransaction,
                                map["local_iban"].toString().toLocal8Bit().constData());
    AB_Transaction_SetLocalBic(abTransaction, map["local_bic"].toString().toLocal8Bit().constData());
    AB_Transaction_SetLocalCountry(abTransaction,
                                   map["local_country"].toString().toLocal8Bit().constData());
    AB_Transaction_SetLocalBankCode(abTransaction,
                                    map["local_bank_code"].toString().toLocal8Bit().constData());
    AB_Transaction_SetLocalBranchId(abTransaction,
                                    map["local_branch_id"].toString().toLocal8Bit().constData());
    AB_Transaction_SetLocalAccountNumber(
        abTransaction, map["local_account_number"].toString().toLocal8Bit().constData());
    AB_Transaction_SetLocalSuffix(abTransaction,
                                  map["local_suffix"].toString().toLocal8Bit().constData());
    AB_Transaction_SetLocalName(abTransaction,
                                map["local_name"].toString().toLocal8Bit().constData());
    AB_Transaction_SetRemoteCountry(abTransaction,
                                    map["remote_country"].toString().toLocal8Bit().constData());
    AB_Transaction_SetRemoteBankCode(abTransaction,
                                     map["remote_bank_code"].toString().toLocal8Bit().constData());
    AB_Transaction_SetRemoteBranchId(abTransaction,
                                     map["remote_branch_id"].toString().toLocal8Bit().constData());
    AB_Transaction_SetRemoteAccountNumber(
        abTransaction, map["remote_account_number"].toString().toLocal8Bit().constData());
    AB_Transaction_SetRemoteSuffix(abTransaction,
                                   map["remote_suffix"].toString().toLocal8Bit().constData());
    AB_Transaction_SetRemoteIban(abTransaction,
                                 map["remote_iban"].toString().toLocal8Bit().constData());
    AB_Transaction_SetRemoteBic(abTransaction,
                                map["remote_bic"].toString().toLocal8Bit().constData());
    AB_Transaction_SetRemoteName(abTransaction,
                                 map["remote_name"].toString().toLocal8Bit().constData());
    AB_Transaction_SetDate(abTransaction, Private::fromDate(map["date"].toDate()));
    AB_Transaction_SetValutaDate(abTransaction, Private::fromDate(map["valuta_date"].toDate()));

    auto value = AB_Value_new();
    AB_Value_SetValueFromDouble(value, map["value"].toDouble());
    AB_Value_SetCurrency(value, map["currency"].toString().toLocal8Bit().constData());
    AB_Transaction_SetValue(abTransaction, AB_Value_dup(value));
    AB_Value_free(value);
    value = nullptr;

    value = AB_Value_new();
    AB_Value_SetValueFromDouble(value, map["fees"].toDouble());
    AB_Transaction_SetFees(abTransaction, AB_Value_dup(value));
    AB_Value_free(value);
    value = nullptr;

    AB_Transaction_SetTransactionCode(abTransaction, map["transaction_code"].toInt());
    AB_Transaction_SetTransactionText(abTransaction,
                                      map["transaction_text"].toString().toLocal8Bit().constData());
    AB_Transaction_SetTransactionKey(abTransaction,
                                     map["transaction_key"].toString().toLocal8Bit().constData());
    AB_Transaction_SetTextKey(abTransaction, map["text_key"].toInt());
    AB_Transaction_SetPrimanota(abTransaction,
                                map["primanota"].toString().toLocal8Bit().constData());
    AB_Transaction_SetPurpose(abTransaction, map["purpose"].toString().toLocal8Bit().constData());
    AB_Transaction_SetCategory(abTransaction, map["category"].toString().toLocal8Bit().constData());
    AB_Transaction_SetCustomerReference(
        abTransaction, map["customer_reference"].toString().toLocal8Bit().constData());
    AB_Transaction_SetBankReference(abTransaction,
                                    map["bank_reference"].toString().toLocal8Bit().constData());
    AB_Transaction_SetEndToEndReference(
        abTransaction, map["end_to_end_reference"].toString().toLocal8Bit().constData());
    AB_Transaction_SetCreditorSchemeId(
        abTransaction, map["creditor_scheme_id"].toString().toLocal8Bit().constData());
    AB_Transaction_SetOriginatorId(abTransaction,
                                   map["originator_id"].toString().toLocal8Bit().constData());
    AB_Transaction_SetMandateId(abTransaction,
                                map["mandate_id"].toString().toLocal8Bit().constData());
    AB_Transaction_SetMandateDate(abTransaction, Private::fromDate(map["mandate_date"].toDate()));
    AB_Transaction_SetMandateDebitorName(
        abTransaction, map["mandate_debitor_name"].toString().toLocal8Bit().constData());
    AB_Transaction_SetOriginalCreditorSchemeId(
        abTransaction, map["original_creditor_scheme_id"].toString().toLocal8Bit().constData());
    AB_Transaction_SetOriginalMandateId(
        abTransaction, map["original_mandate_id"].toString().toLocal8Bit().constData());
    AB_Transaction_SetOriginalCreditorName(
        abTransaction, map["original_creditor_name"].toString().toLocal8Bit().constData());
    AB_Transaction_SetSequence(abTransaction, (TransactionSequence) map["sequence"].toInt());
    AB_Transaction_SetCharge(abTransaction, (TransactionCharge) map["charge"].toInt());
    AB_Transaction_SetRemoteAddrStreet(
        abTransaction, map["remote_addr_street"].toString().toLocal8Bit().constData());
    AB_Transaction_SetRemoteAddrZipcode(
        abTransaction, map["remote_addr_zipcode"].toString().toLocal8Bit().constData());
    AB_Transaction_SetRemoteAddrCity(abTransaction,
                                     map["remote_addr_city"].toString().toLocal8Bit().constData());
    AB_Transaction_SetRemoteAddrPhone(abTransaction,
                                      map["remote_addr_phone"].toString().toLocal8Bit().constData());
    AB_Transaction_SetPeriod(abTransaction, (TransactionPeriod) map["period"].toInt());
    AB_Transaction_SetCycle(abTransaction, map["cycle"].toUInt());
    AB_Transaction_SetExecutionDay(abTransaction, map["execution_day"].toUInt());
    AB_Transaction_SetFirstDate(abTransaction, Private::fromDate(map["first_date"].toDate()));
    AB_Transaction_SetLastDate(abTransaction, Private::fromDate(map["last_date"].toDate()));
    AB_Transaction_SetNextDate(abTransaction, Private::fromDate(map["next_date"].toDate()));
    AB_Transaction_SetUnitId(abTransaction, map["unit_id"].toString().toLocal8Bit().constData());
    AB_Transaction_SetUnitIdNameSpace(abTransaction,
                                      map["unit_id_name_space"].toString().toLocal8Bit().constData());
    AB_Transaction_SetTickerSymbol(abTransaction,
                                   map["ticker_symbol"].toString().toLocal8Bit().constData());

    value = AB_Value_new();
    AB_Value_SetValueFromDouble(value, map["units"].toDouble());
    AB_Transaction_SetUnits(abTransaction, AB_Value_dup(value));
    AB_Value_free(value);
    value = nullptr;

    value = AB_Value_new();
    AB_Value_SetValueFromDouble(value, map["unit_price_value"].toDouble());
    AB_Transaction_SetUnitPriceValue(abTransaction, AB_Value_dup(value));
    AB_Value_free(value);
    value = nullptr;

    AB_Transaction_SetUnitPriceDate(abTransaction, Private::fromDate(map["unit_price_date"].toDate()));

    value = AB_Value_new();
    AB_Value_SetValueFromDouble(value, map["commission_value"].toDouble());
    AB_Transaction_SetCommissionValue(abTransaction, AB_Value_dup(value));
    AB_Value_free(value);
    value = nullptr;

    AB_Transaction_SetMemo(abTransaction, map["memo"].toString().toLocal8Bit().constData());
    AB_Transaction_SetHash(abTransaction, map["hash"].toString().toLocal8Bit().constData());

    const auto transaction = new Transaction(abTransaction);

    AB_Transaction_free(abTransaction);
    abTransaction = nullptr;

    return transaction;
}

bool Transaction::isValid() const {}

QString Transaction::toString() const {}

QMap<QString, QVariant> Transaction::toMap() const
{
    QMap<QString, QVariant> map = {};

    map[":type"] = (qint32) type();
    map[":sub_type"] = (qint32) subType();
    map[":command"] = (qint32) command();
    map[":status"] = (qint32) status();
    map[":unique_account_id"] = uniqueAccountId();
    map[":unique_id"] = uniqueId();
    map[":ref_unique_id"] = refUniqueId();
    map[":id_for_application"] = idForApplication();
    map[":string_id_for_application"] = stringIdForApplication();
    map[":session_id"] = sessionId();
    map[":group_id"] = groupId();
    map[":fi_id"] = fiId();
    map[":local_iban"] = localIban();
    map[":local_bic"] = localBic();
    map[":local_country"] = localCountry();
    map[":local_bank_code"] = localBankCode();
    map[":local_branch_id"] = localBranchId();
    map[":local_account_number"] = localAccountNumber();
    map[":local_suffix"] = localSuffix();
    map[":local_name"] = localName();
    map[":remote_country"] = remoteCountry();
    map[":remote_bank_code"] = remoteBankCode();
    map[":remote_branch_id"] = remoteBranchId();
    map[":remote_account_number"] = remoteAccountNumber();
    map[":remote_suffix"] = remoteSuffix();
    map[":remote_iban"] = remoteIban();
    map[":remote_bic"] = remoteBic();
    map[":remote_name"] = remoteName();
    map[":date"] = date();
    map[":valuta_date"] = valutaDate();
    map[":value"] = value();
    map[":currency"] = currency();
    map[":fees"] = fees();
    map[":transaction_code"] = transactionCode();
    map[":transaction_text"] = transactionText();
    map[":transaction_key"] = transactionKey();
    map[":text_key"] = textKey();
    map[":primanota"] = primanota();
    map[":purpose"] = purpose();
    map[":category"] = category();
    map[":customer_reference"] = customerReference();
    map[":bank_reference"] = bankReference();
    map[":end_to_end_reference"] = endToEndReference();
    map[":creditor_scheme_id"] = creditorSchemeId();
    map[":originator_id"] = originatorId();
    map[":mandate_id"] = mandateId();
    map[":mandate_date"] = mandateDate();
    map[":mandate_debitor_name"] = mandateDebitorName();
    map[":original_creditor_scheme_id"] = originalCreditorSchemeId();
    map[":original_mandate_id"] = originalMandateId();
    map[":original_creditor_name"] = originalCreditorName();
    map[":sequence"] = (qint32) sequence();
    map[":charge"] = (qint32) charge();
    map[":remote_addr_street"] = remoteAddrStreet();
    map[":remote_addr_zipcode"] = remoteAddrZipcode();
    map[":remote_addr_city"] = remoteAddrCity();
    map[":remote_addr_phone"] = remoteAddrPhone();
    map[":period"] = (qint32) period();
    map[":cycle"] = cycle();
    map[":execution_day"] = executionDay();
    map[":first_date"] = firstDate();
    map[":last_date"] = lastDate();
    map[":next_date"] = nextDate();
    map[":unit_id"] = unitId();
    map[":unit_id_name_space"] = unitIdNameSpace();
    map[":ticker_symbol"] = tickerSymbol();
    map[":units"] = units();
    map[":unit_price_value"] = unitPriceValue();
    map[":unit_price_date"] = unitPriceDate();
    map[":commission_value"] = commissionValue();
    map[":memo"] = memo();
    map[":hash"] = calculateTransactionHash();

    return map;
}

QString Transaction::itemType() const
{
    return {"Transaction"};
}
