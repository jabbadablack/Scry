add_executable(engine_test main.cpp test_array.cpp test_ecs.cpp test_resource.cpp test_engine.cpp test_allocators.cpp test_math.cpp)
target_link_libraries(engine_test PRIVATE ENGINE doctest glfw_module)
