# Umbrella entrypoint for the LvkSpriteEditor test suite.
#
# Authored by Agent 3 (Phase 1: Test Infrastructure). Sibling subdirectories
# `format/` (Agent 2) and `security/` (Agent 5) are picked up conditionally
# so each parallel agent's worktree contributes independently and the merge
# is purely additive.

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Test)

# lvk_add_test(name SOURCES srcs... LINKS libs...)
#
# Creates a Qt Test executable wired up with the meta-build conveniences and
# registers it with CTest. Each test runs with WORKING_DIRECTORY set to the
# source tree root so fixtures under examples/ are accessible via relative
# paths.
function(lvk_add_test name)
    cmake_parse_arguments(LVK_TEST "" "" "SOURCES;LINKS" ${ARGN})

    if(NOT LVK_TEST_SOURCES)
        message(FATAL_ERROR "lvk_add_test(${name}): no SOURCES provided")
    endif()

    add_executable(${name} ${LVK_TEST_SOURCES})
    set_target_properties(${name} PROPERTIES AUTOMOC ON)

    target_include_directories(${name} PRIVATE
        "${CMAKE_SOURCE_DIR}/src"
    )

    target_link_libraries(${name} PRIVATE
        Qt6::Test
        Qt6::Core
        Qt6::Gui
        ${LVK_TEST_LINKS}
    )

    add_test(
        NAME ${name}
        COMMAND ${name}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
endfunction()

add_subdirectory(unit)

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/format/CMakeLists.txt)
    add_subdirectory(format)
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/security/CMakeLists.txt)
    add_subdirectory(security)
endif()
