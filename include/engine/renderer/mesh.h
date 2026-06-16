#pragma once
#include <engine/math.h>
#include <engine/engine.h>
#include <cstdint>

namespace Engine {

namespace Graphics {

struct MeshLOD {
    uint32_t indexCount;
    uint32_t firstIndex;
    uint32_t baseVertex;
    float    threshold;   // switch to next LOD at this distance (metres)
};

struct LODGroup {
    MeshLOD  lods[3];     // lods[0]=near, lods[1]=mid, lods[2]=far
    uint32_t group_id;    // index into g_LODGroups / g_LODGroupBuffer
};

/**
 * @brief Uploads a mesh file into global megabuffers.
 */
ENGINE_API LODGroup  LoadMesh(const char* filepath);

/**
 * @brief Retrieves a LOD group by ID.
 */
ENGINE_API const LODGroup* GetLODGroup(uint32_t id);

/**
 * @brief Gets the total number of loaded LOD groups.
 */
ENGINE_API uint32_t  GetLODGroupCount();

} // namespace Graphics

namespace Renderer {

struct MeshData { uint32_t lod_group_id; };

struct AABB {
    Engine::Math::ScryVec3 min;
    Engine::Math::ScryVec3 max;
};

struct Intent { uint32_t mask; };

struct Material {
    uint16_t program_handle;
    float    base_color[4];
};

enum EntityIntent : uint32_t {
    INTENT_NONE      = 0,
    INTENT_VISIBLE   = 1u << 0,
    INTENT_DESTROYED = 1u << 1
};

} // namespace Renderer
} // namespace Engine
