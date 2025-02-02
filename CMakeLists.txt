cmake_minimum_required(VERSION 3.14)

project(HotWatchClient LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 COMPONENTS Core Quick Network Gui WebSockets REQUIRED)

if(NOT DEFINED QML_DIR)
    set(QML_DIR "")
endif()
set(QMLDIR ${CMAKE_SOURCE_DIR}${QML_DIR})

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/config.h.in ${CMAKE_CURRENT_SOURCE_DIR}/src/config.h @ONLY)

qt6_add_library(HotWatchClient STATIC)
qt6_add_qml_module(HotWatchClient
    URI "HotWatch"
    VERSION 1.0
    QML_FILES
        HotWatch.qml
    SOURCES
        src/HotWatchClient.hpp
        src/HotWatchClient.cpp
        src/config.h.in
        src/config.h
    OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/HotWatch
)

target_link_libraries(HotWatchClient
    PRIVATE 
    Qt::Core 
    Qt::Quick 
    Qt::Network
    Qt::Gui
    Qt::WebSockets
)


set_target_properties(HotWatchClient PROPERTIES
    PUBLIC_HEADER "src/HotWatchClient.hpp"
)

target_include_directories(HotWatchClient PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/)

function(deleteinplace IN_FILE pattern)
  file (STRINGS ${IN_FILE} LINES)
  file(WRITE ${IN_FILE} "")
  foreach(LINE IN LISTS LINES)
    string(FIND "${LINE}" "${pattern}" POS)
    if (POS GREATER -1)
    else()
      set(STRIPPED "${LINE}")
      string(FIND "${LINE}" "linktarget" POS)
      if (POS GREATER -1)
        set(STRIPPED "")
      endif()
      string(FIND "${LINE}" "typeinfo" POS)
      if (POS GREATER -1)
        set(STRIPPED "")
      endif()
      string(FIND "${LINE}" "plugin" POS)
      if (POS GREATER -1)
        set(STRIPPED "")
      endif()
      string(FIND "${LINE}" "optional" POS)
      if (POS GREATER -1)
        set(STRIPPED "")
      endif()
      string(FIND "${LINE}" "classname" POS)
      if (POS GREATER -1)
        set(STRIPPED "")
      endif()
      string(SUBSTRING "${STRIPPED}" 0 1 FIRSTCHAR)
      if ("${FIRSTCHAR}" STREQUAL "")
      else()
        file(APPEND ${IN_FILE} "${STRIPPED}\n")
    endif()
    endif()
  endforeach()
endfunction()
function(remove_qmldirs REPERTOIRE)
    file(GLOB_RECURSE QMLDIR_FILES "${REPERTOIRE}/qmldir")

    foreach(QMLDIR_FILE ${QMLDIR_FILES})
        file(REMOVE ${QMLDIR_FILE})
    endforeach()
    message(STATUS "[+] qmldir removed")
endfunction()

function(copy_qmldirs PATH_1 CHEMIN_DESTINATION)
    message(STATUS "[+] copying qmldir from build to source folder")
    file(GLOB_RECURSE QMLDIR_FILES "${PATH_1}/qmldir")

    foreach(QMLDIR_FILE ${QMLDIR_FILES})
        file(RELATIVE_PATH REL_PATH ${PATH_1} ${QMLDIR_FILE})
        string(REGEX REPLACE "qmldir" "" REL_PATH ${REL_PATH})
        # message(STATUS "Copying ${QMLDIR_FILE} to ${CHEMIN_DESTINATION}/${REL_PATH}")
        file(COPY ${QMLDIR_FILE} DESTINATION "${CHEMIN_DESTINATION}/${REL_PATH}")
    endforeach()
endfunction()

function(hotwatch_process REPERTOIRE)
    # remove_qmldirs(${CMAKE_SOURCE_DIR}/qml/)
    file(GLOB_RECURSE QMLDIR_FILES "${REPERTOIRE}/qmldir")
    foreach(QMLDIR_FILE ${QMLDIR_FILES})
        deleteinplace(${QMLDIR_FILE} "prefer :")
    endforeach()
    message(STATUS "[+] qmldir on build updated")
    copy_qmldirs(${REPERTOIRE} ${CMAKE_SOURCE_DIR}/qml/BuMa)
endfunction()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
