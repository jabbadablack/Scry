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

    # Build directly into an isolated folder within the bin directory.
    # We set config-specific output directories to handle Ninja Multi-Config correctly.
    if(GENERATOR_IS_MULTI_CONFIG OR CMAKE_CONFIGURATION_TYPES)
        foreach(cfg ${CMAKE_CONFIGURATION_TYPES})
            string(TOUPPER ${cfg} cfg_upper)
            set_target_properties(${target_name} PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY_${cfg_upper} "${CMAKE_BINARY_DIR}/bin/${cfg}/plugins/${target_name}"
                LIBRARY_OUTPUT_DIRECTORY_${cfg_upper} "${CMAKE_BINARY_DIR}/bin/${cfg}/plugins/${target_name}"
                PDB_OUTPUT_DIRECTORY_${cfg_upper}     "${CMAKE_BINARY_DIR}/bin/${cfg}/plugins/${target_name}"
            )
        endforeach()
    else()
        # Single-config generators
        set(_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin/plugins/${target_name}")
        set_target_properties(${target_name} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${_OUTPUT_DIR}"
            LIBRARY_OUTPUT_DIRECTORY "${_OUTPUT_DIR}"
            PDB_OUTPUT_DIRECTORY     "${_OUTPUT_DIR}"
        )
    endif()

    # Stage plugin.json to the same isolated folder
    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json"
            "$<TARGET_FILE_DIR:${target_name}>/plugin.json"
        COMMENT "Staging plugin.json for ${target_name}"
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
