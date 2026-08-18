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

#include "core/Error.h"

#include <optional>
#include <utility>

namespace olbaflinx::core {

/**
 * Brings a value and an error together for functions that return
 * something and can fail.
 *
 * The caller checks hasValue() before it reads value(). Reading the value of a
 * result that carries an error is a programming error.
 */
template<typename T>
class [[nodiscard]] Result
{
public:
    // Deliberately not explicit, against the usual rule for single argument
    // constructors, so that one function can write both "return value;" and
    // "return Error(...);".
    Result(T value)
        : m_value(std::move(value))
    {}

    // Deliberately not explicit, see above.
    Result(Error error)
        : m_error(std::move(error))
    {}

    [[nodiscard]] bool hasValue() const { return m_value.has_value(); }
    [[nodiscard]] const T &value() const { return *m_value; }
    [[nodiscard]] const Error &error() const { return m_error; }

private:
    std::optional<T> m_value;
    Error m_error;
};

} // namespace olbaflinx::core
