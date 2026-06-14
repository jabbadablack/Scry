cmake_minimum_required(VERSION 3.19)
project(Sandbox VERSION 1.0.0 LANGUAGES CXX)

# Simply add plugins and then the project
set(SCRY_PROJECT_TYPE "PLUGIN")
add_subdirectory(../../plugins/scry_input_plugin plugins/scry_input_plugin)
add_subdirectory(../../plugins/scry_physics_plugin plugins/scry_physics_plugin)

set(SCRY_PROJECT_TYPE "GAME")
engine_add_project(scry_sandbox)
