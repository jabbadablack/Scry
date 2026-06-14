cmake_minimum_required(VERSION 3.19)
project(Sandbox VERSION 1.0.0 LANGUAGES C CXX)

# 1. Read project.json to find required plugins
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/project.json" PROJECT_JSON_RAW)

# 2. Extract plugins array length
string(JSON PLUGIN_COUNT LENGTH "${PROJECT_JSON_RAW}" "plugins")
math(EXPR PLUGIN_MAX "${PLUGIN_COUNT} - 1")

set(REQUIRED_PLUGINS "")
if(PLUGIN_COUNT GREATER 0)
    foreach(IDX RANGE ${PLUGIN_MAX})
        string(JSON PLUGIN_NAME GET "${PROJECT_JSON_RAW}" "plugins" ${IDX})
        list(APPEND REQUIRED_PLUGINS ${PLUGIN_NAME})
    endforeach()
endif()

message(STATUS "[Sandbox] Required plugins from project.json: ${REQUIRED_PLUGINS}")

# 3. Add the project target first so we can add dependencies to it
engine_add_project(scry_sandbox)

# 4. Discover all available plugins and only add those required
file(GLOB PLUGIN_DIRS "../../plugins/*")

foreach(PDIR ${PLUGIN_DIRS})
    if(IS_DIRECTORY ${PDIR} AND EXISTS "${PDIR}/plugin.json")
        file(READ "${PDIR}/plugin.json" PJSON_RAW)
        string(JSON PNAME GET "${PJSON_RAW}" "name")
        
        list(FIND REQUIRED_PLUGINS "${PNAME}" FOUND_IDX)
        if(NOT FOUND_IDX EQUAL -1)
            message(STATUS "[Sandbox] Adding required plugin: ${PNAME} (${PDIR})")
            get_filename_component(PDIR_NAME ${PDIR} NAME)
            add_subdirectory(${PDIR} "${CMAKE_CURRENT_BINARY_DIR}/plugins/${PDIR_NAME}")
            
            # Ensure the plugin is built when the sandbox is built
            add_dependencies(scry_sandbox ${PNAME})
        endif()
    endif()
endforeach()
