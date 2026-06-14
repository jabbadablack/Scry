# Scry Unit Tests CMake Configuration
file(GLOB_RECURSE TEST_SRC CONFIGURE_DEPENDS "*.cpp")

add_executable(scry_tests ${TEST_SRC})
target_compile_options(scry_tests PRIVATE ${SCRY_WARNING_FLAGS})

# flecs is PUBLIC in scry's link graph — headers propagate automatically.
target_link_libraries(scry_tests PRIVATE scry Catch2::Catch2WithMain)
target_include_directories(scry_tests PRIVATE 
    "${CMAKE_CURRENT_SOURCE_DIR}/../include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../external/eigen"
)

# test_main.cpp uses <Eigen/Dense> via scry_math.hpp; precompile it once.
target_precompile_headers(scry_tests PRIVATE <Eigen/Dense>)
