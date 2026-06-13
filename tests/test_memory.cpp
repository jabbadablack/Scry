#include <catch2/catch_test_macros.hpp>
#include <scry/scry_memory.hpp>

TEST_CASE("Arena Allocator basic allocation", "[memory]") {
    const size_t arena_size = 1024;
    void* backing = ::operator new(arena_size);
    REQUIRE(backing != nullptr);

    Scry::Memory::Arena arena;
    Scry::Memory::ScryArenaInit(&arena, backing, arena_size);
    REQUIRE(Scry::Memory::ScryArenaGetTotalSize(&arena) == arena_size);
    REQUIRE(Scry::Memory::ScryArenaGetUsedMemory(&arena) == 0);

    void* p1 = Scry::Memory::ScryArenaAllocate(&arena, 128, 8);
    REQUIRE(p1 != nullptr);
    REQUIRE(Scry::Memory::ScryArenaGetUsedMemory(&arena) == 128);

    ::operator delete(backing);
}
