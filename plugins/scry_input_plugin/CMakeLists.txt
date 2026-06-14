cmake_minimum_required(VERSION 3.19)

# Support standalone builds: when this file is the top-level project, configure
# it as a proper CMake project and locate the Scry engine via find_package.
# When added via add_subdirectory from a game project, CMAKE_SOURCE_DIR is the
# game's root, so the guard is false and we use the parent's 'scry' target.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(ScryInputPlugin VERSION 1.0.0 LANGUAGES C CXX)
    if(NOT TARGET scry)
        find_package(Scry REQUIRED)
    endif()
    if(NOT DEFINED SCRY_PROJECT_TYPE)
        set(SCRY_PROJECT_TYPE "PLUGIN")
    endif()
endif()

scry_add_plugin(scry_input_plugin)
