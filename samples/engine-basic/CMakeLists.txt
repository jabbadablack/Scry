cmake_minimum_required(VERSION 3.19)
project(EngineBasic CXX)

add_executable(engine-basic main.cpp)

target_link_libraries(engine-basic PRIVATE
    SCRY
    renderer
    glfw_module
)
