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

#pragma once

#include "core/OlbaFlinxCore.h"

template<typename S> class OLBAFLINX_CORE_EXPORT Singleton
{
public:
    static S *instance()
    {
        if (_instance == nullptr) {
            _instance = new S();
        }

        return _instance;
    }

    virtual ~Singleton() { delete _instance; _instance = nullptr; }

private:
    static S *_instance;

protected:
    Singleton() = default;
};

template<typename S> OLBAFLINX_CORE_EXPORT S *Singleton<S>::_instance = nullptr;
