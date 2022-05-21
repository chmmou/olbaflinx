set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_CXX_FLAGS "-std=c++17 ${CMAKE_CXX_FLAGS} ${Qt${QT_VERSION_MAJOR}Widgets_EXECUTABLE_COMPILE_FLAGS}")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(
        QT_LIBS
        ${Qt${QT_VERSION_MAJOR}Core_LIBRARIES}
        ${Qt${QT_VERSION_MAJOR}Gui_LIBRARIES}
        ${Qt${QT_VERSION_MAJOR}Widgets_LIBRARIES}
        ${Qt${QT_VERSION_MAJOR}Concurrent_LIBRARIES}
        ${Qt${QT_VERSION_MAJOR}Network_LIBRARIES}
        ${Qt${QT_VERSION_MAJOR}Sql_LIBRARIES}
        ${Qt${QT_VERSION_MAJOR}Svg_LIBRARIES}
        ${Qt${QT_VERSION_MAJOR}Xml_LIBRARIES}
        SingleApplication::SingleApplication
)

set(
        QT_INCLUDES
        ${Qt${QT_VERSION_MAJOR}Core_INCLUDE_DIRS}
        ${Qt${QT_VERSION_MAJOR}Gui_INCLUDE_DIRS}
        ${Qt${QT_VERSION_MAJOR}Widgets_INCLUDE_DIRS}
        ${Qt${QT_VERSION_MAJOR}Concurrent_INCLUDE_DIRS}
        ${Qt${QT_VERSION_MAJOR}Network_INCLUDE_DIRS}
        ${Qt${QT_VERSION_MAJOR}Sql_INCLUDE_DIRS}
        ${Qt${QT_VERSION_MAJOR}Svg_INCLUDE_DIRS}
        ${Qt${QT_VERSION_MAJOR}Xml_INCLUDE_DIRS}
)

set(
        AQ_LIBS
        ${KtoBlzCheck_LIBRARIES}
        aqbanking::aqbanking
        gwenhywfar::core
        gwenhywfar::gui-cpp
        gwenhywfar::gui-qt5
        PkgConfig::LibChipCardc
        PkgConfig::LibChipCards
)

set(
        AQ_INCLUDES
        ${AQBANKING_INCLUDE_DIRS}
        ${GWENHYWFAR_INCLUDE_DIRS}
        ${LibChipCards_INCLUDE_DIRS}
)

include_directories(${AQ_INCLUDES})

add_definitions(
        ${Qt${QT_VERSION_MAJOR}Concurrent_DEFINITIONS}
        ${Qt${QT_VERSION_MAJOR}Widgets_DEFINITIONS}
        ${Qt${QT_VERSION_MAJOR}Gui_DEFINITIONS}
        ${Qt${QT_VERSION_MAJOR}Network_DEFINITIONS}
        ${Qt${QT_VERSION_MAJOR}Sql_DEFINITIONS}
        ${Qt${QT_VERSION_MAJOR}Svg_DEFINITIONS}
        ${Qt${QT_VERSION_MAJOR}Xml_DEFINITIONS}
)

include_directories(${QT_INCLUDES})

set(
        APP_TS_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/../res/${CMAKE_PROJECT_NAME}_de_DE.ts
        ${CMAKE_CURRENT_SOURCE_DIR}/../res/${CMAKE_PROJECT_NAME}_en_US.ts
)
set_source_files_properties(${APP_TS_FILES} PROPERTIES OUTPUT_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/../res)

set(APP_DIR ${CMAKE_CURRENT_SOURCE_DIR})
file(
        GLOB_RECURSE
        APP_SRC_FILES
        ${APP_DIR}/core/*.cpp
        ${APP_DIR}/core/BankData/*.cpp
        ${APP_DIR}/core/Banking/*.cpp
        ${APP_DIR}/core/Logger/*.cpp
        ${APP_DIR}/core/MaterialDesign/*.cpp
        ${APP_DIR}/core/Storage/*.cpp
        ${APP_DIR}/core/Storage/Account/*.cpp
        ${APP_DIR}/core/Storage/Connection/*.cpp
        ${APP_DIR}/core/Storage/Private/*.cpp
        ${APP_DIR}/core/Storage/Transaction/*.cpp

        ${APP_DIR}/app/*.cpp
        ${APP_DIR}/app/Components/*.cpp
        ${APP_DIR}/app/Banking/*.cpp
        ${APP_DIR}/app/Assistant/*.cpp
        ${APP_DIR}/app/Assistant/Page/*.cpp
        ${APP_DIR}/app/DataVault/*.cpp
        ${APP_DIR}/app/Pages/*.cpp
        ${APP_DIR}/app/Pages/Tabs*.cpp
)
file(
        GLOB_RECURSE
        APP_HDR_FILES
        ${APP_DIR}/core/*.h
        ${APP_DIR}/core/BankData/*.h
        ${APP_DIR}/core/Banking/*.h
        ${APP_DIR}/core/Logger/*.h
        ${APP_DIR}/core/MaterialDesign/*.h
        ${APP_DIR}/core/Storage/*.h
        ${APP_DIR}/core/Storage/Account/*.h
        ${APP_DIR}/core/Storage/Connection/*.h
        ${APP_DIR}/core/Storage/Private/*.h
        ${APP_DIR}/core/Storage/Transaction/*.h

        ${APP_DIR}/app/*.h
        ${APP_DIR}/app/Components/*.h
        ${APP_DIR}/app/Banking/*.h
        ${APP_DIR}/app/Assistant/*.h
        ${APP_DIR}/app/Assistant/Page/*.h
        ${APP_DIR}/app/DataVault/*.h
        ${APP_DIR}/app/Pages/*.h
        ${APP_DIR}/app/Pages/Tabs*.h
)

# Include directories for include UIs
include_directories(${APP_DIR})
include_directories(${APP_DIR}/core)
include_directories(${APP_DIR}/app)
include_directories(${APP_DIR}/app/Components)
include_directories(${APP_DIR}/app/Banking)
include_directories(${APP_DIR}/app/Assistant)
include_directories(${APP_DIR}/app/Assistant/Page)
include_directories(${APP_DIR}/app/DataVault)
include_directories(${APP_DIR}/app/Pages)
include_directories(${APP_DIR}/app/Pages/Tabs)

set(APP_UI_DIR ${APP_DIR}/uis)
file(GLOB APP_UI_FILES ${APP_UI_DIR}/*.ui)
include_directories(${APP_UI_DIR})

if (${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_wrap_ui(APP_UI_HDRS ${APP_UI_FILES})
else ()
    qt5_wrap_ui(APP_UI_HDRS ${APP_UI_FILES})
endif ()

set(
        APP_RCS_FILE
        ${CMAKE_CURRENT_SOURCE_DIR}/../res/${CMAKE_PROJECT_NAME}.qrc
        ${CMAKE_CURRENT_SOURCE_DIR}/../res/${CMAKE_PROJECT_NAME}-translations.qrc
)

if (${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_resources(APP_RCS_FILE ${APP_RCS_FILE})
else ()
    qt5_add_resources(APP_RCS_FILE ${APP_RCS_FILE})
endif ()

set(EXEC_PROPERTY "")
set(RC_FILES "")
if (WIN32)
    set(EXEC_PROPERTY "WIN32")
    enable_language(RC)
    set(
            RC_FILES
            ${CMAKE_CURRENT_SOURCE_DIR}/../res/${CMAKE_PROJECT_NAME}.rc
            ${CMAKE_CURRENT_SOURCE_DIR}/../res/${CMAKE_PROJECT_NAME}.manifest
    )
endif ()

set(
        APP_FILES
        ${APP_SRC_FILES}
        ${APP_HDR_FILES}
        ${APP_UI_HDRS}
        ${APP_TS_FILES}
        ${RC_FILES}
)

set(QAPPLICATION_CLASS QApplication)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/core/SingleApplication)

source_group("App Files" FILES ${APP_SRC_FILES} ${APP_HDR_FILES})
source_group("App Ui" FILES ${APP_UI_FILES})
source_group("App Translations" FILES ${APP_TS_FILES})
source_group("App Reource" FILES ${APP_RCS_FILE} ${RC_FILES})

add_compile_definitions(QT_DEPRECATED_WARNINGS)
add_compile_definitions(QT_DISABLE_DEPRECATED_BEFORE=0x050E20)

if (${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_create_translation(APP_TR_FILES ${APP_FILES} ${APP_TS_FILES})
    qt_add_executable(
            ${CMAKE_PROJECT_NAME} ${EXEC_PROPERTY}
            ${APP_FILES}
            ${APP_RCS_FILE}
            ${APP_TR_FILES}
    )
else ()
    qt5_create_translation(APP_TR_FILES ${APP_FILES} ${APP_TS_FILES})
    add_executable(
            ${CMAKE_PROJECT_NAME} ${EXEC_PROPERTY}
            ${APP_FILES}
            ${APP_RCS_FILE}
            ${APP_TR_FILES}
    )
endif ()

target_compile_definitions(
        ${CMAKE_PROJECT_NAME} PRIVATE
        QT_DEPRECATED_WARNINGS
        QT_DISABLE_DEPRECATED_BEFORE=0x050E20
        USE_LIBCHIPCARD
)

target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>")
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ${QT_LIBS} ${AQ_LIBS})

if (APPLE)
    set_target_properties(
            ${CMAKE_PROJECT_NAME} PROPERTIES
            MACOSX_BUNDLE_GUI_IDENTIFIER de.chm-projects.${CMAKE_PROJECT_NAME}
            MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
            MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    )
endif ()

if (NOT APPLE)
    install(TARGETS ${CMAKE_PROJECT_NAME} DESTINATION bin)
endif ()

if (QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(${CMAKE_PROJECT_NAME})
endif ()
