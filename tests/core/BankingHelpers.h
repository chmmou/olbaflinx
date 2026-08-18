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
 */
#pragma once

#include "core/Banking/Account/Account.h"
#include "core/Banking/Balance/Balance.h"
#include "core/Banking/BankingItem.h"

#include <aqbanking/types/balance.h>
#include <aqbanking/types/imexporter_context.h>
#include <aqbanking/types/transaction.h>
#include <aqbanking/types/value.h>

#include <gwenhywfar/gui.h>
#include <gwenhywfar/gui_be.h>
#include <gwenhywfar/gwendate.h>

#include <QtCore/QDate>
#include <QtCore/QList>
#include <QtCore/QSemaphore>
#include <QtCore/QString>
#include <QtCore/QThread>

#include <atomic>
#include <memory>

using namespace olbaflinx::core::banking;
using namespace olbaflinx::core::banking::account;
using namespace olbaflinx::core::banking::balance;

namespace olbaflinx::core::tests {

/** One balance of a response container. */
struct BalanceSpec
{
    AB_BALANCE_TYPE type = AB_Balance_TypeBooked;
    QDate date = QDate(2026, 2, 1);
    double value = 1234.56;
};

/**
 * What the banking backend hands over, built without a bank: an account as it
 * reports one, a balance, and the container a session would have filled.
 *
 * These carry C structures of the backend. Every one of them is released here;
 * what leaves this class is either a shared record of the core or a container
 * whose owner is named at the function.
 */
class BankingHelpers
{
public:
    /**
     * An account the way AqBanking reports one.
     *
     * @param backendName The backend that holds the account. An empty name is
     *  an account without online access, which a fetch passes over.
     */
    static std::shared_ptr<Account> accountFromBackend(quint32 uniqueId,
                                                       const char *backendName = "aqhbci")
    {
        AB_ACCOUNT_SPEC *spec = AB_AccountSpec_new();

        AB_AccountSpec_SetUniqueId(spec, uniqueId);
        AB_AccountSpec_SetBackendName(spec, backendName);
        AB_AccountSpec_SetAccountName(spec, "Girokonto");
        AB_AccountSpec_SetOwnerName(spec, "Erika Mustermann");
        AB_AccountSpec_SetIban(spec, "DE02120300000000202051");
        AB_AccountSpec_SetBankCode(spec, "12030000");
        AB_AccountSpec_SetAccountNumber(spec, "0000202051");
        AB_AccountSpec_SetCurrency(spec, "EUR");

        auto account = std::make_shared<Account>(spec);
        AB_AccountSpec_free(spec);

        return account;
    }

    /**
     * A balance the way a fetch reports one, as the record the storage takes.
     */
    static std::shared_ptr<Balance> balanceFromBackend(quint32 uniqueAccountId,
                                                       const BalanceSpec &spec = {})
    {
        AB_BALANCE *abBalance = AB_Balance_new();

        AB_Balance_SetType(abBalance, spec.type);
        AB_Balance_SetDate(abBalance, gwenDateOf(spec.date).get());

        AB_VALUE *abValue = AB_Value_fromDouble(spec.value);
        AB_Value_SetCurrency(abValue, "EUR");
        AB_Balance_SetValue(abBalance, abValue);
        AB_Value_free(abValue);

        auto balance = std::make_shared<Balance>(uniqueAccountId, abBalance);
        AB_Balance_free(abBalance);

        return balance;
    }

    /**
     * The container a session would have filled. Every test builds its own,
     * which is what makes the evaluation measurable without a bank.
     *
     * The caller owns the result and releases it with AB_ImExporterContext_free.
     *
     * @param firstBookingDate The day of the first booking, every further one a
     *  day on. Left invalid, the bookings carry no date at all, which is what a
     *  test that does not look at the period wants.
     *
     * The bookings carry no account of their own, and that is not an omission of
     * the helper. A statement arrives through the importer of the backend, which
     * fills the fields of the booking and leaves the account among them empty;
     * only the entry the booking sits in names it. A helper that filled it here
     * would measure a container no bank ever sends.
     */
    static AB_IMEXPORTER_CONTEXT *responseContext(quint32 uniqueId,
                                                  int transactionCount,
                                                  const QList<BalanceSpec> &balances,
                                                  const QDate &firstBookingDate = {})
    {
        AB_IMEXPORTER_CONTEXT *context = AB_ImExporterContext_new();

        addAccountToContext(context, uniqueId, transactionCount, balances, firstBookingDate);

        return context;
    }

    /**
     * A further account in a container that already stands.
     *
     * What a fetch over several accounts answers with: one container, one entry
     * per account. Every entry carries an account number and an IBAN of its own,
     * and that is not decoration: the lookup of the backend falls through from
     * the identifier to the bank code and the account number, whatever
     * identifier it was asked for. Entries that share those two would be one
     * entry, and a container of three accounts would hold one.
     */
    static void addAccountToContext(AB_IMEXPORTER_CONTEXT *context,
                                    quint32 uniqueId,
                                    int transactionCount,
                                    const QList<BalanceSpec> &balances,
                                    const QDate &firstBookingDate = {})
    {
        const QString accountNumber = QString::number(uniqueId).rightJustified(10, u'0');
        const QByteArray localAccountNumber = accountNumber.toLatin1();
        const QByteArray localIban = (QStringLiteral("DE0212030000") + accountNumber).toLatin1();

        AB_IMEXPORTER_ACCOUNTINFO *info
            = AB_ImExporterContext_GetOrAddAccountInfo(context,
                                                       uniqueId,
                                                       localIban.constData(),
                                                       "12030000",
                                                       localAccountNumber.constData(),
                                                       AB_AccountType_Checking);

        for (int index = 0; index < transactionCount; ++index) {
            AB_TRANSACTION *transaction = AB_Transaction_new();

            AB_Transaction_SetType(transaction, AB_Transaction_TypeStatement);
            AB_Transaction_SetUniqueId(transaction, static_cast<uint32_t>(index) + 1);

            const auto purpose = QStringLiteral("Booking %1").arg(index + 1).toUtf8();
            AB_Transaction_SetPurpose(transaction, purpose.constData());

            if (firstBookingDate.isValid()) {
                AB_Transaction_SetDate(transaction,
                                       gwenDateOf(firstBookingDate.addDays(index)).get());
            }

            AB_ImExporterAccountInfo_AddTransaction(info, transaction);
        }

        for (const BalanceSpec &spec : balances) {
            AB_BALANCE *balance = AB_Balance_new();

            AB_Balance_SetType(balance, spec.type);
            AB_Balance_SetDate(balance, gwenDateOf(spec.date).get());

            AB_VALUE *value = AB_Value_fromDouble(spec.value);
            AB_Value_SetCurrency(value, "EUR");
            AB_Balance_SetValue(balance, value);
            AB_Value_free(value);

            AB_ImExporterAccountInfo_AddBalance(info, balance);
        }
    }

    /** The command of the given kind, or null if the list carries none. */
    static AB_TRANSACTION *commandOfKind(AB_TRANSACTION_LIST2 *commands, AB_TRANSACTION_COMMAND kind)
    {
        AB_TRANSACTION_LIST2_ITERATOR *iterator = AB_Transaction_List2_First(commands);
        if (iterator == nullptr) {
            return nullptr;
        }

        AB_TRANSACTION *found = nullptr;

        AB_TRANSACTION *command = AB_Transaction_List2Iterator_Data(iterator);
        while (command != nullptr) {
            if (AB_Transaction_GetCommand(command) == kind) {
                found = command;
                break;
            }
            command = AB_Transaction_List2Iterator_Next(iterator);
        }

        AB_Transaction_List2Iterator_free(iterator);

        return found;
    }

    /** The items of the given type, in the order the core reported them. */
    static BankingItems itemsOfType(const BankingItems &items, const QString &type)
    {
        BankingItems found;

        for (const BankingItemPtr &item : items) {
            if (item->itemType() == type) {
                found.append(item);
            }
        }

        return found;
    }

private:
    using GwenDatePtr = std::unique_ptr<GWEN_DATE, decltype(&GWEN_Date_free)>;

    /**
     * The date the backend understands. The setters duplicate what they are
     * given, so the handle is released again on every path.
     */
    static GwenDatePtr gwenDateOf(const QDate &date)
    {
        if (!date.isValid()) {
            return {nullptr, &GWEN_Date_free};
        }

        const auto text = date.toString(QStringLiteral("yyyyMMdd")).toLatin1();

        return {GWEN_Date_fromString(text.constData()), &GWEN_Date_free};
    }
};

/**
 * The non-interactive interface of gwenhywfar. Banking refuses to come up
 * without one, and this one answers no prompt and shows no dialog.
 *
 * Ownership stays with the instance. Banking takes the pointer and never frees
 * it.
 */
class ScopedConsoleGui
{
public:
    ScopedConsoleGui()
        : m_gui(GWEN_Gui_new())
    {}

    ~ScopedConsoleGui() { GWEN_Gui_free(m_gui); }

    ScopedConsoleGui(const ScopedConsoleGui &) = delete;
    ScopedConsoleGui &operator=(const ScopedConsoleGui &) = delete;
    ScopedConsoleGui(ScopedConsoleGui &&) = delete;
    ScopedConsoleGui &operator=(ScopedConsoleGui &&) = delete;

    [[nodiscard]] GWEN_GUI *get() const { return m_gui; }

private:
    GWEN_GUI *m_gui;
};

/**
 * An interface that holds a session at its very first step.
 *
 * AB_Banking_SendCommands opens a progress before it does anything else. An
 * interface that does not come back from there leaves the session standing, and
 * that is what makes the promises around a running fetch measurable without a
 * bank: a second fetch is refused while one runs, and the thread that asked for
 * it goes on in the meantime.
 *
 * Only one instance is in play at a time, which is why the gate is where it is:
 * the callback of gwenhywfar carries no place to hang anything on.
 *
 * Ownership stays with the instance. Banking takes the pointer and never frees
 * it. Whoever builds one releases the session before letting it go, or the
 * thread of the session is still standing in it.
 */
class ScopedHoldingGui
{
public:
    ScopedHoldingGui()
        : m_gui(GWEN_Gui_new())
    {
        // The release of a former instance leaves permits behind. Without
        // taking them back, the next session would walk straight through the
        // hold and the count would be measuring nothing.
        s_gate.tryAcquire(s_gate.available());

        s_progresses = 0;
        s_sessionThread = nullptr;
        GWEN_Gui_SetProgressStartFn(m_gui, &ScopedHoldingGui::holdAtProgressStart);
    }

    ~ScopedHoldingGui()
    {
        release();
        GWEN_Gui_free(m_gui);
    }

    ScopedHoldingGui(const ScopedHoldingGui &) = delete;
    ScopedHoldingGui &operator=(const ScopedHoldingGui &) = delete;
    ScopedHoldingGui(ScopedHoldingGui &&) = delete;
    ScopedHoldingGui &operator=(ScopedHoldingGui &&) = delete;

    [[nodiscard]] GWEN_GUI *get() const { return m_gui; }

    /**
     * How many progresses have been opened.
     *
     * A session opens several of them, one below the other, and the first of a
     * session is the one that stands here until it is released. As long as
     * nothing is released, the count is the number of sessions that started, and
     * that is what shows a refused fetch never got as far as the backend.
     */
    [[nodiscard]] static int progressCount() { return s_progresses.load(); }

    /**
     * The thread the first callback arrived in.
     *
     * It is the interface of that thread the backend asked, so this is where a
     * session actually ran. Null until a session has reached the hold.
     */
    [[nodiscard]] static QThread *sessionThread() { return s_sessionThread.load(); }

    /** Lets every session that is waiting go on. */
    static void release() { s_gate.release(64); }

private:
    static uint32_t GWENHYWFAR_CB
    holdAtProgressStart(GWEN_GUI *, uint32_t, const char *, const char *, uint64_t, uint32_t)
    {
        ++s_progresses;
        s_sessionThread = QThread::currentThread();
        s_gate.acquire();

        // Any id will do. Nothing here reads it back.
        return 1;
    }

    inline static QSemaphore s_gate{0};
    inline static std::atomic_int s_progresses{0};
    inline static std::atomic<QThread *> s_sessionThread{nullptr};

    GWEN_GUI *m_gui;
};

} // namespace olbaflinx::core::tests
