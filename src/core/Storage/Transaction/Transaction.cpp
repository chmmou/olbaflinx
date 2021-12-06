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
    return AB_Value_GetValueAsDouble(AB_Transaction_GetValue(abTransaction));
}

QString Transaction::currency() const
{
    return QString::fromUtf8(AB_Value_GetCurrency(AB_Transaction_GetValue(abTransaction)));
}

qreal Transaction::fees() const
{
    return AB_Value_GetValueAsDouble(AB_Transaction_GetFees(abTransaction));
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
    return AB_Value_GetValueAsDouble(AB_Transaction_GetUnits(abTransaction));
}

qreal Transaction::unitPriceValue() const
{
    return AB_Value_GetValueAsDouble(AB_Transaction_GetUnitPriceValue(abTransaction));
}

QDate Transaction::unitPriceDate() const
{
    return gwenDateToQDate(AB_Transaction_GetUnitPriceDate(abTransaction));
}

qreal Transaction::commissionValue() const
{
    return AB_Value_GetValueAsDouble(AB_Transaction_GetCommissionValue(abTransaction));
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

Transaction *Transaction::create(const QMap<QString, QVariant> &row)
{
    return nullptr;
}
