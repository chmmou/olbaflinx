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

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage")

find_package(Qt6 ${OLBAFLINX_QT_MIN_VERSION} REQUIRED COMPONENTS Test)

### Adding tests here
qt_add_executable(OlbaFlinxTests core/StorageTest.cpp)
add_test(NAME OlbaFlinxTests COMMAND OlbaFlinxTests)

qt_add_executable(TransactionTests core/tst_transaction.cpp)
add_test(NAME TransactionTests COMMAND TransactionTests)

qt_add_executable(StorageLifetimeTests core/tst_storage_lifetime.cpp)
add_test(NAME StorageLifetimeTests COMMAND StorageLifetimeTests)

qt_add_executable(StorageGuidelinesTests core/tst_storage.cpp)
add_test(NAME StorageGuidelinesTests COMMAND StorageGuidelinesTests)

qt_add_executable(BankingTests core/tst_banking.cpp)
add_test(NAME BankingTests COMMAND BankingTests)

qt_add_executable(StorageErrorTests core/tst_storage_errors.cpp)
add_test(NAME StorageErrorTests COMMAND StorageErrorTests)

qt_add_executable(StorageKeyTests core/tst_storage_key.cpp)
add_test(NAME StorageKeyTests COMMAND StorageKeyTests)

qt_add_executable(AppErrorHandlingTests ui/tst_apperrorhandling.cpp)
add_test(NAME AppErrorHandlingTests COMMAND AppErrorHandlingTests)

qt_add_executable(AccountListModelTests ui/tst_accountlistmodel.cpp)
add_test(NAME AccountListModelTests COMMAND AccountListModelTests)

qt_add_executable(TransactionListModelTests ui/tst_transactionlistmodel.cpp)
add_test(NAME TransactionListModelTests COMMAND TransactionListModelTests)

qt_add_executable(StorageDialogTests ui/tst_storagedialog.cpp)
add_test(NAME StorageDialogTests COMMAND StorageDialogTests)

set(OLBAFLINX_CORE_TESTS
        OlbaFlinxTests
        TransactionTests
        StorageLifetimeTests
        StorageGuidelinesTests
        StorageErrorTests
        StorageKeyTests
        BankingTests
)
set(OLBAFLINX_UI_TESTS
        AccountListModelTests
        TransactionListModelTests
        StorageDialogTests
        AppErrorHandlingTests
)

foreach (test_target ${OLBAFLINX_CORE_TESTS} ${OLBAFLINX_UI_TESTS})
    # For "BaseTest.h", which sits next to the test sources.
    target_include_directories(${test_target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/core)

    target_compile_options(${test_target} PRIVATE ${OLBAFLINX_WARNINGS})
    target_compile_definitions(${test_target} PRIVATE ${OLBAFLINX_QT_DEPRECATION})

    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        target_link_libraries(${test_target} PRIVATE gcov)
    endif ()
endforeach ()

# The core targets link against Qt6::Core, Qt6::Test and OlbaFlinxCore only.
# Adding Qt6::Widgets here would break the layering of core.
foreach (test_target ${OLBAFLINX_CORE_TESTS})
    target_link_libraries(${test_target} PRIVATE
            OlbaFlinxCore::OlbaFlinxCore
            Qt6::Core
            Qt6::Test
    )
endforeach ()

# The ui targets need Widgets. They run without a display.
foreach (test_target ${OLBAFLINX_UI_TESTS})
    target_link_libraries(${test_target} PRIVATE
            OlbaFlinxUi::OlbaFlinxUi
            Qt6::Core
            Qt6::Test
            Qt6::Widgets
    )

    set_tests_properties(${test_target} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endforeach ()
### Adding tests here
