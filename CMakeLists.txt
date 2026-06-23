cmake_minimum_required(VERSION 3.21)

project(LvkSpriteEditor
    VERSION 2.0.1
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
#
# Phase 6b (Item 27): LinguistTools is required for the lrelease integration
# below (compiles .ts -> .qm into the binary's :/i18n Qt resource).  We mark
# it OPTIONAL_COMPONENTS so a stripped-down system Qt without Linguist still
# builds the editor (without translations); the i18n wiring is gated on
# Qt6LinguistTools_FOUND below.
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets)
find_package(Qt6 6.4 QUIET OPTIONAL_COMPONENTS LinguistTools)

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
    src/theme.cpp
    # Team D2 (D2.1): centralised image-file validator shared by the GUI
    # dialog path (ImageTabController) and the load path (SpriteState).
    src/image_validation.cpp
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
    src/theme.h
    src/image_validation.h
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

# Compile warnings (Team D5) ---------------------------------------------
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(LvkSpriteEditor PRIVATE -Wall -Wextra -Wpedantic)
elseif(MSVC)
    target_compile_options(LvkSpriteEditor PRIVATE /W4)
endif()

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

# === i18n (Phase 6b, Item 27) ============================================
# Compile each translations/lvkspriteeditor_<lang>.ts into a .qm file and
# embed all the .qm files into the executable under the :/i18n Qt resource
# prefix.  main.cpp loads from :/i18n at startup via QTranslator::load().
#
# We gate on Qt6LinguistTools so a developer build on a system without
# Linguist still produces a runnable editor (just without translations --
# QTranslator::load() returns false at startup and tr() returns the source
# string verbatim, which is the same behavior as today).
if(Qt6LinguistTools_FOUND)
    set(LVK_TRANSLATIONS
        translations/lvkspriteeditor_en.ts
        translations/lvkspriteeditor_es.ts
        translations/lvkspriteeditor_fr.ts
    )

    # qt_add_lrelease is the Qt6 way to drive `lrelease` from CMake.  It
    # writes the .qm files under ${CMAKE_CURRENT_BINARY_DIR} and exposes
    # their absolute paths via the named output variable.
    qt_add_lrelease(LvkSpriteEditor
        TS_FILES ${LVK_TRANSLATIONS}
        QM_FILES_OUTPUT_VARIABLE LVK_QM_FILES
    )

    # qt_add_resources embeds the compiled .qm files under :/i18n so
    # QTranslator::load(..., ":/i18n") finds them at runtime.  BASE points
    # at the binary dir so the resource alias is just the filename
    # (lvkspriteeditor_<lang>.qm), not a full absolute path.
    qt_add_resources(LvkSpriteEditor "i18n"
        PREFIX "/i18n"
        BASE "${CMAKE_CURRENT_BINARY_DIR}"
        FILES ${LVK_QM_FILES}
    )
endif()

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

# Build the Qt version into the package name so users can match it to
# their system Qt (Linux DEB binds to specific libqt6*.so symver, so a
# package built against 6.4.2 won't satisfy a system on 6.2.x). Without
# this the default file name is just "LvkSpriteEditor-2.0.1-Linux.deb"
# and there's no way to tell what Qt it expects.
set(_LVK_QT_VER "${Qt6_VERSION}")
set(CPACK_PACKAGE_FILE_NAME
    "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CMAKE_SYSTEM_NAME}-qt${_LVK_QT_VER}")

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(CPACK_GENERATOR "DEB;TGZ")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Pablo Perez De Angelis <pda@gezie.io>")
    set(CPACK_DEBIAN_PACKAGE_SECTION "graphics")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    # Fallback explicit Depends so `dpkg -i` resolves even when shlibdeps
    # under-reports (e.g. when Qt was installed via aqt rather than apt).
    # Version floors mirror find_package(Qt6 6.4 REQUIRED ...) above: the
    # binary calls qt_standard_project_setup, sets AUTOUIC_SEARCH_PATHS,
    # and uses QImageReader::setAllocationLimit, all Qt 6.4+. Without the
    # floor, `apt install` succeeds on Ubuntu 22.04 (Qt 6.2.x) and the
    # editor crashes at runtime on missing symbols. With the floor, the
    # install fails up front with a clear "unmet dependencies" message.
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libqt6widgets6 (>= 6.4), libqt6gui6 (>= 6.4), libqt6core6 (>= 6.4)")
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
