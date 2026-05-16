# tests/format/CMakeLists.txt
#
# Golden-file round-trip and version-header parse tests for the .lvks
# format. Authored by Agent 2/10 as part of the upgrade plan.
#
# Designed to be added via `add_subdirectory(format)` from
# `tests/CMakeLists.txt` (Agent 3's territory), but also self-sufficient
# enough to be added directly from the root `CMakeLists.txt` for local
# verification — find_package() is idempotent.

cmake_minimum_required(VERSION 3.21)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Test)

# Path to the project's `src/` tree. Resolved relative to this file so the
# tests work whether they're added from `tests/CMakeLists.txt` or directly
# from the root CMakeLists.txt.
set(_LVK_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../../src")

# Minimum set of project sources needed to instantiate and exercise
# SpriteState (load/save). We deliberately avoid pulling in mainwindow.*
# and the widget classes — they bring in Qt::Widgets and a heavier
# Q_OBJECT dependency graph that is irrelevant for parser tests.
set(_LVK_FORMAT_SOURCES
    "${_LVK_SRC}/spritestate.cpp"
    "${_LVK_SRC}/lvkframe.cpp"
    "${_LVK_SRC}/inputimage.cpp"
    "${_LVK_SRC}/lvkanimation.cpp"
    "${_LVK_SRC}/lvkaframe.cpp"
    # Agent 8: SpriteState::exportSprite() now dispatches to the JSON
    # exporter when format=Json|All. The format tests need it linked in.
    "${_LVK_SRC}/exporters/JsonAtlasExporter.cpp"
)

set(_LVK_FORMAT_HEADERS
    "${_LVK_SRC}/spritestate.h"
    "${_LVK_SRC}/lvkframe.h"
    "${_LVK_SRC}/inputimage.h"
    "${_LVK_SRC}/lvkanimation.h"
    "${_LVK_SRC}/lvkaframe.h"
    "${_LVK_SRC}/types.h"
    "${_LVK_SRC}/exporters/JsonAtlasExporter.h"
)

# Absolute path to the `examples/` dir, propagated to tests via
# LVK_EXAMPLES_DIR so they can find mario.lvks / ryu.lvks without depending
# on the CWD at run time.
set(_LVK_EXAMPLES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../examples")

# Helper: declare one Qt Test executable.
function(lvk_add_format_test test_name)
    add_executable(${test_name}
        ${test_name}.cpp
        ${_LVK_FORMAT_SOURCES}
        ${_LVK_FORMAT_HEADERS}
    )
    target_include_directories(${test_name} PRIVATE "${_LVK_SRC}")
    target_link_libraries(${test_name} PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Test
    )
    target_compile_definitions(${test_name} PRIVATE
        LVK_EXAMPLES_DIR="${_LVK_EXAMPLES_DIR}"
    )
    # The ctest name is prefixed "format_" so `ctest -R format` (the
    # filter used by the agent verification step and the upgrade plan
    # DoD) matches every test in this directory.
    add_test(NAME format_${test_name} COMMAND ${test_name})
    set_tests_properties(format_${test_name} PROPERTIES
        WORKING_DIRECTORY "${_LVK_EXAMPLES_DIR}"
        LABELS "format"
        # QGuiApplication (which Qt Test instantiates because we link
        # Qt6::Gui) would otherwise try to open the host display. The
        # offscreen platform plugin is the canonical way to run Qt tests
        # headless in CI and on remote dev boxes.
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )
endfunction()

lvk_add_format_test(tst_lvks_roundtrip)
lvk_add_format_test(tst_lvks_versions)
# Agent 8 (Phase 3): JSON atlas + path-traversal hardening tests.
lvk_add_format_test(tst_json_export)
lvk_add_format_test(tst_path_traversal)
# Phase 2: version-preservation regression test (golden v0.1 round-trip
# and data-driven auto-bump policy).
lvk_add_format_test(tst_version_preservation)
