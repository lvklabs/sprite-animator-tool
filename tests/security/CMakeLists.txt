# tests/security/CMakeLists.txt
#
# Agent 5: negative regression test for the postprocessing-script shell
# injection. Verifies that a payload containing shell metacharacters cannot
# spawn a shell or affect filesystem state outside what the validated
# subprocess explicitly does.

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Test)

# We need the production SpriteState (plus the small set of data classes
# it pulls in) compiled into the test binary. Keep this list minimal --
# don't drag in MainWindow / widgets just to test the export path.
set(SECTEST_SRCS
    "${CMAKE_SOURCE_DIR}/src/spritestate.cpp"
    "${CMAKE_SOURCE_DIR}/src/inputimage.cpp"
    "${CMAKE_SOURCE_DIR}/src/lvkframe.cpp"
    "${CMAKE_SOURCE_DIR}/src/lvkanimation.cpp"
    "${CMAKE_SOURCE_DIR}/src/lvkaframe.cpp"
)

add_executable(tst_postpscript_injection
    tst_postpscript_injection.cpp
    ${SECTEST_SRCS}
)

set_target_properties(tst_postpscript_injection PROPERTIES
    AUTOMOC ON
    AUTOUIC OFF
    AUTORCC OFF
)

target_include_directories(tst_postpscript_injection PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
)

target_link_libraries(tst_postpscript_injection PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
)

add_test(
    NAME security_postpscript_injection
    COMMAND tst_postpscript_injection
)
set_tests_properties(security_postpscript_injection PROPERTIES
    LABELS "security"
    TIMEOUT 60
)
