#pragma once
#include <engine/math.h>
#include <cstdint>

namespace Engine {
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
