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

#include "core/Banking/Balance/Balance.h"

#include <aqbanking/types/value.h>

#include <gwenhywfar/buffer.h>
#include <gwenhywfar/gwendate.h>

#include <memory>

using namespace olbaflinx::core::banking::balance;

namespace {

using GwenBufferPtr = std::unique_ptr<GWEN_BUFFER, decltype(&GWEN_Buffer_free)>;

} // namespace

class Balance::Private
{
public:
    explicit Private(quint32 accountId, const AB_BALANCE *balance)
        : uniqueAccountId(accountId)
        , abBalance(balance ? AB_Balance_dup(balance) : AB_Balance_new())
    {}

    ~Private()
    {
        AB_Balance_free(abBalance);
        abBalance = nullptr;
    }

    /**
     * A GWEN_DATE carries a year, a month and a day and nothing else. A date
     * that cannot be read answers with an invalid QDate rather than with today,
     * which would put an invented day into banking data.
     */
    static QDate toDate(const GWEN_DATE *gwenDate)
    {
        if (gwenDate == nullptr) {
            return {};
        }

        const GwenBufferPtr buffer(GWEN_Buffer_new(nullptr, 16, 0, 1), &GWEN_Buffer_free);

        if (GWEN_Date_toStringWithTemplate(gwenDate, "DD.MM.YYYY", buffer.get()) != GWEN_SUCCESS) {
            return {};
        }

        return QDate::fromString(QString::fromUtf8(GWEN_Buffer_GetStart(buffer.get())),
                                 QStringLiteral("dd.MM.yyyy"));
    }

    quint32 uniqueAccountId;
    AB_BALANCE *abBalance;
};

Balance::Balance(quint32 uniqueAccountId, const AB_BALANCE *balance)
    : BankingItem()
    , d_ptr(new Private(uniqueAccountId, balance))
{}

Balance::~Balance()
{
    delete d_ptr;
}

quint32 Balance::uniqueAccountId() const
{
    return d_ptr->uniqueAccountId;
}

QDate Balance::date() const
{
    return Private::toDate(AB_Balance_GetDate(d_ptr->abBalance));
}

qreal Balance::value() const
{
    const AB_VALUE *value = AB_Balance_GetValue(d_ptr->abBalance);

    return value == nullptr ? 0.0 : AB_Value_GetValueAsDouble(value);
}

QString Balance::currency() const
{
    const AB_VALUE *value = AB_Balance_GetValue(d_ptr->abBalance);
    if (value == nullptr) {
        return {};
    }

    const char *currency = AB_Value_GetCurrency(value);

    return currency == nullptr ? QString() : QString::fromUtf8(currency);
}

BalanceType Balance::type() const
{
    return AB_Balance_GetType(d_ptr->abBalance);
}

/**
 * The account is the only value without which a balance cannot be used. An
 * amount of zero is a balance, and a bank that sends no date leaves a column
 * empty rather than an unusable record.
 */
bool Balance::isValid() const
{
    return d_ptr->uniqueAccountId != 0;
}

/**
 * Carries no amount. The string ends up in log entries, and no entry of this
 * project names a balance, an amount or an account number.
 */
QString Balance::toString() const
{
    return QStringLiteral("Balance of type %1 for account %2")
        .arg(QString::number(type()), QString::number(uniqueAccountId()));
}

/**
 * The column names of the balance table, minus its account_id: that one is the
 * row id of the stored account and is known to the storage alone.
 */
QMap<QString, QVariant> Balance::toMap() const
{
    return {
        {QStringLiteral("unique_account_id"), uniqueAccountId()},
        {QStringLiteral("date"), date()},
        {QStringLiteral("value"), value()},
        {QStringLiteral("type"), static_cast<int>(type())},
        {QStringLiteral("currency"), currency()},
    };
}

QString Balance::itemType() const
{
    return QStringLiteral("Balance");
}
