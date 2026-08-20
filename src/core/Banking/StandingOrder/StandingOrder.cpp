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

#include "core/Banking/StandingOrder/StandingOrder.h"

#include <gwenhywfar/buffer.h>
#include <gwenhywfar/gwendate.h>

#include <QtCore/QCryptographicHash>
#include <QtCore/QDataStream>
#include <QtCore/QIODevice>
#include <QtCore/QObject>

#include <memory>

using namespace olbaflinx::core::banking::standingorder;

namespace {

/**
 * The C structures of the backend, held so that every path out of a function
 * releases them.
 */
using GwenDatePtr = std::unique_ptr<GWEN_DATE, decltype(&GWEN_Date_free)>;
using GwenBufferPtr = std::unique_ptr<GWEN_BUFFER, decltype(&GWEN_Buffer_free)>;

} // namespace

class StandingOrder::Private
{
public:
    // A duplicate is made only of what the caller handed in. A fallback that
    // built a record and duplicated that one as well would never release the
    // structure it had just created.
    explicit Private(StandingOrder *order, const AB_TRANSACTION *abTT)
        : abTransaction(abTT ? AB_Transaction_dup(abTT) : AB_Transaction_new())
        , q_ptr(order)
    {}

    ~Private()
    {
        AB_Transaction_free(abTransaction);
        abTransaction = nullptr;
    }

    Private(const Private &) = delete;
    Private &operator=(const Private &) = delete;
    Private(Private &&) = delete;
    Private &operator=(Private &&) = delete;

    QString calculateFingerprint()
    {
        if (!q_ptr->fingerprint().isEmpty()) {
            return q_ptr->fingerprint();
        }

        QByteArray buffer;
        QDataStream out(&buffer, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_DefaultCompiledVersion);

        out << q_ptr->uniqueAccountId() << q_ptr->remoteIban() << q_ptr->remoteName()
            << q_ptr->value() << q_ptr->currency() << q_ptr->purpose()
            << static_cast<qint32>(q_ptr->period()) << q_ptr->cycle() << q_ptr->executionDay()
            << q_ptr->firstDate();

        return {QCryptographicHash::hash(buffer, QCryptographicHash::Sha256).toHex()};
    }

    /**
     * Reads a date out of the banking backend.
     *
     * A date that cannot be read answers with an invalid QDate rather than with
     * today, which would put an invented day into an order.
     */
    static QDate toDate(const GWEN_DATE *gwenDate)
    {
        if (gwenDate == nullptr) {
            return {};
        }

        // Held, so that the early return below releases it as well.
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
     * given, so the handle has to be released again, and a holder does it at
     * every call site.
     *
     * A date that is not set answers with an empty handle, which the setters
     * read as "no date", rather than with today.
     */
    static GwenDatePtr fromDate(const QDate &date)
    {
        if (!date.isValid() || date.isNull()) {
            return {nullptr, &GWEN_Date_free};
        }

        const auto text = date.toString(QStringLiteral("yyyyMMdd")).toLatin1();

        return {GWEN_Date_fromString(text.constData()), &GWEN_Date_free};
    }

    AB_TRANSACTION *abTransaction = nullptr;

private:
    StandingOrder *q_ptr = nullptr;
};

StandingOrder::StandingOrder(const AB_TRANSACTION *transaction)
    : d_ptr(new Private(this, transaction))
{}

StandingOrder::StandingOrder(quint32 uniqueAccountId, const AB_TRANSACTION *transaction)
    : d_ptr(new Private(this, transaction))
{
    if (AB_Transaction_GetUniqueAccountId(d_ptr->abTransaction) == 0) {
        AB_Transaction_SetUniqueAccountId(d_ptr->abTransaction, uniqueAccountId);
    }
}

StandingOrder::~StandingOrder()
{
    delete d_ptr;
    d_ptr = nullptr;
}

std::shared_ptr<StandingOrder> StandingOrder::fromMap(const QMap<QString, QVariant> &map)
{
    auto *abTransaction = AB_Transaction_new();

    AB_Transaction_SetType(abTransaction, AB_Transaction_TypeStandingOrder);
    AB_Transaction_SetCommand(abTransaction, AB_Transaction_CommandGetStandingOrders);
    AB_Transaction_SetStatus(abTransaction,
                             static_cast<AB_TRANSACTION_STATUS>(
                                 map.value(QStringLiteral("status")).toInt()));
    AB_Transaction_SetUniqueAccountId(abTransaction,
                                      map.value(QStringLiteral("unique_account_id")).toUInt());
    AB_Transaction_SetUniqueId(abTransaction, map.value(QStringLiteral("unique_id")).toUInt());

    const auto setText = [&map, abTransaction](const QString &column,
                                               void (*setter)(AB_TRANSACTION *, const char *)) {
        setter(abTransaction, map.value(column).toString().toUtf8().constData());
    };

    setText(QStringLiteral("fi_id"), &AB_Transaction_SetFiId);
    setText(QStringLiteral("local_iban"), &AB_Transaction_SetLocalIban);
    setText(QStringLiteral("local_bic"), &AB_Transaction_SetLocalBic);
    setText(QStringLiteral("local_name"), &AB_Transaction_SetLocalName);
    setText(QStringLiteral("remote_iban"), &AB_Transaction_SetRemoteIban);
    setText(QStringLiteral("remote_bic"), &AB_Transaction_SetRemoteBic);
    setText(QStringLiteral("remote_name"), &AB_Transaction_SetRemoteName);
    setText(QStringLiteral("purpose"), &AB_Transaction_SetPurpose);
    setText(QStringLiteral("end_to_end_reference"), &AB_Transaction_SetEndToEndReference);
    setText(QStringLiteral("memo"), &AB_Transaction_SetMemo);
    setText(QStringLiteral("fingerprint"), &AB_Transaction_SetHash);

    auto *value = AB_Value_new();
    AB_Value_SetValueFromDouble(value, map.value(QStringLiteral("value")).toDouble());
    AB_Value_SetCurrency(value,
                         map.value(QStringLiteral("currency")).toString().toUtf8().constData());
    AB_Transaction_SetValue(abTransaction, value);
    AB_Value_free(value);

    AB_Transaction_SetPeriod(abTransaction,
                             static_cast<AB_TRANSACTION_PERIOD>(
                                 map.value(QStringLiteral("period")).toInt()));
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

    auto order = std::make_shared<StandingOrder>(abTransaction);
    AB_Transaction_free(abTransaction);

    return order;
}

quint32 StandingOrder::uniqueAccountId() const
{
    return AB_Transaction_GetUniqueAccountId(d_ptr->abTransaction);
}

quint32 StandingOrder::uniqueId() const
{
    return AB_Transaction_GetUniqueId(d_ptr->abTransaction);
}

QString StandingOrder::fiId() const
{
    return QString::fromUtf8(AB_Transaction_GetFiId(d_ptr->abTransaction));
}

QString StandingOrder::localIban() const
{
    return QString::fromUtf8(AB_Transaction_GetLocalIban(d_ptr->abTransaction));
}

QString StandingOrder::localBic() const
{
    return QString::fromUtf8(AB_Transaction_GetLocalBic(d_ptr->abTransaction));
}

QString StandingOrder::localName() const
{
    return QString::fromUtf8(AB_Transaction_GetLocalName(d_ptr->abTransaction));
}

QString StandingOrder::remoteIban() const
{
    return QString::fromUtf8(AB_Transaction_GetRemoteIban(d_ptr->abTransaction));
}

QString StandingOrder::remoteBic() const
{
    return QString::fromUtf8(AB_Transaction_GetRemoteBic(d_ptr->abTransaction));
}

QString StandingOrder::remoteName() const
{
    return QString::fromUtf8(AB_Transaction_GetRemoteName(d_ptr->abTransaction));
}

qreal StandingOrder::value() const
{
    const auto *abValue = AB_Transaction_GetValue(d_ptr->abTransaction);

    return abValue == nullptr ? 0.0 : AB_Value_GetValueAsDouble(abValue);
}

QString StandingOrder::currency() const
{
    const auto *abValue = AB_Transaction_GetValue(d_ptr->abTransaction);

    return abValue == nullptr ? QString() : QString::fromUtf8(AB_Value_GetCurrency(abValue));
}

QString StandingOrder::purpose() const
{
    return QString::fromUtf8(AB_Transaction_GetPurpose(d_ptr->abTransaction));
}

QString StandingOrder::endToEndReference() const
{
    return QString::fromUtf8(AB_Transaction_GetEndToEndReference(d_ptr->abTransaction));
}

AB_TRANSACTION_PERIOD StandingOrder::period() const
{
    return AB_Transaction_GetPeriod(d_ptr->abTransaction);
}

quint32 StandingOrder::cycle() const
{
    return AB_Transaction_GetCycle(d_ptr->abTransaction);
}

quint32 StandingOrder::executionDay() const
{
    return AB_Transaction_GetExecutionDay(d_ptr->abTransaction);
}

QDate StandingOrder::firstDate() const
{
    return Private::toDate(AB_Transaction_GetFirstDate(d_ptr->abTransaction));
}

QDate StandingOrder::lastDate() const
{
    return Private::toDate(AB_Transaction_GetLastDate(d_ptr->abTransaction));
}

QDate StandingOrder::nextDate() const
{
    return Private::toDate(AB_Transaction_GetNextDate(d_ptr->abTransaction));
}

AB_TRANSACTION_STATUS StandingOrder::status() const
{
    return AB_Transaction_GetStatus(d_ptr->abTransaction);
}

QString StandingOrder::memo() const
{
    return QString::fromUtf8(AB_Transaction_GetMemo(d_ptr->abTransaction));
}

QString StandingOrder::fingerprint() const
{
    return QString::fromUtf8(AB_Transaction_GetHash(d_ptr->abTransaction));
}

QString StandingOrder::calculateFingerprint() const
{
    return d_ptr->calculateFingerprint();
}

bool StandingOrder::isValid() const
{
    return AB_Transaction_GetType(d_ptr->abTransaction) == AB_Transaction_TypeStandingOrder;
}

QString StandingOrder::toString() const
{
    // Deliberately without IBAN, account number or name, this ends up in the log.
    return QObject::tr("Standing order %1 - %2 %3")
        .arg(QString::number(uniqueId()), QString::number(value()), currency());
}

QMap<QString, QVariant> StandingOrder::toMap() const
{
    QMap<QString, QVariant> map = {};

    map[QStringLiteral("unique_account_id")] = uniqueAccountId();
    map[QStringLiteral("unique_id")] = uniqueId();
    map[QStringLiteral("fi_id")] = fiId();
    map[QStringLiteral("local_iban")] = localIban();
    map[QStringLiteral("local_bic")] = localBic();
    map[QStringLiteral("local_name")] = localName();
    map[QStringLiteral("remote_iban")] = remoteIban();
    map[QStringLiteral("remote_bic")] = remoteBic();
    map[QStringLiteral("remote_name")] = remoteName();
    map[QStringLiteral("value")] = value();
    map[QStringLiteral("currency")] = currency();
    map[QStringLiteral("purpose")] = purpose();
    map[QStringLiteral("end_to_end_reference")] = endToEndReference();
    map[QStringLiteral("period")] = static_cast<qint32>(period());
    map[QStringLiteral("cycle")] = cycle();
    map[QStringLiteral("execution_day")] = executionDay();
    map[QStringLiteral("first_date")] = firstDate();
    map[QStringLiteral("last_date")] = lastDate();
    map[QStringLiteral("next_date")] = nextDate();
    map[QStringLiteral("status")] = static_cast<qint32>(status());
    map[QStringLiteral("memo")] = memo();
    map[QStringLiteral("fingerprint")] = calculateFingerprint();

    // Formed for both ways, so that an order which gains an identifier later is
    // found by its fingerprint rather than written a second time.
    map[QStringLiteral("identified_by")] = static_cast<qint32>(
        fiId().isEmpty() ? IdentifiedBy::Fingerprint : IdentifiedBy::InstitutionId);

    return map;
}

QString StandingOrder::itemType() const
{
    return QStringLiteral("StandingOrder");
}
