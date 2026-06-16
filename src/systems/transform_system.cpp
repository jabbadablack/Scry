#include <engine/transform.h>
#include <engine/pipeline.h>
#include <engine/engine.h>
#include <cassert>
#include <cstdio>
#include <cglm/cglm.h>
#include <cglm/struct.h>

namespace Engine {
namespace Transform {

ecs_entity_t id_Position  = 0;
ecs_entity_t id_Rotation  = 0;
ecs_entity_t id_Scale     = 0;
ecs_entity_t id_WorldMatrix = 0;
ecs_entity_t id_DirtyMatrix = 0;

void Init(ecs_world_t* world) {
    assert(world != nullptr);
    assert(id_Position == 0);

    auto reg = [&](const char* name, size_t sz, size_t align) -> ecs_entity_t {
        ecs_entity_desc_t ed = {}; ed.name = name;
        ecs_component_desc_t cd = {};
        cd.entity = ecs_entity_init(world, &ed);
        cd.type.size = static_cast<ecs_size_t>(sz);
        cd.type.alignment = static_cast<ecs_size_t>(align);
        return ecs_component_init(world, &cd);
    };

    id_Position    = reg("Position",         sizeof(Position),          alignof(Position));
    id_Rotation    = reg("Rotation",         sizeof(Rotation),          alignof(Rotation));
    id_Scale       = reg("Scale",            sizeof(Scale),             alignof(Scale));
    id_WorldMatrix = reg("WorldMatrix",      sizeof(WorldMatrix),       alignof(WorldMatrix));
    id_DirtyMatrix = reg("DirtyMatrixIntent",sizeof(DirtyMatrixIntent), alignof(DirtyMatrixIntent));

    assert(id_Position && id_Rotation && id_Scale && id_WorldMatrix && id_DirtyMatrix);

    {
        ecs_entity_desc_t ed = {}; ed.name = "TransformSystem";
        ecs_entity_t sys_ent = ecs_entity_init(world, &ed);
        ecs_add_pair(world, sys_ent, EcsDependsOn, Engine::Pipeline::Phase_StateUpdate);

        ecs_system_desc_t s = {};
        s.entity = sys_ent;
        s.query.terms[0].id    = id_Position;
        s.query.terms[0].inout = EcsIn;
        s.query.terms[1].id    = id_Rotation;
        s.query.terms[1].inout = EcsIn;
        s.query.terms[2].id    = id_Scale;
        s.query.terms[2].inout = EcsIn;
        s.query.terms[3].id    = id_WorldMatrix;
        s.query.terms[3].inout = EcsInOut;
        s.query.terms[4].id    = id_DirtyMatrix;
        s.query.terms[4].inout = EcsInOut;
        s.multi_threaded       = true;

        s.callback = [](ecs_iter_t* it) {
            const Position*    pos   = ecs_field(it, Position,          0);
            const Rotation*    rot   = ecs_field(it, Rotation,          1);
            const Scale*       scl   = ecs_field(it, Scale,             2);
            WorldMatrix*       wm    = ecs_field(it, WorldMatrix,       3);
            DirtyMatrixIntent* dirty = ecs_field(it, DirtyMatrixIntent, 4);

            // Targeted SoA Path:
            // In a more complex system, we would transpose to SoA here for SIMD.
            // For now, we use cglm's optimized C math which is 0-allocation.
            for (int i = 0; i < it->count; ++i) {
                if (!dirty[i].active) continue;
                dirty[i].active = 0;

                mat4 m;
                glm_mat4_identity(m);
                
                // Translation
                glm_translate(m, (float*)pos[i].value);

                // Rotation (Euler YXZ)
                glm_rotate(m, rot[i].value[1], (vec3){0.0f, 1.0f, 0.0f}); // Yaw
                glm_rotate(m, rot[i].value[0], (vec3){1.0f, 0.0f, 0.0f}); // Pitch
                glm_rotate(m, rot[i].value[2], (vec3){0.0f, 0.0f, 1.0f}); // Roll

                // Scale
                glm_scale(m, (float*)scl[i].value);

                glm_mat4_copy(m, wm[i].value);
            }
        };

        ecs_system_init(world, &s);
    }
}

} // namespace Transform
} // namespace Engine
