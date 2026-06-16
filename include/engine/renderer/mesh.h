#pragma once
#include <engine/math.h>
#include <engine/engine.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ScryMeshLOD {
    uint32_t indexCount;
    uint32_t firstIndex;
    uint32_t baseVertex;
    float    threshold;   // switch to next LOD at this distance (metres)
} ScryMeshLOD;

typedef struct ScryLODGroup {
    ScryMeshLOD lods[3];     // lods[0]=near, lods[1]=mid, lods[2]=far
    uint32_t    group_id;    // index into g_LODGroups / g_LODGroupBuffer
} ScryLODGroup;

/**
 * @brief Uploads a mesh file into global megabuffers.
 */
ENGINE_API ScryLODGroup ScryGraphics_LoadMesh(const char* filepath);

/**
 * @brief Retrieves a LOD group by ID.
 */
ENGINE_API const ScryLODGroup* ScryGraphics_GetLODGroup(uint32_t id);

/**
 * @brief Gets the total number of loaded LOD groups.
 */
ENGINE_API uint32_t ScryGraphics_GetLODGroupCount(void);

typedef struct ScryMeshData {
    uint32_t lod_group_id;
} ScryMeshData;

typedef struct ScryAABB {
    ScryVec3 min;
    float    pad0;
    ScryVec3 max;
    float    pad1;
} ScryAABB;

typedef struct ScryRendererIntent {
    uint32_t mask;
} ScryRendererIntent;

typedef struct ScryMaterial {
    uint16_t program_handle;
    float    base_color[4];
} ScryMaterial;

typedef enum ScryEntityIntent {
    SCRY_INTENT_NONE      = 0,
    SCRY_INTENT_VISIBLE   = 1u << 0,
    SCRY_INTENT_DESTROYED = 1u << 1
} ScryEntityIntent;

#ifdef __cplusplus
}
#endif
