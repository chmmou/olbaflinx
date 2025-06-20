#/**
# * Copyright (C) 2021-2025, Alexander Saal <developer@olbaflinx.chm-projects.de>
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

project(OlbaFlinxTests VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(QT_COMPONENTS Test Core Gui)
find_package(Qt6 REQUIRED COMPONENTS ${QT_COMPONENTS})

set(QT_LIBS
        ${Qt6Core_LIBRARIES}
        ${Qt6Gui_LIBRARIES}
        ${Qt6Test_LIBRARIES}
)

set(QT_INCLUDES
        ${Qt6Core_INCLUDE_DIRS}
        ${Qt6Gui_INCLUDE_DIRS}
        ${Qt6Test_INCLUDE_DIRS}
)

include_directories(${QT_INCLUDES})

set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(EXECUTABLE_OUTPUT_PATH bin/)

include_directories(${EXPORTED_LIB_TS_DIRS})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib)

### Adding tests here
add_executable(${PROJECT_NAME} lib/StorageTest.cpp)
add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} PRIVATE ${EXPORTED_LIB_TS_DIRS})

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_compile_options(-fprofile-arcs -ftest-coverage)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${QT_LIBS} OlbaFlinxCore::OlbaFlinxCore gcov)
else ()
    target_link_libraries(${PROJECT_NAME} PRIVATE ${QT_LIBS} OlbaFlinxCore::OlbaFlinxCore)
endif ()
### Adding tests here
