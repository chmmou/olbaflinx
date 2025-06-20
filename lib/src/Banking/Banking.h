/**
 * Copyright (C2022-2025, Alexander Saal <developer@olbaflinx.chm-projects.de>
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

#ifndef OLBAFLINX_CORE_BANKING_H
#define OLBAFLINX_CORE_BANKING_H

#include "OlbaFlinxCore.h"

#include <Banking/Account/Account.h>

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
 */
class OLBAFLINX_CORE_EXPORT Banking : public QObject
{
    Q_OBJECT

public:
    explicit Banking(QObject *parent = Q_NULLPTR);
    ~Banking() override;

    /**
     * @brief Initialize the banking backend.
     *
     * @param name Application name registered by German HBCI ZKA
     * @param version Application version registered by German HBCI ZKA
     * @param key The FinTS registration key from German ZKA
     *
     * @return true on success; otherwise false.
     */
    bool initialize(const QString &name, const QString &version, const QString &key);

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
    void errorOccurred(qint32 code, const QString &reason);

    void progressValueChanged(qreal progress);
    void itemsReceived(const QList<BankingItem *> &items);
    void finished();

private:
    class Private;
    Private *d_ptr;
};

} // namespace olbaflinx::core::banking

#endif //OLBAFLINX_CORE_BANKING_H
