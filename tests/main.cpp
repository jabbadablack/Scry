#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <IO/stb_impl.inl>

#include <memory/chained_arena.hpp>
#include <OS/glfw/glfw_window.hpp>
#include <OS/glfw/glfw_impl.inl>
#include <GLFW/glfw3.h>
#if defined(_WIN32) || defined(_WIN64)
    #ifdef CreateWindow
        #undef CreateWindow
    #endif
#endif
#include <IO/threading/job_system.hpp>
#include <chrono>
#include <thread>
#include <iostream>

TEST_CASE("ChainedArena Core Functionality") {
    engine::ChainedArena arena(1024);

    SUBCASE("Successful allocations with proper alignment boundaries") {
        std::byte* ptr1 = arena.Allocate(1, 1);
        REQUIRE(ptr1 != nullptr);

        std::byte* ptr2 = arena.Allocate(4, 4);
        REQUIRE(ptr2 != nullptr);
        REQUIRE(reinterpret_cast<uintptr_t>(ptr2) % 4 == 0);

        std::byte* ptr3 = arena.Allocate(32, 16);
        REQUIRE(ptr3 != nullptr);
        REQUIRE(reinterpret_cast<uintptr_t>(ptr3) % 16 == 0);

        std::byte* ptr4 = arena.Allocate(64, 32);
        REQUIRE(ptr4 != nullptr);
        REQUIRE(reinterpret_cast<uintptr_t>(ptr4) % 32 == 0);

        std::byte* ptr5 = arena.Allocate(128, 64);
        REQUIRE(ptr5 != nullptr);
        REQUIRE(reinterpret_cast<uintptr_t>(ptr5) % 64 == 0);

        REQUIRE(ptr2 > ptr1);
        REQUIRE(ptr3 > ptr2);
        REQUIRE(ptr4 > ptr3);
        REQUIRE(ptr5 > ptr4);
    }

    SUBCASE("Arena chaining on overflow") {
        engine::ChainedArena small_arena(32);

        // First 20 bytes fit in the 32-byte block
        std::byte* ptr1 = small_arena.Allocate(20, 4);
        REQUIRE(ptr1 != nullptr);

        // Second 20 bytes overflow into a chained block
        std::byte* ptr2 = small_arena.Allocate(20, 4);
        REQUIRE(ptr2 != nullptr);

        // Pointers must be distinct and valid
        REQUIRE(ptr1 != ptr2);
    }

    SUBCASE("Clear resets offset so first allocation recycles the same address") {
        std::byte* ptr1 = arena.Allocate(100, 8);
        REQUIRE(ptr1 != nullptr);

        arena.Clear();

        std::byte* ptr2 = arena.Allocate(100, 8);
        REQUIRE(ptr2 == ptr1);
    }
}

TEST_CASE("GlfwWindow Integration") {
    engine::GlfwWindow context;

    REQUIRE(context.Initialize() == true);

    engine::io::JobSystem jobSystem;
    auto future = jobSystem.RunTask([]() {
        return 42;
    });

    int result = future.get();
    REQUIRE(result == 42);

    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    REQUIRE(context.CreateWindow(800, 600, "ENGINE Engine Unit Tests") == true);

    engine::NativeHandles handles = context.GetNativeHandles();
    REQUIRE(handles.window != nullptr);

    for (int i = 0; i < 5; ++i) {
        context.PollEvents();
        context.SwapBuffers();
    }

    context.Shutdown();
}
