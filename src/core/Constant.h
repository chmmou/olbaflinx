/**
* Copyright (C) 2022, Alexander Saal <developer@olbaflinx.chm-projects.de>
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

#ifndef OLBAFLINX_CONSTANT_H
#define OLBAFLINX_CONSTANT_H

#include "core/Logger/Logger.h"

/**
 * Password regular expression
 *
 * /^(?=.*[a-z])(?=.*[A-Z])(?=.*\d)(?=.*[!"§$%&\/()=?´`{}\[\]\\ß@€~’*'+#-_.:,;µöäüÖÄÜ<|>])[A-Za-z\d!"§$%&\/()=?´`{}\[\]\\ß@€~’*'+#-_.:,;µöäüÖÄÜ<|>]{6,}$/g
 *
 * At least one lower case English letter, a-z
 * At least one upper case English letter, A-Z
 * At least one lower case letter, öäü
 * At least one upper case letter, ÖÄÜ
 * At least one digit, 0-9
 * At least one of special character, !"§$%&/()=?´`{}[]\ß@€~’*'+#-_.:,;µöäüÖÄÜ<|>
 * Minimum six in length 6 (with the anchors)
 */
#define MinPasswordReqEx \
    QRegularExpression("^(?=.*[a-z])(?=.*[A-Z])(?=.*\\d)(?=.*[!\"§$%&/" \
                       "()=?´`{}\\[\\]\\ß@€~’*'+#-_.:,;µöäüÖÄÜ<|>])[A-Za-z\\d!\"§$%&/" \
                       "()=?´`{}\\[\\]\\ß@€~’*'+#-_.:,;µöäüÖÄÜ<|>]{6,}$")

#define StorageFileBackupDateTimeFormat "yyyyMMddhhmmsszzz"

#define StorageSqlAccountInsertQuery \
    "INSERT INTO accounts (`type`, unique_id, backend_name, owner_name, " \
    "account_name, currency, memo, iban, bic, country, bank_code, bank_name, " \
    "branch_id, account_number, sub_account_number) " \
    "VALUES (:type, :unique_id, :backend_name, :owner_name, :account_name, " \
    ":currency, :memo, :iban, :bic, :country, :bank_code, :bank_name, " \
    ":branch_id, :account_number, :sub_account_number);"

#define StorageSqlAccountBalanceInsertQuery \
    "INSERT INTO balances (account_id, `date`, `value`, `type`, currency) " \
    "VALUES (:account_id, :date, :value, :type, :currency) " \
    "ON CONFLICT (account_id) DO UPDATE SET " \
    "   date = excluded.date, " \
    "   value = excluded.value, " \
    "   type = excluded.type, " \
    "   currency = excluded.currency " \
    "WHERE excluded.account_id = balances.account_id"

#define StorageSqlAccountSelectQuery "SELECT * FROM accounts"
#define StorageSqlAccountSelectByIdQuery \
    QString("%1 WHERE id = :id").args(StorageSqlAccountSelectQuery)

#define StorageSqlTransactionInsertQuery \
    "INSERT INTO transactions (account_id, `type`, sub_type, command, status, " \
    "unique_account_id, unique_id, ref_unique_id, id_for_application, " \
    "string_id_for_application, session_id, group_id, fi_id, local_iban, local_bic, " \
    "local_country, local_bank_code, local_branch_id, local_account_number, local_suffix, " \
    "local_name, remote_country, remote_bank_code, remote_branch_id, " \
    "remote_account_number, remote_suffix, remote_iban, remote_bic, remote_name, `date`, " \
    "valuta_date, value, currency, fees, transaction_code, transaction_text, " \
    "transaction_key, text_key, primanota, purpose, `category`, customer_reference, " \
    "bank_reference, end_to_end_reference, creditor_scheme_id, originator_id, mandate_id, " \
    "mandate_date, mandate_debitor_name, original_creditor_scheme_id, original_mandate_id, " \
    "original_creditor_name, `sequence`, charge, remote_addr_street, remote_addr_zipcode, " \
    "remote_addr_city, remote_addr_phone, period, `cycle`, execution_day, first_date, " \
    "last_date, next_date, unit_id, unit_id_name_space, ticker_symbol, units, " \
    "unit_price_value, unit_price_date, commission_value, memo, `hash`) " \
    "VALUES (:account_id, :type, :sub_type, :command, :status, :unique_account_id, " \
    ":unique_id, :ref_unique_id, :id_for_application, :string_id_for_application, " \
    ":session_id, :group_id, :fi_id, :local_iban, :local_bic, :local_country, " \
    ":local_bank_code, :local_branch_id, :local_account_number, :local_suffix, " \
    ":local_name, :remote_country, :remote_bank_code, :remote_branch_id, " \
    ":remote_account_number, :remote_suffix, :remote_iban, :remote_bic, :remote_name, " \
    ":date, :valuta_date, :value, :currency, :fees, :transaction_code, :transaction_text, " \
    ":transaction_key, :text_key, :primanota, :purpose, :category, :customer_reference, " \
    ":bank_reference, :end_to_end_reference, :creditor_scheme_id, :originator_id, " \
    ":mandate_id, :mandate_date, :mandate_debitor_name, :original_creditor_scheme_id, " \
    ":original_mandate_id, :original_creditor_name, :sequence, :charge, " \
    ":remote_addr_street, :remote_addr_zipcode, :remote_addr_city, :remote_addr_phone, " \
    ":period, :cycle, :execution_day, :first_date, :last_date, :next_date, :unit_id, " \
    ":unit_id_name_space, :ticker_symbol, :units, :unit_price_value, :unit_price_date, " \
    ":commission_value, :memo, :hash);"

#define StorageSqlTransactionSelectQuery "SELECT * FROM transactions"
#define StorageSqlTransactionSelectCountQuery \
    "SELECT COUNT(id) AS CNT FROM transactions WHERE `type` = :type;"
#define StorageSqlTransactionByAccountIdQuery \
    QString("%1 WHERE account_id = :account_id").arg(StorageSqlTransactionSelectQuery)
#define StorageSqlTransactionByAccountIdWithLimitQuery \
    QString("%1 LIMIT :limit OFFSET :offset").arg(StorageSqlTransactionByAccountIdQuery)

#define StorageSqlTransactionExists \
    "SELECT COUNT(id) AS CNT FROM transactions WHERE account_id = :account_id AND hash = :hash"

/**
 * Group & Key for settings
 */
#define StorageSettingGroup "Vaults"
#define StorageSettingGroupKey "Paths"

#define MaxDateForTransactionsWithoutPin -28
#define GwenDateFormat "yyyyMMdd"
#define DateTimeFormat "dd.MM.yyyy hh:mm"

#define LOG(msg) olbaflinx::core::logger::Logger::instance()->log(msg);

#endif //OLBAFLINX_CONSTANT_H
