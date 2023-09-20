cmake_minimum_required(VERSION 3.14)

project(HotWatch LANGUAGES CXX)

find_package(Qt6 COMPONENTS Sql REQUIRED)

if(NOT DEFINED QML_DIR)
    set(QML_DIR "./")
endif()
set(QMLDIR ${CMAKE_SOURCE_DIR}/${QML_DIR})

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/config.h.in ${CMAKE_CURRENT_SOURCE_DIR}/src/config.h @ONLY)

qt6_add_library(HotWatch STATIC)
qt6_add_qml_module(HotWatch
    URI HotWatch
    VERSION 1.0
    QML_FILES
        qml/HotWatch.qml
    SOURCES
        src/HotWatch.h
        src/HotWatch.cpp
        src/config.h.in
        src/config.h
)

target_link_libraries(HotWatch
    PRIVATE Qt::Core)

set_target_properties(HotWatch PROPERTIES
    PUBLIC_HEADER "src/HotWatch.h"
)
target_include_directories(HotWatch PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
