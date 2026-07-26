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

set(MD3_BUILD_GALLERY OFF CACHE BOOL "Skip Gallery" FORCE)
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../QML_MD3" "${CMAKE_CURRENT_BINARY_DIR}/md3")

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
    Md3
)
if (TARGET Md3plugin)
    target_link_libraries(md3_create PRIVATE Md3plugin)
endif()

if (WIN32 AND MSVC)
    target_link_options(md3_create PRIVATE "/MANIFEST:NO")
endif()
