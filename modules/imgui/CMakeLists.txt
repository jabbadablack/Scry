cmake_minimum_required(VERSION 3.19)
project(ImGuiModule CXX)

include(FetchContent)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG docking
)

FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
endif()

file(WRITE ${imgui_SOURCE_DIR}/CMakeLists.txt "cmake_minimum_required(VERSION 3.19)
project(imgui CXX)
add_library(imgui STATIC
    imgui.cpp
    imgui_demo.cpp
    imgui_draw.cpp
    imgui_tables.cpp
    imgui_widgets.cpp
)
target_include_directories(imgui PUBLIC \${CMAKE_CURRENT_SOURCE_DIR})
add_library(imgui::imgui ALIAS imgui)
")

if(NOT TARGET imgui)
    add_subdirectory(${imgui_SOURCE_DIR} ${imgui_BINARY_DIR})
endif()

add_library(imgui_mod STATIC src/imgui_module.cpp)

target_include_directories(imgui_mod PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${imgui_SOURCE_DIR}>
)

target_link_libraries(imgui_mod PUBLIC
    SCRY
    imgui::imgui
)
