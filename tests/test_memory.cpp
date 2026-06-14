#include <catch2/catch_test_macros.hpp>
#include <engine/memory.hpp>

TEST_CASE("Arena Allocator basic allocation", "[memory]") {
    const size_t arena_size = 1024;
    void* backing = ::operator new(arena_size);
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
