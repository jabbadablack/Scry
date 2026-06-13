# Scry Examples CMake Configuration

add_executable(sandbox main.cpp)

target_link_libraries(sandbox PRIVATE scry flecs mimalloc-static)
target_include_directories(sandbox PRIVATE "${mimalloc_SOURCE_DIR}/include")

# Copy flecs DLL next to the executable
add_custom_command(TARGET sandbox POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:flecs>
    $<TARGET_FILE_DIR:sandbox>
)

# Copy scry_project.json next to the executable
add_custom_command(TARGET sandbox POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_CURRENT_SOURCE_DIR}/scry_project.json
    $<TARGET_FILE_DIR:sandbox>/scry_project.json
)
