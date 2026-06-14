# Standalone example that proves Scry operates as a decoupled framework.
# The only explicit dependency is the scry shared library — no SDL3, Flecs,
# mimalloc, or Eigen headers are used directly by the application code.

add_executable(scry_sandbox main.cpp)
target_compile_options(scry_sandbox PRIVATE ${SCRY_WARNING_FLAGS})
target_link_libraries(scry_sandbox PRIVATE scry)

# scry_input_plugin must exist (declared in root project.json and built first)
add_dependencies(scry_sandbox scry_input_plugin)

# All executables land in the same bin/<config>/ directory, so scry.dll and
# flecs.dll are already there.  We only need to stage the per-sandbox config.
add_custom_command(TARGET scry_sandbox POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/project.json
        $<TARGET_FILE_DIR:scry_sandbox>/sandbox_project.json
    COMMENT "Staging sandbox runtime project config"
)
