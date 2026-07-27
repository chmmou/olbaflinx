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

foreach (test_target OlbaFlinxTests TransactionTests StorageLifetimeTests)
    # Fuer "BaseTest.h", das neben den Testquellen liegt.
    target_include_directories(${test_target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/core)

    target_compile_options(${test_target} PRIVATE ${OLBAFLINX_WARNINGS})
    target_compile_definitions(${test_target} PRIVATE ${OLBAFLINX_QT_DEPRECATION})

    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        target_link_libraries(${test_target} PRIVATE gcov)
    endif ()
endforeach ()

# QTEST_MAIN erzeugt eine QGuiApplication, sobald Qt6::Gui gelinkt ist.
target_link_libraries(OlbaFlinxTests PRIVATE
        OlbaFlinxCore::OlbaFlinxCore
        Qt6::Core
        Qt6::Gui
        Qt6::Test
)

# QTEST_APPLESS_MAIN, beide Ziele brauchen keine Anzeige.
target_link_libraries(TransactionTests PRIVATE
        OlbaFlinxCore::OlbaFlinxCore
        Qt6::Core
        Qt6::Test
)

target_link_libraries(StorageLifetimeTests PRIVATE
        OlbaFlinxCore::OlbaFlinxCore
        Qt6::Core
        Qt6::Test
)
### Adding tests here
