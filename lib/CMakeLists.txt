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

project(OlbaFlinxCore
        VERSION 1.0.0
        DESCRIPTION "OlbaFlinxCore core library"
        LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(LIBRARY_OUTPUT_PATH "bin/")

set(LIB_VERSION 1.0.0)
set(LIB_SOVERSION 1)

# Qt dependencies
set(QT_COMPONENTS Core Gui Widgets Concurrent Core5Compat LinguistTools Sql Svg Xml Network)
find_package(Qt6 REQUIRED COMPONENTS ${QT_COMPONENTS})

# AqBanking dependencies
set(GWENHYWFAR_MIN_VERSION 5.12)
set(GWENGUI_QT_MIN_VERSION 5.12)
set(AQBANKING_MIN_VERSION 6.6)
set(LIBCHIPCARD_MIN_VERSION 5.1)

include(GNUInstallDirs)
find_package(PkgConfig REQUIRED)

pkg_check_modules(PKG_GWENHYWFAR REQUIRED IMPORTED_TARGET gwenhywfar>=${GWENHYWFAR_MIN_VERSION})
pkg_check_modules(PKG_GWENHYWFAR_QT REQUIRED IMPORTED_TARGET gwengui-qt6>=${GWENGUI_QT_MIN_VERSION})
pkg_check_modules(PKG_AQBANKING REQUIRED IMPORTED_TARGET aqbanking>=${AQBANKING_MIN_VERSION})
pkg_check_modules(PKG_LIBCHIPCARD_CLIENT REQUIRED IMPORTED_TARGET libchipcard-client>=${LIBCHIPCARD_MIN_VERSION})

set(LIB_RCS_FILES ${CMAKE_CURRENT_SOURCE_DIR}/res/${PROJECT_NAME}.qrc)
qt6_add_resources(GENERATED_RESOURCES ${LIB_RCS_FILES})

set(LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

file(GLOB_RECURSE LIB_SRC_FILES
        ${LIB_DIR}/*.cpp
        ${LIB_DIR}/Banking/*.cpp
        ${LIB_DIR}/Banking/**/*.cpp
        ${LIB_DIR}/Logger/*.cpp
        ${LIB_DIR}/Logger/**/*.cpp
        ${LIB_DIR}/Storage/*.cpp
        ${LIB_DIR}/Storage/**/*.cpp
        ${LIB_DIR}/Themes/*.cpp
        ${LIB_DIR}/Themes/**/*.cpp
)

file(GLOB_RECURSE LIB_HDR_FILES
        ${LIB_DIR}/*.h
        ${LIB_DIR}/Banking/*.h
        ${LIB_DIR}/Banking/**/*.h
        ${LIB_DIR}/Logger/*.h
        ${LIB_DIR}/Logger/**/*.h
        ${LIB_DIR}/Storage/*.h
        ${LIB_DIR}/Storage/**/*.h
        ${LIB_DIR}/Themes/*.h
        ${LIB_DIR}/Themes/**/*.h
)

set(LIB_TS_DIRS
        ${LIB_DIR}
        ${LIB_DIR}/Banking
        ${LIB_DIR}/Banking/Account
        ${LIB_DIR}/Banking/Transaction
        ${LIB_DIR}/Logger
        ${LIB_DIR}/Storage
        ${LIB_DIR}/Themes
)

set(EXPORTED_LIB_TS_SRC_FILES ${LIB_SRC_FILES} PARENT_SCOPE)
set(EXPORTED_LIB_TS_DIRS ${LIB_TS_DIRS} PARENT_SCOPE)

add_custom_target(ResourceFiles
    SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/res/database/olbaflinx_storage.sql
    ${CMAKE_CURRENT_SOURCE_DIR}/res/themes/darktheme.qss
)

add_library(${PROJECT_NAME}
        STATIC
        ${LIB_SRC_FILES}
        ${LIB_HDR_FILES}
        ${GENERATED_RESOURCES}
)

add_library(${PROJECT_NAME}::${PROJECT_NAME}
        ALIAS
        ${PROJECT_NAME}
)

set_target_properties(${PROJECT_NAME} PROPERTIES
        OUTPUT_NAME ${PROJECT_NAME}
        VERSION ${LIB_VERSION}
        SOVERSION ${LIB_SOVERSION}
        PUBLIC_HEADER "${LIB_HDR_FILES}"
)

target_include_directories(${PROJECT_NAME}
        PUBLIC
        ${LIB_DIR}
        ${PKG_GWENHYWFAR_INCLUDE_DIRS}
        ${PKG_GWENHYWFAR_QT5_INCLUDE_DIRS}
        ${PKG_AQBANKING_INCLUDE_DIRS}
        ${PKG_LIBCHIPCARD_CLIENT_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME}
        PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Concurrent
        Qt6::Core5Compat
        Qt6::Sql
        Qt6::Svg
        Qt6::Xml
        Qt6::Network
        PkgConfig::PKG_GWENHYWFAR
        PkgConfig::PKG_GWENHYWFAR_QT
        PkgConfig::PKG_AQBANKING
        PkgConfig::PKG_LIBCHIPCARD_CLIENT
)

target_compile_definitions(${PROJECT_NAME} PUBLIC
        QT_DEPRECATED_WARNINGS
        QT_DISABLE_DEPRECATED_BEFORE=0x060500
        BUILD_OLBAFLINX_CORE_LIB
)

install(TARGETS ${PROJECT_NAME}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/olbaflinx
)
