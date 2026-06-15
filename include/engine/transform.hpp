#pragma once
#include <engine/engine.h>
#include <engine/math.hpp>
#include <flecs.h>

/* ── Engine-owned transform components ───────────────────────────────────────
 * Call Engine::Transform::Init(world) once from EngineRun after CreateWorld().
 * Users set Transform on entities; the engine computes WorldMatrix every frame.
 */

namespace Engine {
namespace Transform {

/* Component IDs, populated by Init(). */
ENGINE_API extern ecs_entity_t id_Transform;
ENGINE_API extern ecs_entity_t id_WorldMatrix;

#pragma pack(push, 1)

struct TransformComp {
    Math::ScryVec3 position;
    Math::ScryVec3 rotation; /* Euler angles in radians (X=pitch, Y=yaw, Z=roll) */
    Math::ScryVec3 scale;
};

struct WorldMatrix {
    Math::ScryMat4 value;
};

#pragma pack(pop)

/* Register components + Phase_StateUpdate system. Called once by EngineRun. */
ENGINE_API void Init(ecs_world_t* world);

} // namespace Transform
} // namespace Engine
