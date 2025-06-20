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

project(OlbaFlinxApp
        VERSION 1.0.0
        DESCRIPTION "OlbaFlinx is an multibank-capable online banking software for Linux"
        HOMEPAGE_URL "https://olbaflinx.chm-projects.de"
        LANGUAGES CXX
)


set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(EXECUTABLE_OUTPUT_PATH "bin/")

set(PROJECT_VERSION ${PROJECT_VERSION} PARENT_SCOPE)
set(PROJECT_VERSION_MAJOR ${PROJECT_VERSION_MAJOR} PARENT_SCOPE)
set(PROJECT_VERSION_MINOR ${PROJECT_VERSION_MINOR} PARENT_SCOPE)
set(PROJECT_VERSION_PATCH ${PROJECT_VERSION_PATCH} PARENT_SCOPE)
set(PROJECT_DESCRIPTION ${PROJECT_DESCRIPTION} PARENT_SCOPE)
set(PROJECT_HOMEPAGE_URL ${PROJECT_HOMEPAGE_URL} PARENT_SCOPE)
set(PROJECT_BUILD_TYPE ${CMAKE_BUILD_TYPE} PARENT_SCOPE)

set(QT_COMPONENTS Core Gui Widgets Concurrent LinguistTools Help Sql)
find_package(Qt6 REQUIRED COMPONENTS ${QT_COMPONENTS})
find_package(qtadvanceddocking-qt6 REQUIRED)

set(APP_RCS_FILE
        ${CMAKE_CURRENT_SOURCE_DIR}/res/app.qrc
        ${CMAKE_CURRENT_SOURCE_DIR}/res/translations.qrc
)

set(APP_TS_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/res/${PROJECT_NAME}_de_DE.ts
        ${CMAKE_CURRENT_SOURCE_DIR}/res/${PROJECT_NAME}_en_US.ts
)
set_source_files_properties(${APP_TS_FILES} PROPERTIES OUTPUT_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/res)

set(APP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
file(GLOB_RECURSE APP_SRC_FILES
        ${APP_DIR}/*.cpp
        ${APP_DIR}/**/*.cpp
)
file(GLOB_RECURSE APP_HDR_FILES
        ${APP_DIR}/*.h
        ${APP_DIR}/**/*.h
)

file(GLOB_RECURSE APP_UI_FILES
        ${APP_DIR}/ui/*.ui
        ${APP_DIR}/ui/**/*.ui
)
qt6_wrap_ui(GENERATED_UI_HDRS ${APP_UI_FILES})

set(TS_DIRS
        ${APP_DIR}
        ${APP_DIR}/ui
        ${EXPORTED_LIB_TS_DIRS}
)
qt6_add_resources(GENERATED_RESOURCES ${APP_RCS_FILE})
qt6_create_translation(GENERATED_TS ${TS_DIRS} ${APP_TS_FILES})


set(EXPORTED_APP_DIR "${CMAKE_CURRENT_BINARY_DIR}" PARENT_SCOPE)

add_executable(${PROJECT_NAME}
        ${APP_SRC_FILES}
        ${APP_HDR_FILES}
        ${GENERATED_UI_HDRS}
        ${GENERATED_RESOURCES}
        ${GENERATED_TS}
)

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "olbaflinx")

target_compile_definitions(${PROJECT_NAME} PRIVATE
        QT_DISABLE_DEPRECATED_BEFORE=0x060500
)

target_include_directories(${PROJECT_NAME}
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../lib/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Assistant
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Assistant/Pages
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Help
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Storage
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/Assistant
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/Storage
)

target_link_libraries(${PROJECT_NAME}
        PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Help
        Qt6::Sql
        Qt6::Concurrent
        OlbaFlinxCore::OlbaFlinxCore
        ads::qtadvanceddocking-qt6
)

install(TARGETS ${PROJECT_NAME} DESTINATION bin)
