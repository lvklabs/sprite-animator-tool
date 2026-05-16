# tests/security/CMakeLists.txt
#
# Agent 5: negative regression test for the postprocessing-script shell
# injection. Verifies that a payload containing shell metacharacters cannot
# spawn a shell or affect filesystem state outside what the validated
# subprocess explicitly does.
#
# Phase 4 (Agent 10b) extends this with four hardening tests:
#   tst_image_path_injection   -- InputImage::fromString path rejection
#   tst_atlas_size_bomb        -- LvkFrame + JsonAtlasExporter size caps
#   tst_path_traversal         -- expanded full-filename validation cases
#   tst_script_toctou          -- postprocessing-script symlink TOCTOU

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
    # Agent 8: SpriteState::exportSprite() pulled in JsonAtlasExporter.
    "${CMAKE_SOURCE_DIR}/src/exporters/JsonAtlasExporter.cpp"
)

function(lvk_add_security_test name)
    add_executable(${name}
        ${name}.cpp
        ${SECTEST_SRCS}
    )
    set_target_properties(${name} PROPERTIES
        AUTOMOC ON
        AUTOUIC OFF
        AUTORCC OFF
    )
    target_include_directories(${name} PRIVATE
        "${CMAKE_SOURCE_DIR}/src"
    )
    target_link_libraries(${name} PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Test
    )
    add_test(
        NAME security_${name}
        COMMAND ${name}
    )
    set_tests_properties(security_${name} PROPERTIES
        LABELS "security"
        TIMEOUT 120
    )
endfunction()

lvk_add_security_test(tst_postpscript_injection)
# Phase 4 (Item 15): image-path injection on .lvks load.
lvk_add_security_test(tst_image_path_injection)
# Phase 4 (Item 16): frame-dimension bounds + atlas overflow.
lvk_add_security_test(tst_atlas_size_bomb)
# Phase 4 (Item 17): strengthened path-traversal cases.
# Named "_hard" to avoid a target-name collision with tests/format/tst_path_traversal.
lvk_add_security_test(tst_path_traversal_hard)
# Phase 4 (Item 18): symlink-TOCTOU on the postprocessing script.
lvk_add_security_test(tst_script_toctou)
