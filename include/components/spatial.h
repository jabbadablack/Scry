#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SCRY_CHUNK_SIZE 64.0f

typedef struct ScryChunkCoord {
    int32_t x;
    int32_t y;
} ScryChunkCoord;

typedef struct ScryChunkHash {
    uint64_t hash;
} ScryChunkHash;

/**
 * @brief Packs coordinates into a 64-bit hash.
 */
static inline uint64_t ScrySpatial_CalculateChunkHash(int32_t cx, int32_t cy) {
    return (uint64_t)((uint32_t)cx) << 32 | (uint64_t)((uint32_t)cy);
}

#ifdef __cplusplus
}
#endif
