#include <catch2/catch_test_macros.hpp>
#include <engine/memory.hpp>
#include <cassert>
#include <cstdio>

/**
 * @brief Let's play in the sand! This test checks if our Arena Allocator is behaving.
 * 
 * We're looking to make sure it can hand out memory chunks and keep track of how much it has left.
 * 
 * @example
 * // This is a test case, so it's run by the Catch2 test runner.
 */
TEST_CASE("Arena Allocator basic allocation", "[memory]") {
    const size_t arena_size = 1024;
    void* backing = ::operator new(arena_size);
    assert(arena_size > 0 && "We need some space for the arena!");
    assert(backing != nullptr && "Operator new should have given us some memory.");
    std::printf("[test] Starting Arena Allocator basic allocation test...\n");
    std::printf("[test] Initializing arena with %zu bytes...\n", arena_size);
    REQUIRE(backing != nullptr);

    Engine::Memory::Arena arena;
    Engine::Memory::ArenaInit(&arena, backing, arena_size);
    REQUIRE(Engine::Memory::ArenaGetTotalSize(&arena) == arena_size);
    REQUIRE(Engine::Memory::ArenaGetUsedMemory(&arena) == 0);

    void* p1 = Engine::Memory::ArenaAllocate(&arena, 128, 8);
    REQUIRE(p1 != nullptr);
    REQUIRE(Engine::Memory::ArenaGetUsedMemory(&arena) == 128);

    ::operator delete(backing);
}
