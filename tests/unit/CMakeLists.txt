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

lvk_add_full_test(tst_mainwindow_construction)
lvk_add_full_test(tst_image_tab_controller)
lvk_add_full_test(tst_export_controller)
