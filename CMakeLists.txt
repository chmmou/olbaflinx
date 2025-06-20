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

project(OlbaFlinxProject)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_INSTALL_PREFIX "$ENV{HOME}/.${PROJECT_NAME}")
endif ()

set(PROJECT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(${PROJECT_DIR}/lib)
add_subdirectory(${PROJECT_DIR}/app)

add_dependencies(OlbaFlinxApp OlbaFlinxCore)

option(BUILD_TESTS "Build test programs" ON)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(${PROJECT_DIR}/tests)
    add_dependencies(OlbaFlinxTests OlbaFlinxCore)
endif(BUILD_TESTS)

include(CPackConfig.cmake)