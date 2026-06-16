#pragma once
#include <cstdint>

namespace Engine {
namespace Renderer {

struct MeshInstance { uint32_t mesh_id; };

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
