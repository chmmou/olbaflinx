find_package(KtoBlzCheck 1.53)
find_package(gwenhywfar 5.7)
find_package(gwengui-cpp 5.7)
find_package(gwengui-qt5 5.7)
find_package(aqbanking 6.3)

set(
        QT_LIBS
        ${Qt${QT_VERSION_MAJOR}Core_LIBRARIES}
        ${Qt${QT_VERSION_MAJOR}Concurrent_LIBRARIES}
        ${Qt${QT_VERSION_MAJOR}Gui_LIBRARIES}
        ${Qt${QT_VERSION_MAJOR}Network_LIBRARIES}
        ${Qt${QT_VERSION_MAJOR}Sql_LIBRARIES}
        ${Qt${QT_VERSION_MAJOR}Svg_LIBRARIES}
        ${Qt${QT_VERSION_MAJOR}Xml_LIBRARIES}
        ${Qt${QT_VERSION_MAJOR}Test_LIBRARIES}
        SingleApplication::SingleApplication
)

set(
        QT_INCLUDES
        ${Qt${QT_VERSION_MAJOR}Core_INCLUDE_DIRS}
        ${Qt${QT_VERSION_MAJOR}Gui_INCLUDE_DIRS}
        ${Qt${QT_VERSION_MAJOR}Concurrent_INCLUDE_DIRS}
        ${Qt${QT_VERSION_MAJOR}Network_INCLUDE_DIRS}
        ${Qt${QT_VERSION_MAJOR}Sql_INCLUDE_DIRS}
        ${Qt${QT_VERSION_MAJOR}Svg_INCLUDE_DIRS}
        ${Qt${QT_VERSION_MAJOR}Xml_INCLUDE_DIRS}
        ${Qt${QT_VERSION_MAJOR}Test_INCLUDE_DIRS}
)

include_directories(${QT_INCLUDES})

set(AQ_LIBS ${KtoBlzCheck_LIBRARIES} aqbanking::aqbanking gwenhywfar::core gwenhywfar::gui-cpp gwenhywfar::gui-qt5)
set(AQ_INCLUDES ${AQBANKING_INCLUDE_DIRS} ${GWENHYWFAR_INCLUDE_DIRS})
include_directories(${AQ_INCLUDES})

add_definitions(${Qt${QT_VERSION_MAJOR}Core_DEFINITIONS})
add_definitions(${Qt${QT_VERSION_MAJOR}Concurrent_DEFINITIONS})
add_definitions(${Qt${QT_VERSION_MAJOR}Gui_DEFINITIONS})
add_definitions(${Qt${QT_VERSION_MAJOR}Sql_DEFINITIONS})
add_definitions(${Qt${QT_VERSION_MAJOR}Svg_DEFINITIONS})
add_definitions(${Qt${QT_VERSION_MAJOR}Xml_DEFINITIONS})
add_definitions(${Qt${QT_VERSION_MAJOR}Test_DEFINITIONS})

set(TEST_APP_CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../src)
file(
        GLOB_RECURSE APP_SRC_FILES
        ${TEST_APP_CORE_DIR}/core/*.cpp
        ${TEST_APP_CORE_DIR}/core/Banking/*.cpp
        ${TEST_APP_CORE_DIR}/core/Logger/*.cpp
        ${TEST_APP_CORE_DIR}/core/MaterialDesign/*.cpp
        ${TEST_APP_CORE_DIR}/core/SingleApplication/*.cpp
        ${TEST_APP_CORE_DIR}/core/Storage/*.cpp
        ${TEST_APP_CORE_DIR}/core/Storage/Account/*.cpp
        ${TEST_APP_CORE_DIR}/core/Storage/Connection/*.cpp
        ${TEST_APP_CORE_DIR}/core/Storage/Transaction/*.cpp
)
file(
        GLOB_RECURSE APP_HDR_FILES
        ${TEST_APP_CORE_DIR}/core/*.h
        ${TEST_APP_CORE_DIR}/core/Banking/*.h
        ${TEST_APP_CORE_DIR}/core/Logger/*.h
        ${TEST_APP_CORE_DIR}/core/MaterialDesign/*.h
        ${TEST_APP_CORE_DIR}/core/SingleApplication/*.h
        ${TEST_APP_CORE_DIR}/core/Storage/*.h
        ${TEST_APP_CORE_DIR}/core/Storage/Account/*.h
        ${TEST_APP_CORE_DIR}/core/Storage/Connection/*.h
        ${TEST_APP_CORE_DIR}/core/Storage/Transaction/*.h
)
set(APP_FILES ${APP_SRC_FILES} ${APP_HDR_FILES})

set(TEST_APP_RCS_FILE ${TEST_APP_CORE_DIR}/../res/olbaflinx.qrc)
qt_add_resources(TEST_APP_RCS_FILE ${TEST_APP_RCS_FILE})

set(
        TEST_INCLUDES
        ${TEST_APP_CORE_DIR}
        ${TEST_APP_CORE_DIR}/core
        ${TEST_APP_CORE_DIR}/core/Banking
        ${TEST_APP_CORE_DIR}/core/Logger
        ${TEST_APP_CORE_DIR}/core/MaterialDesign
        ${TEST_APP_CORE_DIR}/core/SingleApplication
        ${TEST_APP_CORE_DIR}/core/Storage
        ${TEST_APP_CORE_DIR}/core/Storage/Account
        ${TEST_APP_CORE_DIR}/core/Storage/Connection
        ${TEST_APP_CORE_DIR}/core/Storage/Transaction
)
include_directories(${TEST_INCLUDES})
