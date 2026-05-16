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
    # Agent 8: JSON atlas exporter + 5 MainWindow tab controllers split
    # out of the 2,511-line god class.
    src/exporters/JsonAtlasExporter.cpp
    src/controllers/ImageTabController.cpp
    src/controllers/FrameTabController.cpp
    src/controllers/AnimationTabController.cpp
    src/controllers/TransitionTabController.cpp
    src/controllers/ExportController.cpp
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
    src/exporters/JsonAtlasExporter.h
    src/controllers/ImageTabController.h
    src/controllers/FrameTabController.h
    src/controllers/AnimationTabController.h
    src/controllers/TransitionTabController.h
    src/controllers/ExportController.h
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

# AUTOUIC needs to find mainwindow.ui from the controllers/ + exporters/
# subdirectories that include "ui_mainwindow.h". Adding the .ui dir to
# AUTOUIC_SEARCH_PATHS is the canonical Qt6 way.
set_target_properties(LvkSpriteEditor PROPERTIES
    AUTOUIC_SEARCH_PATHS "${CMAKE_CURRENT_SOURCE_DIR}/src"
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

# === CPack (Agent 4) ====================================================
# Install rules + CPack configuration for desktop packaging. Append-only;
# nothing above this marker is modified by Agent 4.
install(TARGETS LvkSpriteEditor
    RUNTIME DESTINATION bin
    BUNDLE DESTINATION .
)

set(CPACK_PACKAGE_NAME "LvkSpriteEditor")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${PROJECT_VERSION_PATCH}")
set(CPACK_PACKAGE_VENDOR "LVK Labs")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "WYSIWYG 2D sprite animation tool")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "${CPACK_PACKAGE_NAME}")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/COPYING")

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(CPACK_GENERATOR "DEB;TGZ")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "LVK Labs <noreply@anthropic.com>")
    set(CPACK_DEBIAN_PACKAGE_SECTION "graphics")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_DMG_VOLUME_NAME "${CPACK_PACKAGE_NAME}")
elseif(WIN32)
    set(CPACK_GENERATOR "NSIS;ZIP")
    set(CPACK_NSIS_DISPLAY_NAME "${CPACK_PACKAGE_NAME}")
    set(CPACK_NSIS_PACKAGE_NAME "${CPACK_PACKAGE_NAME}")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
endif()

include(CPack)
