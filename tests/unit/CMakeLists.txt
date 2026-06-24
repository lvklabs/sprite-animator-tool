# Unit tests for the LvkSpriteEditor data classes.
#
# Each executable links directly against the small set of production .cpp
# files it exercises. Duplicate compilation across tests is intentional --
# it keeps the dependency graph trivial and isolates failures.

set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)

lvk_add_test(tst_inputimage
    SOURCES
        tst_inputimage.cpp
        ${SRC_DIR}/inputimage.cpp
)

lvk_add_test(tst_lvkframe
    SOURCES
        tst_lvkframe.cpp
        ${SRC_DIR}/lvkframe.cpp
)

# Team J1 (J1.2): regression test for LvkTableWidget::keyReleaseEvent
# null-deref when currentItem() is nullptr (Ctrl+Up/Down on an unset
# cell would segfault the GUI). Needs Qt6::Widgets for QTableWidget +
# QTest::keyClick, and runs under offscreen QPA since it instantiates
# a widget.
lvk_add_test(tst_lvk_table_widget
    SOURCES
        tst_lvk_table_widget.cpp
        ${SRC_DIR}/lvktablewidget.cpp
    LINKS
        Qt6::Widgets
)
set_tests_properties(tst_lvk_table_widget PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

lvk_add_test(tst_lvkaframe
    SOURCES
        tst_lvkaframe.cpp
        ${SRC_DIR}/lvkaframe.cpp
)

lvk_add_test(tst_lvkanimation
    SOURCES
        tst_lvkanimation.cpp
        ${SRC_DIR}/lvkanimation.cpp
        ${SRC_DIR}/lvkaframe.cpp
)

lvk_add_test(tst_statecircularbuffer
    SOURCES
        tst_statecircularbuffer.cpp
        ${SRC_DIR}/statecircularbuffer.cpp
        ${SRC_DIR}/inputimage.cpp
        ${SRC_DIR}/lvkframe.cpp
        ${SRC_DIR}/lvkanimation.cpp
        ${SRC_DIR}/lvkaframe.cpp
)

lvk_add_test(tst_spritestate_crud
    SOURCES
        tst_spritestate_crud.cpp
        ${SRC_DIR}/spritestate.cpp
        ${SRC_DIR}/inputimage.cpp
        ${SRC_DIR}/lvkframe.cpp
        ${SRC_DIR}/lvkanimation.cpp
        ${SRC_DIR}/lvkaframe.cpp
        # Team D2 (D2.1): SpriteState::load now calls lvk::validateImageFile.
        ${SRC_DIR}/image_validation.cpp
        # Agent 8: SpriteState::exportSprite now references the JSON
        # exporter; include it so the unit test still links.
        ${SRC_DIR}/exporters/JsonAtlasExporter.cpp
    LINKS
        Qt6::Widgets
)
# SpriteState carries QPixmap members and so needs a QGuiApplication; run
# under the offscreen QPA so the test passes on headless CI runners.
set_tests_properties(tst_spritestate_crud PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# Team H1.2: SpriteState2 transaction depth-counting + undo/redo
# termination guards. Exercises the re-entrant transaction marker
# collapse and the iteration cap that prevents infinite loops when a
# transaction marker has been evicted from the ring buffer.
lvk_add_test(tst_spritestate2_transactions
    SOURCES
        tst_spritestate2_transactions.cpp
        ${SRC_DIR}/spritestate.cpp
        ${SRC_DIR}/spritestate2.cpp
        ${SRC_DIR}/statecircularbuffer.cpp
        ${SRC_DIR}/inputimage.cpp
        ${SRC_DIR}/lvkframe.cpp
        ${SRC_DIR}/lvkanimation.cpp
        ${SRC_DIR}/lvkaframe.cpp
        ${SRC_DIR}/image_validation.cpp
        ${SRC_DIR}/exporters/JsonAtlasExporter.cpp
    LINKS
        Qt6::Widgets
)
# Build the transaction tests with DEBUG_UNDO so BUFF_SIZE collapses to 5
# and we can construct an exactly-full transaction (start + 3 ops + end =
# 5 entries). The full-buffer test then exercises the off-by-one iteration
# cap fix without having to spin up a 200-entry transaction.
target_compile_definitions(tst_spritestate2_transactions PRIVATE DEBUG_UNDO)
set_tests_properties(tst_spritestate2_transactions PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    # Defense in depth: if a future regression brings back the infinite
    # loop, fail fast rather than block CI for the default test timeout.
    TIMEOUT 30
)

# Phase 5b: controller + MainWindow construction tests. ------------------
#
# These tests instantiate the full MainWindow (and through it the five tab
# controllers) under the offscreen QPA. Unlike the data-class tests above
# they need AUTOUIC (for mainwindow.ui), AUTORCC (for the .qrc icon
# resource referenced by the About dialog code path), Qt6::Widgets, and
# the complete production source list.
#
# We declare them with add_executable directly (rather than lvk_add_test)
# because lvk_add_test doesn't enable AUTOUIC / AUTORCC and doesn't accept
# .ui / .qrc inputs.
set(LVK_FULL_SOURCES
    ${SRC_DIR}/mainwindow.cpp
    ${SRC_DIR}/spritestate.cpp
    ${SRC_DIR}/spritestate2.cpp
    ${SRC_DIR}/inputimage.cpp
    ${SRC_DIR}/lvkframe.cpp
    ${SRC_DIR}/lvkanimation.cpp
    ${SRC_DIR}/lvkaframe.cpp
    ${SRC_DIR}/lvkanimationwidget.cpp
    ${SRC_DIR}/lvkaction.cpp
    ${SRC_DIR}/lvkinputimagewidget.cpp
    ${SRC_DIR}/lvktablewidget.cpp
    ${SRC_DIR}/dialogs.cpp
    ${SRC_DIR}/statecircularbuffer.cpp
    ${SRC_DIR}/lvkframedefwidget.cpp
    ${SRC_DIR}/theme.cpp
    # Team D2 (D2.1): shared image-file validator.
    ${SRC_DIR}/image_validation.cpp
    ${SRC_DIR}/exporters/JsonAtlasExporter.cpp
    ${SRC_DIR}/controllers/ImageTabController.cpp
    ${SRC_DIR}/controllers/FrameTabController.cpp
    ${SRC_DIR}/controllers/AnimationTabController.cpp
    ${SRC_DIR}/controllers/TransitionTabController.cpp
    ${SRC_DIR}/controllers/ExportController.cpp
)

function(lvk_add_full_test name)
    add_executable(${name}
        ${name}.cpp
        ${LVK_FULL_SOURCES}
        ${SRC_DIR}/mainwindow.ui
        ${SRC_DIR}/res.qrc
    )
    set_target_properties(${name} PROPERTIES
        AUTOMOC ON
        AUTOUIC ON
        AUTORCC ON
        AUTOUIC_SEARCH_PATHS "${SRC_DIR}"
    )
    target_include_directories(${name} PRIVATE "${SRC_DIR}")
    # Match the main target's warning flags (Team D5 / Team F3.1).
    lvk_set_warning_flags(${name})
    target_link_libraries(${name} PRIVATE
        Qt6::Test
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
    )
    add_test(
        NAME ${name}
        COMMAND ${name}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(${name} PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )
endfunction()

# Team J3.4: LvkTableWidget no longer redundantly emits cellChanged after
# setText (the model emits it once already). The test drives Ctrl+Up /
# Ctrl+Down and asserts the connected slot fires exactly once -- pre-J3
# it fired twice and produced spurious undo entries.
lvk_add_test(tst_lvktablewidget
    SOURCES
        tst_lvktablewidget.cpp
        ${SRC_DIR}/lvktablewidget.cpp
    LINKS
        Qt6::Widgets
)
set_tests_properties(tst_lvktablewidget PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

lvk_add_full_test(tst_mainwindow_construction)
lvk_add_full_test(tst_image_tab_controller)
lvk_add_full_test(tst_export_controller)
lvk_add_full_test(tst_transition_tab_controller)
# Team H5.4 / H5.5: MRU recent-files and geometry persistence tests both
# instantiate MainWindow under offscreen QPA, so they ride the same
# lvk_add_full_test helper.
lvk_add_full_test(tst_recent_files)
lvk_add_full_test(tst_geometry_persistence)

# Team H5.7: undo + save integration test. Uses SpriteState2 + the
# examples/mario.lvks fixture; doesn't need the MainWindow sources.
lvk_add_test(tst_undo_save_integration
    SOURCES
        tst_undo_save_integration.cpp
        ${SRC_DIR}/spritestate.cpp
        ${SRC_DIR}/spritestate2.cpp
        ${SRC_DIR}/inputimage.cpp
        ${SRC_DIR}/lvkframe.cpp
        ${SRC_DIR}/lvkanimation.cpp
        ${SRC_DIR}/lvkaframe.cpp
        ${SRC_DIR}/statecircularbuffer.cpp
        ${SRC_DIR}/image_validation.cpp
        ${SRC_DIR}/exporters/JsonAtlasExporter.cpp
    LINKS
        Qt6::Widgets
)
target_compile_definitions(tst_undo_save_integration PRIVATE
    LVK_EXAMPLES_DIR="${CMAKE_SOURCE_DIR}/examples"
)
# SpriteState2 carries QPixmap members like SpriteState -- run under the
# offscreen QPA so it passes on headless CI runners.
set_tests_properties(tst_undo_save_integration PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# Team H5.3: spawns the built editor via QProcess to assert documented
# --export exit codes (0 / -1 / 2). No production sources are linked --
# the test is a thin Qt Test harness around QProcess.
lvk_add_test(tst_cli_exit_codes
    SOURCES
        tst_cli_exit_codes.cpp
)
target_compile_definitions(tst_cli_exit_codes PRIVATE
    LVK_BIN_PATH="$<TARGET_FILE:LvkSpriteEditor>"
    LVK_EXAMPLES_DIR="${CMAKE_SOURCE_DIR}/examples"
)
# Make sure the editor binary exists before this test runs.
add_dependencies(tst_cli_exit_codes LvkSpriteEditor)
set_tests_properties(tst_cli_exit_codes PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
