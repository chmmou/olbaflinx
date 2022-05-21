cmake_minimum_required(VERSION 3.17 FATAL_ERROR)
project(OlbaFlinxTests LANGUAGES CXX)

find_package(QT NAMES Qt6 Qt5 COMPONENTS Test Core Gui Concurrent Sql Svg Xml Network REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Test Core Gui Concurrent Sql Svg Xml Network REQUIRED)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_CXX_FLAGS "-std=c++17 ${CMAKE_CXX_FLAGS} --coverage")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(EXECUTABLE_OUTPUT_PATH bin/)

include("Config.cmake")

add_compile_definitions(QT_DEPRECATED_WARNINGS)
add_compile_definitions(QT_DISABLE_DEPRECATED_BEFORE=0x050E20)

### Adding tests here
add_executable(BankingTest core/BankingTest.cpp ${APP_FILES} ${TEST_APP_RCS_FILE})
add_test(NAME BankingTest COMMAND BankingTest)
target_link_libraries(BankingTest PRIVATE ${QT_LIBS} ${AQ_LIBS})

add_executable(StorageTest core/StorageTest.cpp ${APP_FILES} ${TEST_APP_RCS_FILE})
add_test(NAME StorageTest COMMAND StorageTest)
target_link_libraries(StorageTest PRIVATE ${QT_LIBS} ${AQ_LIBS})
### Adding tests here

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_compile_options(--coverage)
    #Uncomment in case of linker errors
    link_libraries(gcov)
endif ()
