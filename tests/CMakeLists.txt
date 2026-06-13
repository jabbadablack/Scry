# Scry Unit Tests CMake Configuration
file(GLOB_RECURSE TEST_SRC CONFIGURE_DEPENDS "*.cpp")

add_executable(scry_tests ${TEST_SRC})

target_link_libraries(scry_tests PRIVATE scry Catch2::Catch2WithMain)
target_include_directories(scry_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../include")
