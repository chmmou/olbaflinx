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

#include "ui/ErrorMessage.h"

#include <QtCore/QCoreApplication>

using namespace olbaflinx::core;

QString olbaflinx::ui::userMessage(const ErrorCode code)
{
    switch (code) {
    case ErrorCode::None:
        return {};
    case ErrorCode::NotFound:
        return QCoreApplication::translate("olbaflinx::ui",
                                           "Nothing was found. Import your accounts to fill the "
                                           "storage.");
    case ErrorCode::PermissionDenied:
        return QCoreApplication::translate("olbaflinx::ui",
                                           "The password does not open this storage. Check it and "
                                           "try again.");
    case ErrorCode::InvalidInput:
        return QCoreApplication::translate("olbaflinx::ui",
                                           "A record could not be used. It is incomplete and was "
                                           "not stored.");
    case ErrorCode::IoFailure:
        return QCoreApplication::translate("olbaflinx::ui",
                                           "A file could not be read or written. Check the path "
                                           "and its permissions.");
    case ErrorCode::DatabaseFailure:
        return QCoreApplication::translate("olbaflinx::ui",
                                           "The storage could not be used. Open it again or "
                                           "restore a backup.");
    case ErrorCode::BankingFailure:
        return QCoreApplication::translate("olbaflinx::ui",
                                           "The banking backend reported a failure. Check the "
                                           "setup of your accounts.");
    case ErrorCode::NotImplemented:
        return QCoreApplication::translate("olbaflinx::ui",
                                           "This part of the application is not finished yet.");
    case ErrorCode::DriverMissing:
        return QCoreApplication::translate("olbaflinx::ui",
                                           "The database component this program needs is missing "
                                           "from the installation.");
    case ErrorCode::SchemaMismatch:
        return QCoreApplication::translate("olbaflinx::ui",
                                           "This storage was written by a different version of "
                                           "the program and cannot be opened.");
    case ErrorCode::Busy:
        return QCoreApplication::translate("olbaflinx::ui",
                                           "The storage is busy with another run. Try again in a "
                                           "moment.");
    }

    return {};
}
