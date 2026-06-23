# Umbrella entrypoint for the LvkSpriteEditor test suite.
#
# Authored by Agent 3 (Phase 1: Test Infrastructure). Sibling subdirectories
# `format/` (Agent 2) and `security/` (Agent 5) are picked up conditionally
# so each parallel agent's worktree contributes independently and the merge
# is purely additive.

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Test)

# lvk_set_warning_flags(target)
#
# Apply the same -Wall -Wextra -Wpedantic (or MSVC /W4) flags the main
# target uses (Team D5) to a test target. Centralised here so that each
# of the four lvk_add_*_test helpers in this directory tree -- the
# canonical lvk_add_test below, tests/format/CMakeLists.txt's
# lvk_add_format_test, tests/security/CMakeLists.txt's
# lvk_add_security_test, and tests/unit/CMakeLists.txt's
# lvk_add_full_test -- can call into the SAME warning-flag policy
# without each re-declaring it (or, as before, silently dropping it for
# 12 of 18 test executables).
function(lvk_set_warning_flags target_name)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
    elseif(MSVC)
        target_compile_options(${target_name} PRIVATE /W4)
    endif()
endfunction()

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

    lvk_set_warning_flags(${name})

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
