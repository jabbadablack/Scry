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

    if(DEFINED SCRY_C_FLAGS AND DEFINED SCRY_CXX_FLAGS)
        target_compile_options(${target_name} PRIVATE 
            $<$<COMPILE_LANGUAGE:C>:${SCRY_C_FLAGS}>
            $<$<COMPILE_LANGUAGE:CXX>:${SCRY_CXX_FLAGS}>
        )
    endif()

    # Isolated output path: bin/<config>/plugins/<name>
    set(_PLUGIN_DIR "$<TARGET_FILE_DIR:scry>/plugins/${target_name}")

    set_target_properties(${target_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${_PLUGIN_DIR}"
        LIBRARY_OUTPUT_DIRECTORY "${_PLUGIN_DIR}"
        PDB_OUTPUT_DIRECTORY     "${_PLUGIN_DIR}"
    )

    # Config-specific overrides for Ninja Multi-Config
    foreach(cfg ${CMAKE_CONFIGURATION_TYPES})
        string(TOUPPER ${cfg} cfg_upper)
        set_target_properties(${target_name} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY_${cfg_upper} "${CMAKE_BINARY_DIR}/bin/${cfg}/plugins/${target_name}"
            LIBRARY_OUTPUT_DIRECTORY_${cfg_upper} "${CMAKE_BINARY_DIR}/bin/${cfg}/plugins/${target_name}"
            PDB_OUTPUT_DIRECTORY_${cfg_upper}     "${CMAKE_BINARY_DIR}/bin/${cfg}/plugins/${target_name}"
        )
    endforeach()

    # Stage plugin.json to the isolated directory
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

    if(DEFINED SCRY_C_FLAGS AND DEFINED SCRY_CXX_FLAGS)
        target_compile_options(${target_name} PRIVATE 
            $<$<COMPILE_LANGUAGE:C>:${SCRY_C_FLAGS}>
            $<$<COMPILE_LANGUAGE:CXX>:${SCRY_CXX_FLAGS}>
        )
    endif()
endmacro()
