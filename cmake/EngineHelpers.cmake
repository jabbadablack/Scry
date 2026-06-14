# EngineHelpers.cmake

# ─── engine_add_plugin(target_name) ──────────────────────────────────────────
# Builds a shared-library plugin and stages it with its descriptor.
macro(engine_add_plugin target_name)
    file(GLOB_RECURSE _SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
    )

    add_library(${target_name} SHARED ${_SOURCES})
    target_link_libraries(${target_name} PRIVATE scry flecs)

    if(DEFINED SCRY_WARNING_FLAGS)
        target_compile_options(${target_name} PRIVATE ${SCRY_WARNING_FLAGS})
    endif()

    # Create plugin folder in bin directory
    set(_PLUGIN_DIR "$<TARGET_FILE_DIR:scry>/plugins/${target_name}")

    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${_PLUGIN_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:${target_name}>"
            "${_PLUGIN_DIR}/$<TARGET_FILE_NAME:${target_name}>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json"
            "${_PLUGIN_DIR}/plugin.json"
        COMMENT "Staging plugin ${target_name} to bin/plugins/${target_name}"
    )
endmacro()

# ─── engine_add_project(target_name) ─────────────────────────────────────────
# Builds an executable project using the engine.
macro(engine_add_project target_name)
    file(GLOB_RECURSE _SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
    )

    add_executable(${target_name} ${_SOURCES})
    target_link_libraries(${target_name} PRIVATE scry)

    if(DEFINED SCRY_WARNING_FLAGS)
        target_compile_options(${target_name} PRIVATE ${SCRY_WARNING_FLAGS})
    endif()
endmacro()
