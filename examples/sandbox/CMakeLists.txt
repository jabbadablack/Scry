cmake_minimum_required(VERSION 3.19)
project(ScrySandbox VERSION 1.0.0 LANGUAGES CXX)

# ─── Project type ─────────────────────────────────────────────────────────────
# SCRY_PROJECT_TYPE tells ScryHelpers.cmake whether we're building an executable
# game or a plugin library.  The sandbox is always a GAME.
set(SCRY_PROJECT_TYPE "GAME")

# ─── Engine discovery ─────────────────────────────────────────────────────────
# When built as add_subdirectory of the engine project the 'scry' target already
# exists.  When built standalone the user must point CMake at the engine build:
#   cmake -DScry_DIR=<engine_build_dir> -S examples/sandbox -B build_sandbox
if(NOT TARGET scry)
    find_package(Scry REQUIRED)
endif()

# ─── Plugin root ──────────────────────────────────────────────────────────────
# Default: sibling plugins/ directory relative to the engine source root.
# Override with -DSCRY_PLUGINS_DIR=<path> when building standalone.
if(NOT DEFINED SCRY_PLUGINS_DIR)
    set(SCRY_PLUGINS_DIR "${CMAKE_SOURCE_DIR}/plugins")
endif()

# ─── Plugin dependencies from project.json ────────────────────────────────────
# project.json (build-time) declares which plugins this game depends on by name.
# For each entry the sandbox:
#   1. Verifies the plugin directory and plugin.json exist.
#   2. Validates the name in plugin.json matches the declared name.
#   3. Sets SCRY_PROJECT_TYPE=PLUGIN and adds the plugin as a subdirectory so
#      it links against the SAME 'scry' target this sandbox is using.
#   4. Restores SCRY_PROJECT_TYPE=GAME for subsequent targets.
#
# This is the key architectural property: the plugin connects to the engine
# instance the GAME is using, not to the engine project directly.

set(_SANDBOX_PLUGIN_TARGETS "")

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/project.json")
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/project.json" _SANDBOX_JSON)
    string(JSON _PLUGIN_COUNT LENGTH "${_SANDBOX_JSON}" "plugins")
    math(EXPR _PLUGIN_LAST "${_PLUGIN_COUNT} - 1")

    if(_PLUGIN_LAST GREATER_EQUAL 0)
        foreach(idx RANGE ${_PLUGIN_LAST})
            string(JSON _PLUGIN_NAME GET "${_SANDBOX_JSON}" "plugins" ${idx})
            set(_PLUGIN_DIR "${SCRY_PLUGINS_DIR}/${_PLUGIN_NAME}")
            set(_PLUGIN_JSON_PATH "${_PLUGIN_DIR}/plugin.json")

            if(NOT EXISTS "${_PLUGIN_JSON_PATH}")
                message(WARNING "[ScrySandbox] plugin.json not found for '${_PLUGIN_NAME}' at ${_PLUGIN_JSON_PATH} — skipping")
                continue()
            endif()

            file(READ "${_PLUGIN_JSON_PATH}" _PLUGIN_META)
            string(JSON _PLUGIN_DECLARED_NAME GET "${_PLUGIN_META}" "name")

            if(NOT _PLUGIN_DECLARED_NAME STREQUAL _PLUGIN_NAME)
                message(WARNING
                    "[ScrySandbox] Name mismatch for plugin '${_PLUGIN_NAME}': "
                    "plugin.json declares '${_PLUGIN_DECLARED_NAME}' — skipping")
                continue()
            endif()

            # Temporarily flip project type so the plugin's CMakeLists sees PLUGIN
            set(SCRY_PROJECT_TYPE "PLUGIN")
            add_subdirectory(
                "${_PLUGIN_DIR}"
                "${CMAKE_CURRENT_BINARY_DIR}/plugins/${_PLUGIN_NAME}"
            )
            set(SCRY_PROJECT_TYPE "GAME")

            list(APPEND _SANDBOX_PLUGIN_TARGETS "${_PLUGIN_NAME}")
            message(STATUS "[ScrySandbox] Added plugin '${_PLUGIN_NAME}' from ${_PLUGIN_DIR}")
        endforeach()
    endif()
endif()

# ─── Game executable ──────────────────────────────────────────────────────────
add_executable(scry_sandbox main.cpp)
target_compile_options(scry_sandbox PRIVATE ${SCRY_WARNING_FLAGS})
target_precompile_headers(scry_sandbox PRIVATE <Eigen/Dense>)
target_link_libraries(scry_sandbox PRIVATE scry)

# Ensure every plugin this game declared is built before the executable is linked
foreach(_pt IN LISTS _SANDBOX_PLUGIN_TARGETS)
    if(TARGET ${_pt})
        add_dependencies(scry_sandbox ${_pt})
    endif()
endforeach()

# Stage the runtime config (plugin DLL paths, initial ECS state) next to the exe
add_custom_command(TARGET scry_sandbox POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/scry_project.json"
        "$<TARGET_FILE_DIR:scry_sandbox>/scry_project.json"
    COMMENT "Staging runtime scry_project.json for scry_sandbox"
)
