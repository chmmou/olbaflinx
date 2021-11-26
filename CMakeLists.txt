cmake_minimum_required(VERSION 3.17 FATAL_ERROR)
project(olbaflinx VERSION 1.0.0 LANGUAGES CXX)

set(GWENHYWFAR_MIN_VERSION 5.7.0)
set(GWENGUI_CPP_MIN_VERSION 5.7.0)
set(GWENGUI_QT5_MIN_VERSION 5.7.0)
set(AQBANKING_MIN_VERSION 6.3.0)
set(QT5_MIN_VERSION 5.15.2)
set(KTOBLZCHECK_MIN_VERSION 1.53.0)
set(LIBCHIPCARD_MIN_VERSION 5.1.6)
set(EXECUTABLE_OUTPUT_PATH "bin/")

option(BUILD_TESTS "Build test" OFF)

find_package(QT NAMES Qt6 Qt5 COMPONENTS Core Gui Widgets LinguistTools Sql Svg Network REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Core Gui Widgets LinguistTools Sql Svg Network REQUIRED)

# Check if we have the required Qt5 plugin installed
message(STATUS "Checking for qsqlcipher >= ${QT5_MIN_VERSION}")
if (NOT "Qt5::QSQLCipherDriverPlugin" IN_LIST Qt${QT_VERSION_MAJOR}Sql_PLUGINS)
    message(SEND_ERROR "QSQL Cipher Driver not installed! Please install qsqlcipher-qt5 >= ${QT5_MIN_VERSION}
    https://github.com/sjemens/qsqlcipher-qt5.git")
else ()
    message(STATUS "\tFound qsqlcipher in plugin list")
endif ()

message(STATUS "Checking for KtoBlzCheck >= ${KTOBLZCHECK_MIN_VERSION}")
find_package(KtoBlzCheck ${KTOBLZCHECK_MIN_VERSION})
if (NOT KtoBlzCheck_FOUND)
    message(SEND_ERROR "KtoBlzCheck >= ${KTOBLZCHECK_MIN_VERSION} not found! Please install KtoBlzCheck >= ${KTOBLZCHECK_MIN_VERSION}
    https://sourceforge.net/projects/ktoblzcheck/")
else ()
    message(STATUS "\tFound KtoBlzCheck >= ${KtoBlzCheck_VERSION}")
endif ()

message(STATUS "Checking for gwenhywfar ${GWENHYWFAR_MIN_VERSION}")
find_package(gwenhywfar ${GWENHYWFAR_MIN_VERSION})
if (NOT GWENHYWFAR_FOUND)
    message(SEND_ERROR "gwenhywfar >= ${GWENHYWFAR_MIN_VERSION} not found! Please install gwenhywfar >= ${GWENHYWFAR_MIN_VERSION}
    https://github.com/aqbanking/gwenhywfar.git or https://git.aquamaniac.de/git/gwenhywfar")
else ()
    message(STATUS "\tFound gwenhywfar >= ${gwenhywfar_VERSION}")
endif ()

message(STATUS "Checking for gwengui-cpp >= ${GWENGUI_CPP_MIN_VERSION}")
find_package(gwengui-cpp ${GWENGUI_CPP_MIN_VERSION})
if (NOT GWENGUI_CPP_FOUND)
    message(SEND_ERROR "gwengui-cpp >= ${GWENGUI_CPP_MIN_VERSION} not found! Please install gwengui-cpp >= ${GWENGUI_CPP_MIN_VERSION}
    https://github.com/aqbanking/gwenhywfar.git or https://git.aquamaniac.de/git/gwenhywfar")
else ()
    message(STATUS "\tFound gwengui-cpp >= ${gwengui-cpp_VERSION}")
endif ()

message(STATUS "Checking for gwengui-qt5 >= ${GWENGUI_QT5_MIN_VERSION}")
find_package(gwengui-qt5 ${GWENGUI_QT5_MIN_VERSION})
if (NOT gwengui-qt5_FOUND)
    message(SEND_ERROR "gwengui-qt5 >= ${GWENGUI_QT5_MIN_VERSION} not found! Please install gwengui-qt5 >= ${GWENGUI_QT5_MIN_VERSION}
    https://github.com/aqbanking/gwenhywfar.git or https://git.aquamaniac.de/git/gwenhywfar")
else ()
    message(STATUS "\tFound gwengui-qt5 >= ${gwengui-qt5_VERSION}")
endif ()

message(STATUS "Checking for aqbanking >= ${AQBANKING_MIN_VERSION}")
find_package(aqbanking ${AQBANKING_MIN_VERSION})
if (NOT AQBANKING_FOUND)
    message(SEND_ERROR "aqbanking >= ${AQBANKING_MIN_VERSION} not found! Please install aqbanking >= ${AQBANKING_MIN_VERSION}
    https://github.com/aqbanking/aqbanking.git or https://git.aquamaniac.de/git/aqbanking")
else ()
    message(STATUS "\tFound aqbanking >= ${aqbanking_VERSION}")
endif ()

add_subdirectory(src)

if (BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif (BUILD_TESTS)
