cmake_minimum_required(VERSION 3.21)

project(LvkSpriteEditor
    VERSION 2.0.0
    DESCRIPTION "Lavanda Ink Sprite Editor (Qt6 modernization)"
    LANGUAGES CXX
)

# C++ language settings ---------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Qt meta-build conveniences ---------------------------------------------
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Reasonable defaults
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Find Qt6 ----------------------------------------------------------------
# The plan targeted 6.8 LTS but the dev environment ships 6.4.2; require
# 6.4 minimum so this builds on the available system Qt.
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets)

# qt_standard_project_setup() exists from Qt 6.3+
if(COMMAND qt_standard_project_setup)
    qt_standard_project_setup()
endif()

# Source / header lists --------------------------------------------------
set(LVK_SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/spritestate.cpp
    src/lvkframe.cpp
    src/inputimage.cpp
    src/lvkanimation.cpp
    src/lvkaframe.cpp
    src/lvkanimationwidget.cpp
    src/lvkaction.cpp
    src/lvkinputimagewidget.cpp
    src/lvktablewidget.cpp
    src/spritestate2.cpp
    src/dialogs.cpp
    src/statecircularbuffer.cpp
    src/lvkframedefwidget.cpp
)

set(LVK_HEADERS
    src/mainwindow.h
    src/spritestate.h
    src/lvkframe.h
    src/inputimage.h
    src/types.h
    src/lvkanimation.h
    src/lvkaframe.h
    src/lvkanimationwidget.h
    src/lvkaction.h
    src/settings.h
    src/lvkinputimagewidget.h
    src/lvktablewidget.h
    src/spritestate2.h
    src/dialogs.h
    src/statecircularbuffer.h
    src/lvkframedefwidget.h
)

set(LVK_FORMS
    src/mainwindow.ui
)

set(LVK_RESOURCES
    src/res.qrc
)

# Executable target ------------------------------------------------------
add_executable(LvkSpriteEditor
    ${LVK_SOURCES}
    ${LVK_HEADERS}
    ${LVK_FORMS}
    ${LVK_RESOURCES}
)

target_include_directories(LvkSpriteEditor PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(LvkSpriteEditor PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

# Per-platform compile definitions (preserved from the legacy .pro file)
if(APPLE)
    target_compile_definitions(LvkSpriteEditor PRIVATE MAC_OS_X)
    set_target_properties(LvkSpriteEditor PROPERTIES MACOSX_BUNDLE TRUE)
endif()

if(WIN32)
    target_compile_definitions(LvkSpriteEditor PRIVATE WIN32)
    set_target_properties(LvkSpriteEditor PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

# Tests (wired by Agent 3) -----------------------------------------------
enable_testing()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
    add_subdirectory(tests)
endif()
