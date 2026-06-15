#pragma once
#include <engine/engine.h>
#include <engine/math.hpp>
#include <flecs.h>

namespace Engine {
namespace Camera {

struct Camera {
    Math::ScryVec3 position;
    float          pitch;
    float          yaw;
    Math::ScryMat4 view_proj;
};

ENGINE_API extern ecs_entity_t id_Camera;

ENGINE_API void Init(ecs_world_t* world);

} // namespace Camera
} // namespace Engine
