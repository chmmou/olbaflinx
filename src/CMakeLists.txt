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

# AqBanking dependencies
set(GWENHYWFAR_MIN_VERSION 5.12)
set(GWENGUI_QT_MIN_VERSION 5.12)
set(AQBANKING_MIN_VERSION 6.6)
set(LIBCHIPCARD_MIN_VERSION 5.1)

include(GNUInstallDirs)
find_package(PkgConfig REQUIRED)
find_package(qtadvanceddocking-qt6 REQUIRED)

pkg_check_modules(PKG_GWENHYWFAR REQUIRED IMPORTED_TARGET gwenhywfar>=${GWENHYWFAR_MIN_VERSION})
pkg_check_modules(PKG_GWENHYWFAR_QT REQUIRED IMPORTED_TARGET gwengui-qt6>=${GWENGUI_QT_MIN_VERSION})
pkg_check_modules(PKG_AQBANKING REQUIRED IMPORTED_TARGET aqbanking>=${AQBANKING_MIN_VERSION})
pkg_check_modules(PKG_LIBCHIPCARD_CLIENT REQUIRED IMPORTED_TARGET libchipcard-client>=${LIBCHIPCARD_MIN_VERSION})

set(OLBAFLINX_RES_DIR ${CMAKE_SOURCE_DIR}/res)

# ----------------------------------------------------------------------------
# OlbaFlinxCore, LGPLv3, see src/core/LICENSE
# ----------------------------------------------------------------------------

qt_add_resources(CORE_RESOURCES ${OLBAFLINX_RES_DIR}/OlbaFlinxCore.qrc)

qt_add_library(OlbaFlinxCore STATIC
        core/OlbaFlinxCore.h
        core/Singleton.h
        core/Banking/Banking.cpp
        core/Banking/Banking.h
        core/Banking/BankingItem.h
        core/Banking/Account/Account.cpp
        core/Banking/Account/Account.h
        core/Banking/Account/ReferenceAccount.cpp
        core/Banking/Account/ReferenceAccount.h
        core/Banking/Transaction/Transaction.cpp
        core/Banking/Transaction/Transaction.h
        core/Logger/Logger.cpp
        core/Logger/Logger.h
        core/Storage/Storage.cpp
        core/Storage/Storage.h
        core/Storage/StorageConnection.cpp
        core/Storage/StorageConnection.h
        ${CORE_RESOURCES}
)

add_library(OlbaFlinxCore::OlbaFlinxCore ALIAS OlbaFlinxCore)

set_target_properties(OlbaFlinxCore PROPERTIES
        OUTPUT_NAME OlbaFlinxCore
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
)

# src/ ist die Include-Wurzel. Projektinterne Includes lauten damit
# "core/..." beziehungsweise "ui/...".
target_include_directories(OlbaFlinxCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(OlbaFlinxCore
        PUBLIC
        Qt6::Core
        Qt6::Sql
        PkgConfig::PKG_GWENHYWFAR
        PkgConfig::PKG_AQBANKING
        PRIVATE
        Qt6::Concurrent
        Qt6::Core5Compat
        Qt6::Network
        Qt6::Xml
        # Storage und Banking greifen noch auf QApplication zu. Solange das so ist,
        # bleiben beide Module an core.
        Qt6::Gui
        Qt6::Widgets
        PkgConfig::PKG_GWENHYWFAR_QT
        PkgConfig::PKG_LIBCHIPCARD_CLIENT
)

target_compile_definitions(OlbaFlinxCore PUBLIC
        QT_DEPRECATED_WARNINGS
        QT_DISABLE_DEPRECATED_BEFORE=0x060500
        BUILD_OLBAFLINX_CORE_LIB
)

install(TARGETS OlbaFlinxCore
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# ----------------------------------------------------------------------------
# OlbaFlinxApp, GPLv3, see src/ui/LICENSE
# ----------------------------------------------------------------------------

set(APP_TS_FILES
        ${OLBAFLINX_RES_DIR}/i18n/OlbaFlinxApp_de_DE.ts
        ${OLBAFLINX_RES_DIR}/i18n/OlbaFlinxApp_en_US.ts
)
set_source_files_properties(${APP_TS_FILES} PROPERTIES
        OUTPUT_LOCATION ${OLBAFLINX_RES_DIR}/i18n
)

qt_add_resources(APP_RESOURCES
        ${OLBAFLINX_RES_DIR}/app.qrc
        ${OLBAFLINX_RES_DIR}/i18n/translations.qrc
)
qt6_create_translation(APP_GENERATED_TS ${CMAKE_CURRENT_SOURCE_DIR} ${APP_TS_FILES})

qt_add_executable(OlbaFlinxApp
        main.cpp
        ui/App.cpp
        ui/App.h
        ui/App.ui
        ui/AppCentralWidget.cpp
        ui/AppCentralWidget.h
        ui/AppCentralWidget.ui
        ui/Assistant/SetupAssistant.cpp
        ui/Assistant/SetupAssistant.h
        ui/Assistant/SetupAssistant.ui
        ui/Assistant/ImExportAssistant.ui
        ui/Assistant/Pages/OptionBankingPage.cpp
        ui/Assistant/Pages/OptionBankingPage.h
        ui/Assistant/Pages/OptionBankingPage.ui
        ui/Storage/NewStorageDialog.cpp
        ui/Storage/NewStorageDialog.h
        ui/Storage/NewStorageDialog.ui
        ui/Storage/NewStorageItem.cpp
        ui/Storage/NewStorageItem.h
        ui/Storage/NewStorageItem.ui
        ui/Storage/StorageDialog.cpp
        ui/Storage/StorageDialog.h
        ui/Themes/ThemeManager.cpp
        ui/Themes/ThemeManager.h
        ui/Themes/ThemeManagerIconNames.h
        ${APP_RESOURCES}
        ${APP_GENERATED_TS}
)

set_target_properties(OlbaFlinxApp PROPERTIES OUTPUT_NAME "olbaflinx")

target_compile_definitions(OlbaFlinxApp PRIVATE
        QT_DISABLE_DEPRECATED_BEFORE=0x060500
)

target_link_libraries(OlbaFlinxApp PRIVATE
        OlbaFlinxCore::OlbaFlinxCore
        Qt6::Help
        Qt6::Svg
        Qt6::Widgets
        ads::qtadvanceddocking-qt6
)

install(TARGETS OlbaFlinxApp DESTINATION bin)
