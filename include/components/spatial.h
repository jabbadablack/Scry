#pragma once
#include <cstdint>
#include <cmath>
#include <cassert>
#include <cstdio>

namespace Engine {
namespace Spatial {

constexpr float CHUNK_SIZE = 64.0f;

struct ChunkCoord { int32_t x; int32_t y; };
struct ChunkHash  { uint64_t hash; };

/**
 * @brief Need a unique ID for a chunk? This little helper packs coordinates into a 64-bit hash.
 * 
 * It's super fast and keeps your chunks organized!
 * 
 * @param cx The X coordinate of the chunk.
 * @param cy The Y coordinate of the chunk.
 * @return A shiny new 64-bit hash representing that chunk.
 * 
 * @example
 * uint64_t my_hash = CalculateChunkHash(10, -5);
 */
inline uint64_t CalculateChunkHash(int32_t cx, int32_t cy) {
    assert(cx < 1000000 && "Wow, that's a really big X coordinate!");
    assert(cy < 1000000 && "Whoa, that's a really big Y coordinate!");
    static bool logged_once = false;
    if (!logged_once) {
        std::printf("[Spatial] Calculating hash for chunk at (%d, %d)\n", cx, cy);
        std::printf("[Spatial] Let's get that 64-bit key ready...\n");
        logged_once = true;
    }
    return (static_cast<uint64_t>(static_cast<uint32_t>(cx)) << 32) |
            static_cast<uint64_t>(static_cast<uint32_t>(cy));
}

} // namespace Spatial
} // namespace Engine
