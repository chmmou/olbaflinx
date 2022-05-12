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

#include <QtCore/QCryptographicHash>
#include <QtCore/QVariant>
#include <QtSql/QSqlField>
#include <QtSql/QSqlRecord>

#include "core/Container.h"
#include "Transaction.h"

using namespace olbaflinx::core::storage::transaction;

Transaction::Transaction(const AB_TRANSACTION *transaction)
    : abTransaction(AB_Transaction_dup(transaction))
{

}

Transaction::~Transaction()
{
    AB_Transaction_free(abTransaction);
}

QDate Transaction::gwenDateToQDate(const GWEN_DATE *gwenDate) const
{
    if (gwenDate) {
        GWEN_BUFFER *buffer = GWEN_Buffer_new(nullptr, 16, 0, 1);
        const int rv = GWEN_Date_toStringWithTemplate(gwenDate, "DD.MM.YYYY HH:mm:ss", buffer);
        if (rv == 0) {
            char *start = GWEN_Buffer_GetStart(buffer);
            QDate qDate = QDate::fromString(QString(start), "dd.MM.yyyy HH:mm:ss");

            GWEN_Buffer_Reset(buffer);
            GWEN_Buffer_free(buffer);

            return qDate;
        }
    }

    return QDate::currentDate();
}

GWEN_DATE *Transaction::qDateToGwenDate(const QDate &qDate)
{
    if (qDate.isValid()) {
        const auto dateString = qDate.toString("YYYY-DD-MM HH:mm::ss");
        return GWEN_Date_fromString(dateString.toLocal8Bit().constData());
    }

    return nullptr;
}

TransactionType Transaction::type() const
{
    return AB_Transaction_GetType(abTransaction);
}

TransactionSubType Transaction::subType() const
{
    return AB_Transaction_GetSubType(abTransaction);
}

TransactionCommand Transaction::command() const
{
    return AB_Transaction_GetCommand(abTransaction);
}

TransactionStatus Transaction::status() const
{
    return AB_Transaction_GetStatus(abTransaction);
}

quint32 Transaction::uniqueAccountId() const
{
    return AB_Transaction_GetUniqueAccountId(abTransaction);
}

quint32 Transaction::uniqueId() const
{
    return AB_Transaction_GetUniqueId(abTransaction);
}

quint32 Transaction::refUniqueId() const
{
    return AB_Transaction_GetRefUniqueId(abTransaction);
}

quint32 Transaction::idForApplication() const
{
    return AB_Transaction_GetIdForApplication(abTransaction);
}

QString Transaction::stringIdForApplication() const
{
    return QString::fromUtf8(AB_Transaction_GetStringIdForApplication(abTransaction));
}

quint32 Transaction::sessionId() const
{
    return AB_Transaction_GetSessionId(abTransaction);
}

quint32 Transaction::groupId() const
{
    return AB_Transaction_GetGroupId(abTransaction);
}

QString Transaction::fiId() const
{
    return QString::fromUtf8(AB_Transaction_GetFiId(abTransaction));
}

QString Transaction::localIban() const
{
    return QString::fromUtf8(AB_Transaction_GetLocalIban(abTransaction));
}

QString Transaction::localBic() const
{
    return QString::fromUtf8(AB_Transaction_GetLocalBic(abTransaction));
}

QString Transaction::localCountry() const
{
    return QString::fromUtf8(AB_Transaction_GetLocalCountry(abTransaction));
}

QString Transaction::localBankCode() const
{
    return QString::fromUtf8(AB_Transaction_GetLocalBankCode(abTransaction));
}

QString Transaction::localBranchId() const
{
    return QString::fromUtf8(AB_Transaction_GetLocalBranchId(abTransaction));
}

QString Transaction::localAccountNumber() const
{
    return QString::fromUtf8(AB_Transaction_GetLocalAccountNumber(abTransaction));
}

QString Transaction::localSuffix() const
{
    return QString::fromUtf8(AB_Transaction_GetLocalSuffix(abTransaction));
}

QString Transaction::localName() const
{
    return QString::fromUtf8(AB_Transaction_GetLocalName(abTransaction));
}

QString Transaction::remoteCountry() const
{
    return QString::fromUtf8(AB_Transaction_GetRemoteCountry(abTransaction));
}

QString Transaction::remoteBankCode() const
{
    return QString::fromUtf8(AB_Transaction_GetRemoteBankCode(abTransaction));
}

QString Transaction::remoteBranchId() const
{
    return QString::fromUtf8(AB_Transaction_GetRemoteBranchId(abTransaction));
}

QString Transaction::remoteAccountNumber() const
{
    return QString::fromUtf8(AB_Transaction_GetRemoteAccountNumber(abTransaction));
}

QString Transaction::remoteSuffix() const
{
    return QString::fromUtf8(AB_Transaction_GetRemoteSuffix(abTransaction));
}

QString Transaction::remoteIban() const
{
    return QString::fromUtf8(AB_Transaction_GetRemoteIban(abTransaction));
}

QString Transaction::remoteBic() const
{
    return QString::fromUtf8(AB_Transaction_GetRemoteBic(abTransaction));
}

QString Transaction::remoteName() const
{
    return QString::fromUtf8(AB_Transaction_GetRemoteName(abTransaction));
}

QDate Transaction::date() const
{
    return gwenDateToQDate(AB_Transaction_GetDate(abTransaction));
}

QDate Transaction::valutaDate() const
{
    return gwenDateToQDate(AB_Transaction_GetValutaDate(abTransaction));
}

qreal Transaction::value() const
{
    const auto value = AB_Transaction_GetValue(abTransaction);
    if (value == nullptr) {
        return 0.0;
    }

    return AB_Value_GetValueAsDouble(value);
}

QString Transaction::currency() const
{
    return QString::fromUtf8(AB_Value_GetCurrency(AB_Transaction_GetValue(abTransaction)));
}

qreal Transaction::fees() const
{
    const auto fees = AB_Transaction_GetFees(abTransaction);
    if (fees == nullptr) {
        return 0.0;
    }

    return AB_Value_GetValueAsDouble(fees);
}

int Transaction::transactionCode() const
{
    return AB_Transaction_GetTransactionCode(abTransaction);
}

QString Transaction::transactionText() const
{
    return QString::fromUtf8(AB_Transaction_GetTransactionText(abTransaction));
}

QString Transaction::transactionKey() const
{
    return QString::fromUtf8(AB_Transaction_GetTransactionKey(abTransaction));
}

int Transaction::textKey() const
{
    return AB_Transaction_GetTextKey(abTransaction);
}

QString Transaction::primanota() const
{
    return QString::fromUtf8(AB_Transaction_GetPrimanota(abTransaction));
}

QString Transaction::purpose() const
{
    return QString::fromUtf8(AB_Transaction_GetPurpose(abTransaction));
}

QString Transaction::category() const
{
    return QString::fromUtf8(AB_Transaction_GetCategory(abTransaction));
}

QString Transaction::customerReference() const
{
    return QString::fromUtf8(AB_Transaction_GetCustomerReference(abTransaction));
}

QString Transaction::bankReference() const
{
    return QString::fromUtf8(AB_Transaction_GetBankReference(abTransaction));
}

QString Transaction::endToEndReference() const
{
    return QString::fromUtf8(AB_Transaction_GetEndToEndReference(abTransaction));
}

QString Transaction::creditorSchemeId() const
{
    return QString::fromUtf8(AB_Transaction_GetCreditorSchemeId(abTransaction));
}

QString Transaction::originatorId() const
{
    return QString::fromUtf8(AB_Transaction_GetOriginatorId(abTransaction));
}

QString Transaction::mandateId() const
{
    return QString::fromUtf8(AB_Transaction_GetMandateId(abTransaction));
}

QDate Transaction::mandateDate() const
{
    return gwenDateToQDate(AB_Transaction_GetMandateDate(abTransaction));
}

QString Transaction::mandateDebitorName() const
{
    return QString::fromUtf8(AB_Transaction_GetMandateDebitorName(abTransaction));
}

QString Transaction::originalCreditorSchemeId() const
{
    return QString::fromUtf8(AB_Transaction_GetOriginalCreditorSchemeId(abTransaction));
}

QString Transaction::originalMandateId() const
{
    return QString::fromUtf8(AB_Transaction_GetOriginalMandateId(abTransaction));
}

QString Transaction::originalCreditorName() const
{
    return QString::fromUtf8(AB_Transaction_GetOriginalCreditorName(abTransaction));
}

TransactionSequence Transaction::sequence() const
{
    return AB_Transaction_GetSequence(abTransaction);
}

TransactionCharge Transaction::charge() const
{
    return AB_Transaction_GetCharge(abTransaction);
}

QString Transaction::remoteAddrStreet() const
{
    return QString::fromUtf8(AB_Transaction_GetRemoteAddrStreet(abTransaction));
}

QString Transaction::remoteAddrZipcode() const
{
    return QString::fromUtf8(AB_Transaction_GetRemoteAddrZipcode(abTransaction));
}

QString Transaction::remoteAddrCity() const
{
    return QString::fromUtf8(AB_Transaction_GetRemoteAddrCity(abTransaction));
}

QString Transaction::remoteAddrPhone() const
{
    return QString::fromUtf8(AB_Transaction_GetRemoteAddrPhone(abTransaction));
}

TransactionPeriod Transaction::period() const
{
    return AB_Transaction_GetPeriod(abTransaction);
}

quint32 Transaction::cycle() const
{
    return AB_Transaction_GetCycle(abTransaction);
}

quint32 Transaction::executionDay() const
{
    return AB_Transaction_GetExecutionDay(abTransaction);
}

QDate Transaction::firstDate() const
{
    return gwenDateToQDate(AB_Transaction_GetFirstDate(abTransaction));
}

QDate Transaction::lastDate() const
{
    return gwenDateToQDate(AB_Transaction_GetLastDate(abTransaction));
}

QDate Transaction::nextDate() const
{
    return gwenDateToQDate(AB_Transaction_GetNextDate(abTransaction));
}

QString Transaction::unitId() const
{
    return QString::fromUtf8(AB_Transaction_GetUnitId(abTransaction));
}

QString Transaction::unitIdNameSpace() const
{
    return QString::fromUtf8(AB_Transaction_GetUnitIdNameSpace(abTransaction));
}

QString Transaction::tickerSymbol() const
{
    return QString::fromUtf8(AB_Transaction_GetTickerSymbol(abTransaction));
}

qreal Transaction::units() const
{
    const auto units = AB_Transaction_GetUnits(abTransaction);
    if (units == nullptr) {
        return 0.0;
    }

    return AB_Value_GetValueAsDouble(units);
}

qreal Transaction::unitPriceValue() const
{
    const auto unitPriceValue = AB_Transaction_GetUnitPriceValue(abTransaction);
    if (unitPriceValue == nullptr) {
        return 0.0;
    }

    return AB_Value_GetValueAsDouble(unitPriceValue);
}

QDate Transaction::unitPriceDate() const
{
    return gwenDateToQDate(AB_Transaction_GetUnitPriceDate(abTransaction));
}

qreal Transaction::commissionValue() const
{
    const auto commissionValue = AB_Transaction_GetCommissionValue(abTransaction);
    if (commissionValue == nullptr) {
        return 0.0;
    }

    return AB_Value_GetValueAsDouble(commissionValue);
}

QString Transaction::memo() const
{
    return QString::fromUtf8(AB_Transaction_GetMemo(abTransaction));
}

QString Transaction::hash() const
{
    return QString::fromUtf8(AB_Transaction_GetHash(abTransaction));
}

QString Transaction::toString() const
{
    return "";
}

bool Transaction::isStandingOrder() const
{
    return type() == AB_Transaction_TypeStandingOrder;
}

QString Transaction::calculateTransactionHash() const
{
    if (!hash().isEmpty()) {
        return hash();
    }

    const QString purpose = this->purpose().append(QDateTime::currentDateTime().toString(" - dd.MM.yyyy hh:mm:ss.z"));
    const QByteArray hash = QCryptographicHash::hash(purpose.toUtf8(), QCryptographicHash::Sha1);
    return QString("%1").arg(QString(hash.toHex()));
}

QSqlQuery Transaction::createInsertQuery(const quint32 &accountId, QSqlQuery &query) const
{
    query.prepare(StorageSqlTransactionInsertQuery);
    query.bindValue(":account_id", accountId);
    query.bindValue(":type", (qint32) type());
    query.bindValue(":sub_type", (qint32) subType());
    query.bindValue(":command", (qint32) command());
    query.bindValue(":status", (qint32) status());
    query.bindValue(":unique_account_id", uniqueAccountId());
    query.bindValue(":unique_id", uniqueId());
    query.bindValue(":ref_unique_id", refUniqueId());
    query.bindValue(":id_for_application", idForApplication());
    query.bindValue(":string_id_for_application", stringIdForApplication());
    query.bindValue(":session_id", sessionId());
    query.bindValue(":group_id", groupId());
    query.bindValue(":fi_id", fiId());
    query.bindValue(":local_iban", localIban());
    query.bindValue(":local_bic", localBic());
    query.bindValue(":local_country", localCountry());
    query.bindValue(":local_bank_code", localBankCode());
    query.bindValue(":local_branch_id", localBranchId());
    query.bindValue(":local_account_number", localAccountNumber());
    query.bindValue(":local_suffix", localSuffix());
    query.bindValue(":local_name", localName());
    query.bindValue(":remote_country", remoteCountry());
    query.bindValue(":remote_bank_code", remoteBankCode());
    query.bindValue(":remote_branch_id", remoteBranchId());
    query.bindValue(":remote_account_number", remoteAccountNumber());
    query.bindValue(":remote_suffix", remoteSuffix());
    query.bindValue(":remote_iban", remoteIban());
    query.bindValue(":remote_bic", remoteBic());
    query.bindValue(":remote_name", remoteName());
    query.bindValue(":date", date());
    query.bindValue(":valuta_date", valutaDate());
    query.bindValue(":value", value());
    query.bindValue(":currency", currency());
    query.bindValue(":fees", fees());
    query.bindValue(":transaction_code", transactionCode());
    query.bindValue(":transaction_text", transactionText());
    query.bindValue(":transaction_key", transactionKey());
    query.bindValue(":text_key", textKey());
    query.bindValue(":primanota", primanota());
    query.bindValue(":purpose", purpose());
    query.bindValue(":category", category());
    query.bindValue(":customer_reference", customerReference());
    query.bindValue(":bank_reference", bankReference());
    query.bindValue(":end_to_end_reference", endToEndReference());
    query.bindValue(":creditor_scheme_id", creditorSchemeId());
    query.bindValue(":originator_id", originatorId());
    query.bindValue(":mandate_id", mandateId());
    query.bindValue(":mandate_date", mandateDate());
    query.bindValue(":mandate_debitor_name", mandateDebitorName());
    query.bindValue(":original_creditor_scheme_id", originalCreditorSchemeId());
    query.bindValue(":original_mandate_id", originalMandateId());
    query.bindValue(":original_creditor_name", originalCreditorName());
    query.bindValue(":sequence", (qint32) sequence());
    query.bindValue(":charge", (qint32) charge());
    query.bindValue(":remote_addr_street", remoteAddrStreet());
    query.bindValue(":remote_addr_zipcode", remoteAddrZipcode());
    query.bindValue(":remote_addr_city", remoteAddrCity());
    query.bindValue(":remote_addr_phone", remoteAddrPhone());
    query.bindValue(":period", (qint32) period());
    query.bindValue(":cycle", cycle());
    query.bindValue(":execution_day", executionDay());
    query.bindValue(":first_date", firstDate());
    query.bindValue(":last_date", lastDate());
    query.bindValue(":next_date", nextDate());
    query.bindValue(":unit_id", unitId());
    query.bindValue(":unit_id_name_space", unitIdNameSpace());
    query.bindValue(":ticker_symbol", tickerSymbol());
    query.bindValue(":units", units());
    query.bindValue(":unit_price_value", unitPriceValue());
    query.bindValue(":unit_price_date", unitPriceDate());
    query.bindValue(":commission_value", commissionValue());
    query.bindValue(":memo", memo());
    query.bindValue(":hash", calculateTransactionHash());

    return query;
}

QMap<QString, QVariant> Transaction::transactionQueryToMap(const QSqlQuery &query)
{
    QMap<QString, QVariant> map = QMap<QString, QVariant>();

    map["accountId"] = query.record().field("account_id").value();
    map["type"] = query.record().field("type").value();
    map["subType"] = query.record().field("sub_type").value();
    map["command"] = query.record().field("command").value();
    map["status"] = query.record().field("status").value();
    map["uniqueAccountId"] = query.record().field("unique_account_id").value();
    map["uniqueId"] = query.record().field("unique_id").value();
    map["refUniqueId"] = query.record().field("ref_unique_id").value();
    map["idForApplication"] = query.record().field("id_for_application").value();
    map["stringIdForApplication"] = query.record().field("string_id_for_application").value();
    map["sessionId"] = query.record().field("session_id").value();
    map["groupId"] = query.record().field("group_id").value();
    map["fiId"] = query.record().field("fi_id").value();
    map["localIban"] = query.record().field("local_iban").value();
    map["localBic"] = query.record().field("local_bic").value();
    map["localCountry"] = query.record().field("local_country").value();
    map["localBankCode"] = query.record().field("local_bank_code").value();
    map["localBranchId"] = query.record().field("local_branch_id").value();
    map["localAccountNumber"] = query.record().field("local_account_number").value();
    map["localSuffix"] = query.record().field("local_suffix").value();
    map["localName"] = query.record().field("local_name").value();
    map["remoteCountry"] = query.record().field("remote_country").value();
    map["remoteBankCode"] = query.record().field("remote_bank_code").value();
    map["remoteBranchId"] = query.record().field("remote_branch_id").value();
    map["remoteAccountNumber"] = query.record().field("remote_account_number").value();
    map["remoteSuffix"] = query.record().field("remote_suffix").value();
    map["remoteIban"] = query.record().field("remote_iban").value();
    map["remoteBic"] = query.record().field("remote_bic").value();
    map["remoteName"] = query.record().field("remote_name").value();
    map["date"] = query.record().field("date").value();
    map["valutaDate"] = query.record().field("valuta_date").value();
    map["value"] = query.record().field("value").value();
    map["currency"] = query.record().field("currency").value();
    map["fees"] = query.record().field("fees").value();
    map["transactionCode"] = query.record().field("transaction_code").value();
    map["transactionText"] = query.record().field("transaction_text").value();
    map["transactionKey"] = query.record().field("transaction_key").value();
    map["textKey"] = query.record().field("text_key").value();
    map["primanota"] = query.record().field("primanota").value();
    map["purpose"] = query.record().field("purpose").value();
    map["category"] = query.record().field("category").value();
    map["customerReference"] = query.record().field("customer_reference").value();
    map["bankReference"] = query.record().field("bank_reference").value();
    map["endToEndReference"] = query.record().field("end_to_end_reference").value();
    map["creditorSchemeId"] = query.record().field("creditor_scheme_id").value();
    map["originatorId"] = query.record().field("originator_id").value();
    map["mandateId"] = query.record().field("mandate_id").value();
    map["mandateDate"] = query.record().field("mandate_date").value();
    map["mandateDebitorName"] = query.record().field("mandate_debitor_name").value();
    map["originalCreditorSchemeId"] = query.record().field("original_creditor_scheme_id").value();
    map["originalMandateId"] = query.record().field("original_mandate_id").value();
    map["originalCreditorName"] = query.record().field("original_creditor_name").value();
    map["sequence"] = query.record().field("sequence").value();
    map["charge"] = query.record().field("charge").value();
    map["remoteAddrStreet"] = query.record().field("remote_addr_street").value();
    map["remoteAddrZipcode"] = query.record().field("remote_addr_zipcode").value();
    map["remoteAddrCity"] = query.record().field("remote_addr_city").value();
    map["remoteAddrPhone"] = query.record().field("remote_addr_phone").value();
    map["period"] = query.record().field("period").value();
    map["cycle"] = query.record().field("cycle").value();
    map["executionDay"] = query.record().field("execution_day").value();
    map["firstDate"] = query.record().field("first_date").value();
    map["lastDate"] = query.record().field("last_date").value();
    map["nextDate"] = query.record().field("next_date").value();
    map["unitId"] = query.record().field("unit_id").value();
    map["unitIdNameSpace"] = query.record().field("unit_id_name_space").value();
    map["tickerSymbol"] = query.record().field("ticker_symbol").value();
    map["units"] = query.record().field("units").value();
    map["unitPriceValue"] = query.record().field("unit_price_value").value();
    map["unitPriceDate"] = query.record().field("unit_price_date").value();
    map["commissionValue"] = query.record().field("commission_value").value();
    map["memo"] = query.record().field("memo").value();
    map["hash"] = query.record().field("hash").value();

    return map;
}

Transaction *Transaction::create(const QMap<QString, QVariant> &row)
{
    auto abTransaction = AB_Transaction_new();

    AB_Transaction_SetType(abTransaction, (TransactionType) row["type"].toInt());
    AB_Transaction_SetSubType(abTransaction, (TransactionSubType) row["subType"].toInt());
    AB_Transaction_SetCommand(abTransaction, (TransactionCommand) row["command"].toInt());
    AB_Transaction_SetStatus(abTransaction, (TransactionStatus) row["status"].toInt());
    AB_Transaction_SetUniqueAccountId(abTransaction, row["uniqueAccountId"].toUInt());
    AB_Transaction_SetUniqueId(abTransaction, row["uniqueId"].toUInt());
    AB_Transaction_SetRefUniqueId(abTransaction, row["refUniqueId"].toUInt());
    AB_Transaction_SetIdForApplication(abTransaction, row["idForApplication"].toUInt());
    AB_Transaction_SetStringIdForApplication(abTransaction, row["stringIdForApplication"].toString().toLocal8Bit().constData());
    AB_Transaction_SetSessionId(abTransaction, row["sessionId"].toUInt());
    AB_Transaction_SetGroupId(abTransaction, row["groupId"].toUInt());
    AB_Transaction_SetFiId(abTransaction, row["fiId"].toString().toLocal8Bit().constData());
    AB_Transaction_SetLocalIban(abTransaction, row["localIban"].toString().toLocal8Bit().constData());
    AB_Transaction_SetLocalBic(abTransaction, row["localBic"].toString().toLocal8Bit().constData());
    AB_Transaction_SetLocalCountry(abTransaction, row["localCountry"].toString().toLocal8Bit().constData());
    AB_Transaction_SetLocalBankCode(abTransaction, row["localBankCode"].toString().toLocal8Bit().constData());
    AB_Transaction_SetLocalBranchId(abTransaction, row["localBranchId"].toString().toLocal8Bit().constData());
    AB_Transaction_SetLocalAccountNumber(abTransaction, row["localAccountNumber"].toString().toLocal8Bit().constData());
    AB_Transaction_SetLocalSuffix(abTransaction, row["localSuffix"].toString().toLocal8Bit().constData());
    AB_Transaction_SetLocalName(abTransaction, row["localName"].toString().toLocal8Bit().constData());
    AB_Transaction_SetRemoteCountry(abTransaction, row["remoteCountry"].toString().toLocal8Bit().constData());
    AB_Transaction_SetRemoteBankCode(abTransaction, row["remoteBankCode"].toString().toLocal8Bit().constData());
    AB_Transaction_SetRemoteBranchId(abTransaction, row["remoteBranchId"].toString().toLocal8Bit().constData());
    AB_Transaction_SetRemoteAccountNumber(abTransaction, row["remoteAccountNumber"].toString().toLocal8Bit().constData());
    AB_Transaction_SetRemoteSuffix(abTransaction, row["remoteSuffix"].toString().toLocal8Bit().constData());
    AB_Transaction_SetRemoteIban(abTransaction, row["remoteIban"].toString().toLocal8Bit().constData());
    AB_Transaction_SetRemoteBic(abTransaction, row["remoteBic"].toString().toLocal8Bit().constData());
    AB_Transaction_SetRemoteName(abTransaction, row["remoteName"].toString().toLocal8Bit().constData());
    AB_Transaction_SetDate(abTransaction, Transaction::qDateToGwenDate(row["date"].toDate()));
    AB_Transaction_SetValutaDate(abTransaction, Transaction::qDateToGwenDate(row["valutaDate"].toDate()));

    auto value = AB_Value_new();
    AB_Value_SetValueFromDouble(value, row["value"].toDouble());
    AB_Value_SetCurrency(value, row["currency"].toString().toLocal8Bit().constData());
    AB_Transaction_SetValue(abTransaction, AB_Value_dup(value));
    AB_Value_free(value);
    value = nullptr;

    value = AB_Value_new();
    AB_Value_SetValueFromDouble(value, row["fees"].toDouble());
    AB_Transaction_SetFees(abTransaction, AB_Value_dup(value));
    AB_Value_free(value);
    value = nullptr;

    AB_Transaction_SetTransactionCode(abTransaction, row["transactionCode"].toInt());
    AB_Transaction_SetTransactionText(abTransaction, row["transactionText"].toString().toLocal8Bit().constData());
    AB_Transaction_SetTransactionKey(abTransaction, row["transactionKey"].toString().toLocal8Bit().constData());
    AB_Transaction_SetTextKey(abTransaction, row["textKey"].toInt());
    AB_Transaction_SetPrimanota(abTransaction, row["primanota"].toString().toLocal8Bit().constData());
    AB_Transaction_SetPurpose(abTransaction, row["purpose"].toString().toLocal8Bit().constData());
    AB_Transaction_SetCategory(abTransaction, row["category"].toString().toLocal8Bit().constData());
    AB_Transaction_SetCustomerReference(abTransaction, row["customerReference"].toString().toLocal8Bit().constData());
    AB_Transaction_SetBankReference(abTransaction, row["bankReference"].toString().toLocal8Bit().constData());
    AB_Transaction_SetEndToEndReference(abTransaction, row["endToEndReference"].toString().toLocal8Bit().constData());
    AB_Transaction_SetCreditorSchemeId(abTransaction, row["creditorSchemeId"].toString().toLocal8Bit().constData());
    AB_Transaction_SetOriginatorId(abTransaction, row["originatorId"].toString().toLocal8Bit().constData());
    AB_Transaction_SetMandateId(abTransaction, row["mandateId"].toString().toLocal8Bit().constData());
    AB_Transaction_SetMandateDate(abTransaction, Transaction::qDateToGwenDate(row["mandateDate"].toDate()));
    AB_Transaction_SetMandateDebitorName(abTransaction, row["mandateDebitorName"].toString().toLocal8Bit().constData());
    AB_Transaction_SetOriginalCreditorSchemeId(abTransaction, row["originalCreditorSchemeId"].toString().toLocal8Bit().constData());
    AB_Transaction_SetOriginalMandateId(abTransaction, row["originalMandateId"].toString().toLocal8Bit().constData());
    AB_Transaction_SetOriginalCreditorName(abTransaction, row["originalCreditorName"].toString().toLocal8Bit().constData());
    AB_Transaction_SetSequence(abTransaction, (TransactionSequence) row["sequence"].toInt());
    AB_Transaction_SetCharge(abTransaction, (TransactionCharge) row["charge"].toInt());
    AB_Transaction_SetRemoteAddrStreet(abTransaction, row["remoteAddrStreet"].toString().toLocal8Bit().constData());
    AB_Transaction_SetRemoteAddrZipcode(abTransaction, row["remoteAddrZipcode"].toString().toLocal8Bit().constData());
    AB_Transaction_SetRemoteAddrCity(abTransaction, row["remoteAddrCity"].toString().toLocal8Bit().constData());
    AB_Transaction_SetRemoteAddrPhone(abTransaction, row["remoteAddrPhone"].toString().toLocal8Bit().constData());
    AB_Transaction_SetPeriod(abTransaction, (TransactionPeriod) row["period"].toInt());
    AB_Transaction_SetCycle(abTransaction, row["cycle"].toUInt());
    AB_Transaction_SetExecutionDay(abTransaction, row["executionDay"].toUInt());
    AB_Transaction_SetFirstDate(abTransaction, Transaction::qDateToGwenDate(row["firstDate"].toDate()));
    AB_Transaction_SetLastDate(abTransaction, Transaction::qDateToGwenDate(row["lastDate"].toDate()));
    AB_Transaction_SetNextDate(abTransaction, Transaction::qDateToGwenDate(row["nextDate"].toDate()));
    AB_Transaction_SetUnitId(abTransaction, row["unitId"].toString().toLocal8Bit().constData());
    AB_Transaction_SetUnitIdNameSpace(abTransaction, row["unitIdNameSpace"].toString().toLocal8Bit().constData());
    AB_Transaction_SetTickerSymbol(abTransaction, row["tickerSymbol"].toString().toLocal8Bit().constData());

    value = AB_Value_new();
    AB_Value_SetValueFromDouble(value, row["units"].toDouble());
    AB_Transaction_SetUnits(abTransaction, AB_Value_dup(value));
    AB_Value_free(value);
    value = nullptr;

    value = AB_Value_new();
    AB_Value_SetValueFromDouble(value, row["unitPriceValue"].toDouble());
    AB_Transaction_SetUnitPriceValue(abTransaction, AB_Value_dup(value));
    AB_Value_free(value);
    value = nullptr;

    AB_Transaction_SetUnitPriceDate(abTransaction, Transaction::qDateToGwenDate(row["unitPriceDate"].toDate()));

    value = AB_Value_new();
    AB_Value_SetValueFromDouble(value, row["commissionValue"].toDouble());
    AB_Transaction_SetCommissionValue(abTransaction, AB_Value_dup(value));
    AB_Value_free(value);
    value = nullptr;

    AB_Transaction_SetMemo(abTransaction, row["memo"].toString().toLocal8Bit().constData());
    AB_Transaction_SetHash(abTransaction, row["hash"].toString().toLocal8Bit().constData());

    const auto transaction = new Transaction(abTransaction);

    AB_Transaction_free(abTransaction);
    abTransaction = nullptr;

    return transaction;
}
