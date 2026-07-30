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

#include "core/ApplicationInfo.h"
#include "core/Banking/Account/Account.h"
#include "core/Error.h"

#include <QtCore/QObject>

namespace olbaflinx::core::banking {

using namespace ::account;

/**
 * @brief
 *  The Banking Backend Object contains the encapsulated and complete business logic of AQBanking
 *  for Qt 6.
 * @note
 *  Currently only the fetching of accounts and their (SEPA) transfers / direct debits / standing
 *  orders are supported. The sending of transfers / direct debits will be added step by step.
 * @author Alexander Saal
 * @version 1.0
 * @package olbaflinx::core::banking
 *
 * Ownership: the creator owns the instance. The accounts reported through
 * itemsReceived pass into the ownership of the receiver.
 */
class OLBAFLINX_CORE_EXPORT Banking : public QObject
{
    Q_OBJECT

public:
    /**
     * @param applicationInfo Details used to sign on to the chip card service
     *  and for the title of the setup dialog.
     * @param parent Optional owner.
     */
    explicit Banking(ApplicationInfo applicationInfo, QObject *parent = nullptr);
    ~Banking() override;

    /**
     * @brief Initialize the banking backend.
     *
     * @param name Application name registered by German HBCI ZKA
     * @param version Application version registered by German HBCI ZKA
     * @param key The FinTS registration key from German ZKA
     *
     * @return A default constructed Error on success, otherwise the reason. The
     *  caller has to check it, the return type is [[nodiscard]].
     */
    Error initialize(const QString &name, const QString &version, const QString &key);

    /**
     * @brief Finalize the banking backend and free all resources.
     */
    void finalize();

    /**
     * @brief Open the aqbaking setup dialog.
     *
     * @return
     *  If banking backend not initialized or other error occurred -1 is returned; otherwise
     *  the return value from setup dialog.
     */
    int setupAccounts();

    /**
     * @brief Get all accounts previously set up with Banking::setupAccounts
     */
    void accounts();

Q_SIGNALS:
    /**
     * @brief This signal is emitted if an error occurred on an asynchronous path.
     *
     * @param errorCode @ref olbaflinx::core::ErrorCode
     * @param reason Technical message, meant for the log. It carries the return
     *  value of the banking backend where there is one.
     */
    void errorOccurred(olbaflinx::core::ErrorCode errorCode, const QString &reason);

    void progressValueChanged(qreal progress);
    void itemsReceived(const BankingItems &items);
    void finished();

private:
    class Private;
    Private *d_ptr = nullptr;
};

} // namespace olbaflinx::core::banking
