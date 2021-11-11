cmake_minimum_required(VERSION 3.16 FATAL_ERROR)
project(OlbaFlinxTests LANGUAGES CXX)

find_package(QT NAMES Qt6 Qt5 COMPONENTS Test Core Sql REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Test Core Sql REQUIRED)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_CXX_FLAGS "-std=c++17 ${CMAKE_CXX_FLAGS}")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(EXECUTABLE_OUTPUT_PATH bin/)

set(QT_LIBS ${Qt${QT_VERSION_MAJOR}Sql_LIBRARIES} ${Qt${QT_VERSION_MAJOR}Test_LIBRARIES} SingleApplication::SingleApplication)

set(QT_INCLUDES ${Qt${QT_VERSION_MAJOR}Sql_INCLUDE_DIRS} ${Qt${QT_VERSION_MAJOR}Test_INCLUDE_DIRS})
include_directories(${QT_INCLUDES})

add_definitions(${Qt${QT_VERSION_MAJOR}Sql_DEFINITIONS})
add_definitions(${Qt${QT_VERSION_MAJOR}Test_DEFINITIONS})

set(TEST_APP_CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../src)
file(GLOB_RECURSE APP_SRC_FILES ${TEST_APP_CORE_DIR}/core/*.cpp ${TEST_APP_CORE_DIR}/core/Banking/*.cpp ${TEST_APP_CORE_DIR}/core/Storage/*.cpp ${TEST_APP_CORE_DIR}/core/Storage/Connection/*.cpp)
file(GLOB_RECURSE APP_HDR_FILES ${TEST_APP_CORE_DIR}/core/*.h ${TEST_APP_CORE_DIR}/core/Banking/*.h ${TEST_APP_CORE_DIR}/core/Storage/*.h ${TEST_APP_CORE_DIR}/core/Storage/Connection/*.h)
set(APP_FILES ${APP_SRC_FILES} ${APP_HDR_FILES})

set(TEST_APP_RCS_FILE ${CMAKE_CURRENT_SOURCE_DIR}/../res/olbaflinx.qrc)
qt_add_resources(TEST_APP_RCS_FILE ${TEST_APP_RCS_FILE})

set(TEST_INCLUDES ${TEST_APP_CORE_DIR} ${TEST_APP_CORE_DIR}/core ${TEST_APP_CORE_DIR}/core/Banking ${TEST_APP_CORE_DIR}/core/SingleApplication ${TEST_APP_CORE_DIR}/core/Storage ${TEST_APP_CORE_DIR}/core/Storage/Connection)
include_directories(${TEST_INCLUDES})

add_executable(StorageTest core/StorageTest.cpp ${APP_FILES} ${TEST_APP_RCS_FILE})
add_test(NAME StorageTest COMMAND StorageTest)
target_link_libraries(StorageTest PRIVATE ${QT_LIBS})
