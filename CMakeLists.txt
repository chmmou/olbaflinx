#/**
# * Copyright (C) 2022-2025, Alexander Saal <developer@olbaflinx.chm-projects.de>
# *
# * This program is free software: you can redistribute it and/or modify
# * it under the terms of the GNU General Public License as published by
# * the Free Software Foundation, either version 3 of the License, or
# * (at your option) any later version.
# *
# * This program is distributed in the hope that it will be useful,
# * but WITHOUT ANY WARRANTY; without even the implied warranty of
# * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# * GNU General Public License for more details.
# *
# * You should have received a copy of the GNU General Public License
# * along with this program.  If not, see <https://www.gnu.org/licenses/>.
# */
cmake_minimum_required(VERSION 3.22 FATAL_ERROR)

project(OlbaFlinx
        VERSION 1.0.0
        DESCRIPTION "OlbaFlinx is an multibank-capable online banking software for Linux"
        HOMEPAGE_URL "https://olbaflinx.chm-projects.de"
        LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_INSTALL_PREFIX "$ENV{HOME}/.${PROJECT_NAME}")
endif ()

set(OLBAFLINX_QT_MIN_VERSION 6.8)

find_package(Qt6 ${OLBAFLINX_QT_MIN_VERSION} REQUIRED COMPONENTS
        Concurrent
        Core
        Core5Compat
        Gui
        Help
        LinguistTools
        Network
        Sql
        Svg
        Widgets
        Xml
)
qt_standard_project_setup(REQUIRES ${OLBAFLINX_QT_MIN_VERSION})

add_subdirectory(src)

option(BUILD_TESTS "Build test programs" ON)

if (BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif ()

include(CPackConfig.cmake)
