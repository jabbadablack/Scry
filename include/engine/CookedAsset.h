#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Binary mesh format ──────────────────────────────────────────────────────
 * A .scrymesh file (version 3) layout:
 *   [ScryMeshHeader]
 *   [ScryVertex × lod0_vertex_count]   ← LOD0 vertex buffer (full mesh)
 *   [uint32_t   × lod0_index_count]    ← LOD0 index buffer
 *   [ScryVertex × lod1_vertex_count]   ← LOD1 vertex buffer (50% — compact)
 *   [uint32_t   × lod1_index_count]    ← LOD1 index buffer
 *   [ScryVertex × lod2_vertex_count]   ← LOD2 vertex buffer (10% — compact)
 *   [uint32_t   × lod2_index_count]    ← LOD2 index buffer
 */

#define SCRY_MESH_MAGIC   0x59524353u  /* little-endian 'SCRY' */
#define SCRY_MESH_VERSION 3u

#pragma pack(push, 1)

typedef struct ScryVertex {
    float px, py, pz;   /* position */
    float nx, ny, nz;   /* normal   */
    float u,  v;        /* texcoord */
} ScryVertex;

typedef struct ScryMeshHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t lod0_vertex_count;
    uint32_t lod0_index_count;
    uint32_t lod1_vertex_count;
    uint32_t lod1_index_count;
    uint32_t lod2_vertex_count;
    uint32_t lod2_index_count;
} ScryMeshHeader;

/* ── Binary texture format ───────────────────────────────────────────────────
 * A .scrytex file:
 *   [ScryTexHeader][uint8_t RGBA × width × height × 4]
 */

#define SCRY_TEX_MAGIC   0x58455453u  /* little-endian 'STEX' */
#define SCRY_TEX_VERSION 1u

typedef struct ScryTexHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t width;
    uint32_t height;
} ScryTexHeader;

#pragma pack(pop)

#ifdef __cplusplus
}
#endif
