# ScryHelpers.cmake
# Plugin and game helper macros for the Scry Framework.
# Included by the engine build and exported alongside the package config
# so both integrated (add_subdirectory) and standalone (find_package) builds
# share the same interface.
#
# SCRY_PROJECT_TYPE must be set to "PLUGIN" or "GAME" in the calling scope
# before invoking these macros.  The sandbox CMakeLists sets this automatically
# when it adds plugin subdirectories.

# ─── scry_add_plugin(target_name) ──────────────────────────────────────────
# Builds a shared-library plugin that slots into the Scry runtime.
# Requires:
#   - SCRY_PROJECT_TYPE == "PLUGIN"   (enforced below)
#   - A visible 'scry' CMake target    (engine lib or Scry:: imported target)
macro(scry_add_plugin target_name)
    if(NOT SCRY_PROJECT_TYPE STREQUAL "PLUGIN")
        message(FATAL_ERROR
            "scry_add_plugin(${target_name}): SCRY_PROJECT_TYPE must be 'PLUGIN' "
            "(currently '${SCRY_PROJECT_TYPE}'). The parent CMakeLists must set "
            "SCRY_PROJECT_TYPE=\"PLUGIN\" before calling add_subdirectory on a plugin.")
    endif()

    if(NOT TARGET scry)
        message(FATAL_ERROR
            "scry_add_plugin(${target_name}): the 'scry' target is not visible. "
            "Either build this plugin as a subdirectory of the engine/game project, "
            "or call find_package(Scry REQUIRED) and set Scry_DIR first.")
    endif()

    file(GLOB_RECURSE _SCRY_PLUGIN_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
    )

    add_library(${target_name} SHARED ${_SCRY_PLUGIN_SOURCES})

    if(DEFINED SCRY_WARNING_FLAGS)
        target_compile_options(${target_name} PRIVATE ${SCRY_WARNING_FLAGS})
    endif()

    # Link against the engine that the parent game project is already using.
    # Whether 'scry' is a real target (integrated build) or an IMPORTED target
    # (standalone find_package build) the semantics are identical.
    target_link_libraries(${target_name} PRIVATE scry flecs)

    # Stage the plugin DLL into <engine-bin>/plugins/ so every executable in
    # the same output directory can locate it at runtime via SDL_GetBasePath().
    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:scry>/plugins"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:${target_name}>"
            "$<TARGET_FILE_DIR:scry>/plugins/$<TARGET_FILE_NAME:${target_name}>"
        COMMENT "Staging ${target_name} → engine plugins directory"
    )
endmacro()
