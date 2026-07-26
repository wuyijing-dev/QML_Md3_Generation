cmake_minimum_required(VERSION 3.16)
project(Md3Create VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS
    Quick
    QuickControls2
    QuickEffects
    QuickShapesPrivate
    Widgets
)
qt_standard_project_setup(REQUIRES 6.8)

# ---------------------------------------------------------------------------
# Md3: prefer packaged library in the SAME directory as this CMakeLists.txt
#
#   QML_Md3_Generation/
#     CMakeLists.txt
#     Main.qml
#     Md3/                 ← fixed name (from package-linux.sh / package-windows.ps1)
#       lib/
#       include/
#       lib/cmake/Md3/
#
# Fallback: -DMD3_ROOT=/path/to/QML_MD3 sources (add_subdirectory).
# ---------------------------------------------------------------------------
set(MD3_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Md3" CACHE PATH
    "Packaged Md3 next to this CMakeLists.txt")
set(_md3_from_package FALSE)

if (EXISTS "${MD3_DIR}/lib/cmake/Md3/Md3Config.cmake")
    list(APPEND CMAKE_PREFIX_PATH "${MD3_DIR}")
    find_package(Md3 REQUIRED CONFIG)
    set(_md3_from_package TRUE)
    message(STATUS "Md3Create: using same-dir package MD3_DIR=${MD3_DIR}")
elseif (EXISTS "${MD3_DIR}/Md3Prebuilt.cmake")
    include("${MD3_DIR}/Md3Prebuilt.cmake")
    set(_md3_from_package TRUE)
    message(STATUS "Md3Create: using same-dir Md3Prebuilt.cmake at ${MD3_DIR}")
else()
    set(MD3_ROOT "" CACHE PATH "Path to QML_MD3 repository (contains src/Md3)")
    if (NOT MD3_ROOT)
        foreach (_cand IN ITEMS
                "${CMAKE_CURRENT_SOURCE_DIR}/../QML_MD3"
                "${CMAKE_CURRENT_SOURCE_DIR}/../QML_Md3"
                "$ENV{MD3_ROOT}"
                "$ENV{HOME}/QML_MD3"
                "$ENV{HOME}/md3_generation/QML_MD3")
            if (_cand AND EXISTS "${_cand}/CMakeLists.txt" AND EXISTS "${_cand}/src/Md3")
                set(MD3_ROOT "${_cand}" CACHE PATH "Path to QML_MD3 repository (contains src/Md3)" FORCE)
                break()
            endif()
        endforeach()
    endif()
    if (NOT MD3_ROOT OR NOT EXISTS "${MD3_ROOT}/CMakeLists.txt")
        message(FATAL_ERROR
            "Md3 not found.\n"
            "  Preferred: put packaged library at\n"
            "    ${CMAKE_CURRENT_SOURCE_DIR}/Md3\n"
            "    (run QML_MD3/scripts/package-linux.sh and copy dist/Md3 here)\n"
            "  Or set -DMD3_ROOT=/path/to/QML_MD3 sources.")
    endif()
    message(STATUS "Md3Create: MD3_ROOT=${MD3_ROOT} (sources via add_subdirectory)")
    set(MD3_BUILD_GALLERY OFF CACHE BOOL "Skip Gallery" FORCE)
    add_subdirectory("${MD3_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/md3")
endif()

qt_add_executable(md3_create main.cpp)

qt_add_qml_module(md3_create
    URI Md3Create
    VERSION 1.0
    SOURCES
        projectgenerator.h
        projectgenerator.cpp
    QML_FILES
        Main.qml
)

# Project templates embedded as resources
qt_add_resources(md3_create "md3_create_templates"
    PREFIX "/md3-create"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}"
    FILES
        templates/_common/CMakeLists.txt.in
        templates/_common/main.cpp.in
        templates/_common/README.md.in
        templates/_common/.gitignore
        templates/empty/Main.qml.in
        templates/basic/Main.qml.in
        templates/rail/Main.qml.in
        templates/rail/pages/HomePage.qml.in
        templates/rail/pages/WidgetsPage.qml.in
        templates/rail/pages/SettingsPage.qml.in
)

set_target_properties(md3_create PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
    OUTPUT_NAME "Md3Create"
)

target_link_libraries(md3_create PRIVATE
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Widgets
)
if (TARGET Md3)
    target_link_libraries(md3_create PRIVATE Md3)
endif()
if (TARGET Md3::Md3)
    target_link_libraries(md3_create PRIVATE Md3::Md3)
endif()
if (TARGET Md3plugin)
    target_link_libraries(md3_create PRIVATE Md3plugin)
endif()
if (TARGET Md3plugin_init)
    target_link_libraries(md3_create PRIVATE Md3plugin_init)
endif()

if (WIN32 AND MSVC)
    target_link_options(md3_create PRIVATE "/MANIFEST:NO")
endif()
