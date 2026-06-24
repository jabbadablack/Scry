cmake_minimum_required(VERSION 3.19)
project(Profiler CXX)

include(FetchContent)

FetchContent_Declare(
    tracy
    GIT_REPOSITORY https://github.com/wolfpld/tracy.git
    GIT_TAG master
)

FetchContent_MakeAvailable(tracy)

add_library(profiler STATIC src/tracy_module.cpp)

target_include_directories(profiler PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(profiler PUBLIC
    ENGINE
    TracyClient
)
