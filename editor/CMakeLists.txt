cmake_minimum_required(VERSION 3.21)
project(ScryEditor CXX)

# ---------------------------------------------------------------------------
# Slint UI Framework (GPLv3)
# Locate the installed pre-built Slint package.  The official installer drops
# files under "C:/Program Files/Slint-cpp <version>/".  Glob for any installed
# version so the CMakeLists does not need to be updated when Slint is upgraded.
# Alternatively, pass -DCMAKE_PREFIX_PATH="<slint-root>" on the cmake command
# line to override.
# ---------------------------------------------------------------------------
file(GLOB _slint_candidates "C:/Program Files/Slint-cpp*")
if(_slint_candidates)
    list(SORT _slint_candidates ORDER DESCENDING)
    list(GET _slint_candidates 0 _slint_prefix)
    list(APPEND CMAKE_PREFIX_PATH "${_slint_prefix}")
    message(STATUS "[SCRY_Editor] Found Slint install prefix: ${_slint_prefix}")
endif()

find_package(Slint REQUIRED)

# ---------------------------------------------------------------------------
# SCRY_Editor executable
# ---------------------------------------------------------------------------
add_executable(SCRY_Editor
    src/main.cpp
    src/glfw_impl.cpp
    src/editor_renderer_bridge.cpp
)

# Compile editor_window.slint -> editor_window.h in the build tree.
# slint_target_sources also appends the generated include path to SCRY_Editor.
slint_target_sources(SCRY_Editor ui/editor_window.slint)

target_include_directories(SCRY_Editor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# CRITICAL isolation: Slint::Slint (GPLv3) is linked ONLY to SCRY_Editor.
# Neither SCRY nor renderer list Slint as a dependency, so there is zero risk
# of GPLv3 contamination to those targets.
target_link_libraries(SCRY_Editor PRIVATE
    SCRY
    renderer
    Slint::Slint
)

# Copy Slint runtime DLLs next to the executable on Windows.
if(WIN32)
    add_custom_command(TARGET SCRY_Editor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:SCRY_Editor>
            $<TARGET_FILE_DIR:SCRY_Editor>
        COMMAND_EXPAND_LISTS
    )
endif()
