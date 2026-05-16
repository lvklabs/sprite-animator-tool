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
    LINKS
        Qt6::Widgets
)
# SpriteState carries QPixmap members and so needs a QGuiApplication; run
# under the offscreen QPA so the test passes on headless CI runners.
set_tests_properties(tst_spritestate_crud PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
