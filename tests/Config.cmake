find_package(PkgConfig)
find_package(KtoBlzCheck 1.53)
find_package(gwenhywfar 5.7)
find_package(gwengui-cpp 5.7)
find_package(gwengui-qt5 5.7)
find_package(aqbanking 6.3)
pkg_check_modules(libchipcard-client REQUIRED IMPORTED_TARGET libchipcard-client>=${LIBCHIPCARD_MIN_VERSION})

set(QT_LIBS ${Qt${QT_VERSION_MAJOR}Sql_LIBRARIES} ${Qt${QT_VERSION_MAJOR}Test_LIBRARIES} SingleApplication::SingleApplication)

set(QT_INCLUDES ${Qt${QT_VERSION_MAJOR}Sql_INCLUDE_DIRS} ${Qt${QT_VERSION_MAJOR}Test_INCLUDE_DIRS})
include_directories(${QT_INCLUDES})

set(AQ_LIBS ${KtoBlzCheck_LIBRARIES} aqbanking::aqbanking gwenhywfar::core gwenhywfar::gui-cpp gwenhywfar::gui-qt5 ${libchipcard-client_LIBRARIES})
set(AQ_INCLUDES ${AQBANKING_INCLUDE_DIRS} ${GWENHYWFAR_INCLUDE_DIRS} ${libchipcard-client_INCLUDE_DIRS})
include_directories(${AQ_INCLUDES})

add_definitions(${Qt${QT_VERSION_MAJOR}Sql_DEFINITIONS})
add_definitions(${Qt${QT_VERSION_MAJOR}Test_DEFINITIONS})

set(TEST_APP_CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../src)
file(
        GLOB_RECURSE APP_SRC_FILES
        ${TEST_APP_CORE_DIR}/core/*.cpp
        ${TEST_APP_CORE_DIR}/core/Banking/*.cpp
        ${TEST_APP_CORE_DIR}/core/Banking/Private/*.cpp
        ${TEST_APP_CORE_DIR}/core/Storage/*.cpp
        ${TEST_APP_CORE_DIR}/core/Storage/Account/*.cpp
        ${TEST_APP_CORE_DIR}/core/Storage/Connection/*.cpp
        ${TEST_APP_CORE_DIR}/core/Storage/Private/*.cpp
)
file(
        GLOB_RECURSE APP_HDR_FILES
        ${TEST_APP_CORE_DIR}/core/*.h
        ${TEST_APP_CORE_DIR}/core/Banking/*.h
        ${TEST_APP_CORE_DIR}/core/Banking/Private/*.h
        ${TEST_APP_CORE_DIR}/core/Storage/*.h
        ${TEST_APP_CORE_DIR}/core/Storage/Account/*.h
        ${TEST_APP_CORE_DIR}/core/Storage/Connection/*.h
        ${TEST_APP_CORE_DIR}/core/Storage/Private/*.h
)
set(APP_FILES ${APP_SRC_FILES} ${APP_HDR_FILES})

set(TEST_APP_RCS_FILE ${TEST_APP_CORE_DIR}/../res/olbaflinx.qrc)
qt_add_resources(TEST_APP_RCS_FILE ${TEST_APP_RCS_FILE})

set(
        TEST_INCLUDES
        ${TEST_APP_CORE_DIR}
        ${TEST_APP_CORE_DIR}/core
        ${TEST_APP_CORE_DIR}/core/Banking
        ${TEST_APP_CORE_DIR}/core/Banking/Private
        ${TEST_APP_CORE_DIR}/core/SingleApplication
        ${TEST_APP_CORE_DIR}/core/Storage
        ${TEST_APP_CORE_DIR}/core/Storage/Connection
        ${TEST_APP_CORE_DIR}/core/Storage/Private
)
include_directories(${TEST_INCLUDES})
