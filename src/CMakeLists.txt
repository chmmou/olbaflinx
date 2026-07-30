#/**
# * Copyright (C) 2022-2026, Alexander Saal <developer@olbaflinx.chm-projects.de>
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
        core/ApplicationInfo.h
        core/Error.h
        core/Logging.cpp
        core/Logging.h
        core/Result.h
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

# src/ is the include root. Project internal includes therefore read
# "core/..." and "ui/..." respectively.
target_include_directories(OlbaFlinxCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(OlbaFlinxCore
        PUBLIC
        Qt6::Core
        Qt6::Sql
        PkgConfig::PKG_GWENHYWFAR
        PkgConfig::PKG_AQBANKING
        PRIVATE
        Qt6::Concurrent
        Qt6::Network
        Qt6::Xml
        PkgConfig::PKG_GWENHYWFAR_QT
        PkgConfig::PKG_LIBCHIPCARD_CLIENT
)

target_compile_definitions(OlbaFlinxCore
        PUBLIC
        BUILD_OLBAFLINX_CORE_LIB
        PRIVATE
        ${OLBAFLINX_QT_DEPRECATION}
)

target_compile_options(OlbaFlinxCore PRIVATE ${OLBAFLINX_WARNINGS})

install(TARGETS OlbaFlinxCore
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# ----------------------------------------------------------------------------
# OlbaFlinxUi, GPLv3, see src/ui/LICENSE
#
# A target of its own so that the presentation models and the dialogs can be
# linked by the tests. An executable cannot be linked against.
# ----------------------------------------------------------------------------

qt_add_library(OlbaFlinxUi STATIC
        ui/App.cpp
        ui/App.h
        ui/App.ui
        ui/AppCentralWidget.cpp
        ui/AppCentralWidget.h
        ui/AppCentralWidget.ui
        ui/ErrorMessage.cpp
        ui/ErrorMessage.h
        ui/Logging.cpp
        ui/Logging.h
        ui/Assistant/SetupAssistant.cpp
        ui/Assistant/SetupAssistant.h
        ui/Assistant/SetupAssistant.ui
        ui/Assistant/ImExportAssistant.ui
        ui/Assistant/Pages/OptionBankingPage.cpp
        ui/Assistant/Pages/OptionBankingPage.h
        ui/Assistant/Pages/OptionBankingPage.ui
        ui/Models/AccountListModel.cpp
        ui/Models/AccountListModel.h
        ui/Models/TransactionListModel.cpp
        ui/Models/TransactionListModel.h
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
)

add_library(OlbaFlinxUi::OlbaFlinxUi ALIAS OlbaFlinxUi)

target_compile_definitions(OlbaFlinxUi PRIVATE ${OLBAFLINX_QT_DEPRECATION})
target_compile_options(OlbaFlinxUi PRIVATE ${OLBAFLINX_WARNINGS})

target_link_libraries(OlbaFlinxUi
        PUBLIC
        OlbaFlinxCore::OlbaFlinxCore
        Qt6::Widgets
        PRIVATE
        Qt6::Gui
        Qt6::Svg
        ads::qtadvanceddocking-qt6
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
        ${APP_RESOURCES}
        ${APP_GENERATED_TS}
)

set_target_properties(OlbaFlinxApp PROPERTIES OUTPUT_NAME "olbaflinx")

target_compile_definitions(OlbaFlinxApp PRIVATE
        ${OLBAFLINX_QT_DEPRECATION}
        OLBAFLINX_VERSION="${PROJECT_VERSION}"
)

target_compile_options(OlbaFlinxApp PRIVATE ${OLBAFLINX_WARNINGS})

target_link_libraries(OlbaFlinxApp PRIVATE
        OlbaFlinxUi::OlbaFlinxUi
        Qt6::Help
        Qt6::Widgets
)

install(TARGETS OlbaFlinxApp DESTINATION bin)
