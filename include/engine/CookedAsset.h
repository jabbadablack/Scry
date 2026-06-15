#pragma once
#include <stdint.h>

/* ── Binary mesh format ──────────────────────────────────────────────────────
 * A .scrymesh file is a single contiguous block:
 *   [ScryMeshHeader][ScryVertex × vertex_count][uint32_t × index_count]
 * Load with a single fread into an arena; no pointer fixup needed.
 */

#define SCRY_MESH_MAGIC   0x59524353u  /* little-endian 'SCRY' */
#define SCRY_MESH_VERSION 1u

#pragma pack(push, 1)

typedef struct {
    float px, py, pz;   /* position */
    float nx, ny, nz;   /* normal   */
    float u,  v;        /* texcoord */
} ScryVertex;

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t vertex_count;
    uint32_t index_count;
} ScryMeshHeader;

/* ── Binary texture format ───────────────────────────────────────────────────
 * A .scrytex file:
 *   [ScryTexHeader][uint8_t RGBA × width × height × 4]
 */

#define SCRY_TEX_MAGIC   0x58455453u  /* little-endian 'STEX' */
#define SCRY_TEX_VERSION 1u

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t width;
    uint32_t height;
    /* Followed by: uint8_t pixels[width * height * 4] (RGBA) */
} ScryTexHeader;

#pragma pack(pop)

#ifdef __cplusplus
static_assert(sizeof(ScryVertex)     == 32u, "ScryVertex layout changed");
static_assert(sizeof(ScryMeshHeader) == 16u, "ScryMeshHeader layout changed");
static_assert(sizeof(ScryTexHeader)  == 16u, "ScryTexHeader layout changed");
#endif
