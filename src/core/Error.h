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

#include <QtCore/QObject>
#include <QtCore/QString>

#include <utility>

namespace olbaflinx::core {

Q_NAMESPACE_EXPORT(OLBAFLINX_CORE_EXPORT)

/**
 * @brief The one machine readable error code of the project.
 *
 * The enumeration is registered with the meta object system so that it can
 * travel through a signal and be read back by name in a log entry.
 */
enum class ErrorCode {
    /** No error. The default of a value initialised Error. */
    None,
    /** The requested record does not exist. */
    NotFound,
    /** The operation was refused for want of a permission or a matching key. */
    PermissionDenied,
    /** An argument or a record handed in does not meet the contract. */
    InvalidInput,
    /** A file could not be read, written or removed. */
    IoFailure,
    /** The database rejected a statement or could not be opened. */
    DatabaseFailure,
    /** The banking backend reported a failure. */
    BankingFailure,
    /** The operation is not implemented yet. */
    NotImplemented,
    /** The database driver the storage needs is not registered with Qt. */
    DriverMissing,
    /** The schema of the file does not match the one this build understands. */
    SchemaMismatch,
};
Q_ENUM_NS(ErrorCode)

/**
 * @brief An error with a machine readable code and a human readable message.
 *
 * Ownership: a value type, it is copied. The message is meant for the log and
 * for the user interface to translate into something a user can act on. It
 * carries no password, key or account number.
 */
class [[nodiscard]] Error
{
public:
    Error() = default;

    Error(ErrorCode code, QString message)
        : m_code(code)
        , m_message(std::move(message))
    {}

    [[nodiscard]] ErrorCode code() const { return m_code; }
    [[nodiscard]] QString message() const { return m_message; }
    [[nodiscard]] bool isError() const { return m_code != ErrorCode::None; }

private:
    ErrorCode m_code = ErrorCode::None;
    QString m_message;
};

} // namespace olbaflinx::core
