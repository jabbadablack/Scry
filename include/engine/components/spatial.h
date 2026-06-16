#pragma once
#include <cstdint>
#include <cmath>

namespace Engine {
namespace Spatial {

constexpr float CHUNK_SIZE = 64.0f;

struct ChunkCoord { int32_t x; int32_t y; };
struct ChunkHash  { uint64_t hash; };

// Packs chunk coords into a sortable 64-bit key (sorted primarily by X, then Y).
inline uint64_t CalculateChunkHash(int32_t cx, int32_t cy) {
    return (static_cast<uint64_t>(static_cast<uint32_t>(cx)) << 32) |
            static_cast<uint64_t>(static_cast<uint32_t>(cy));
}

} // namespace Spatial
} // namespace Engine
