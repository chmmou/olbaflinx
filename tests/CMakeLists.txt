#/**
# * Copyright (C) 2021-2026, Alexander Saal <developer@olbaflinx.chm-projects.de>
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

# Coverage slows every run down and drops a .gcda file next to every object
# file. It used to be switched on unconditionally, in every configuration.
option(ENABLE_COVERAGE "Build the tests with coverage instrumentation" OFF)

find_package(Qt6 ${OLBAFLINX_QT_MIN_VERSION} REQUIRED COMPONENTS Test)

# Every test is a target of its own with a CTest entry of its own, so that a
# failure names the unit it belongs to. Additional arguments are the libraries
# the test links beyond Qt6::Core and Qt6::Test.
function(olbaflinx_add_test name source)
    qt_add_executable(${name} ${source})

    # For "TestHelpers.h", which sits next to the core test sources.
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/core)

    target_compile_options(${name} PRIVATE ${OLBAFLINX_WARNINGS})
    target_compile_definitions(${name} PRIVATE ${OLBAFLINX_QT_DEPRECATION})

    target_link_libraries(${name} PRIVATE Qt6::Core Qt6::Test ${ARGN})

    if (ENABLE_COVERAGE AND "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        target_compile_options(${name} PRIVATE --coverage)
        target_link_options(${name} PRIVATE --coverage)
    endif ()

    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endfunction()

### Adding tests here

# The core targets link against Qt6::Core, Qt6::Test and OlbaFlinxCore only.
# Adding Qt6::Widgets here would break the layering of core.
olbaflinx_add_test(tst_storage core/tst_storage.cpp OlbaFlinxCore::OlbaFlinxCore)
olbaflinx_add_test(tst_storage_key core/tst_storage_key.cpp OlbaFlinxCore::OlbaFlinxCore)
olbaflinx_add_test(tst_storage_errors core/tst_storage_errors.cpp OlbaFlinxCore::OlbaFlinxCore)
olbaflinx_add_test(tst_storage_lifetime core/tst_storage_lifetime.cpp OlbaFlinxCore::OlbaFlinxCore)
olbaflinx_add_test(tst_banking core/tst_banking.cpp OlbaFlinxCore::OlbaFlinxCore)
olbaflinx_add_test(tst_banking_fetch core/tst_banking_fetch.cpp OlbaFlinxCore::OlbaFlinxCore)
olbaflinx_add_test(tst_account core/tst_account.cpp OlbaFlinxCore::OlbaFlinxCore)
olbaflinx_add_test(tst_referenceaccount core/tst_referenceaccount.cpp OlbaFlinxCore::OlbaFlinxCore)
olbaflinx_add_test(tst_transaction core/tst_transaction.cpp OlbaFlinxCore::OlbaFlinxCore)
olbaflinx_add_test(tst_logger core/tst_logger.cpp OlbaFlinxCore::OlbaFlinxCore)

# The ui targets need Widgets. They run without a display.
olbaflinx_add_test(tst_accounttreemodel ui/tst_accounttreemodel.cpp
        OlbaFlinxUi::OlbaFlinxUi Qt6::Widgets)
olbaflinx_add_test(tst_transactiontablemodel ui/tst_transactiontablemodel.cpp
        OlbaFlinxUi::OlbaFlinxUi Qt6::Widgets)
olbaflinx_add_test(tst_storagedialog ui/tst_storagedialog.cpp
        OlbaFlinxUi::OlbaFlinxUi Qt6::Widgets)
olbaflinx_add_test(tst_apperrorhandling ui/tst_apperrorhandling.cpp
        OlbaFlinxUi::OlbaFlinxUi Qt6::Widgets)
olbaflinx_add_test(tst_thememanager ui/tst_thememanager.cpp
        OlbaFlinxUi::OlbaFlinxUi Qt6::Widgets)
olbaflinx_add_test(tst_setupassistant ui/tst_setupassistant.cpp
        OlbaFlinxUi::OlbaFlinxUi Qt6::Widgets)
olbaflinx_add_test(tst_appcentralwidget ui/tst_appcentralwidget.cpp
        OlbaFlinxUi::OlbaFlinxUi Qt6::Widgets)
olbaflinx_add_test(tst_newstoragedialog ui/tst_newstoragedialog.cpp
        OlbaFlinxUi::OlbaFlinxUi Qt6::Widgets)
olbaflinx_add_test(tst_appaccessibility ui/tst_appaccessibility.cpp
        OlbaFlinxUi::OlbaFlinxUi Qt6::Widgets)
olbaflinx_add_test(tst_appkeyboard ui/tst_appkeyboard.cpp
        OlbaFlinxUi::OlbaFlinxUi Qt6::Widgets)

# The only test that reaches for the docking library itself. OlbaFlinxUi links
# it privately, which carries the symbols but not the right to name its types.
olbaflinx_add_test(tst_appdocklayout ui/tst_appdocklayout.cpp
        OlbaFlinxUi::OlbaFlinxUi Qt6::Widgets ads::qtadvanceddocking-qt6)
### Adding tests here
