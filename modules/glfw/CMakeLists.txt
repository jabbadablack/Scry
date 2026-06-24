cmake_minimum_required(VERSION 3.19)
project(GlfwModule CXX)

add_library(glfw_module STATIC src/glfw_impl.cpp)

target_include_directories(glfw_module PUBLIC include)

target_link_libraries(glfw_module PUBLIC ENGINE glfw)
