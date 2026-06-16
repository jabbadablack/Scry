#include <engine/transform.h>
#include <engine/pipeline.h>
#include <engine/engine.h>
#include <cassert>
#include <cstdio>
#include <Eigen/Geometry>

namespace Engine {
namespace Transform {

ecs_entity_t id_Position  = 0;
ecs_entity_t id_Rotation  = 0;
ecs_entity_t id_Scale     = 0;
ecs_entity_t id_WorldMatrix = 0;
ecs_entity_t id_DirtyMatrix = 0;

/**
 * @brief Initializes the transform system by registering components and the transform system itself.
 *
 * This function sets up everything needed for handling positions, rotations, and scales within the ECS.
 * It's like giving the engine a sense of space and orientation!
 *
 * @param world A pointer to the ECS world where components and systems will be registered.
 *
 * @example
 * ecs_world_t* world = ecs_init();
 * Engine::Transform::Init(world);
 */
void Init(ecs_world_t* world) {
    assert(world != nullptr);
    assert(id_Position == 0); // Ensure we don't init twice
    EngineLog("Transform::Init starting up...");
    EngineLog("Registering transform components into the ECS world.");

    auto reg = [&](const char* name, size_t sz, size_t align) -> ecs_entity_t {
        ecs_entity_desc_t ed = {}; ed.name = name;
        ecs_component_desc_t cd = {};
        cd.entity = ecs_entity_init(world, &ed);
        cd.type.size = sz;
        cd.type.alignment = align;
        return ecs_component_init(world, &cd);
    };

    id_Position    = reg("Position",         sizeof(Position),          alignof(Position));
    id_Rotation    = reg("Rotation",         sizeof(Rotation),          alignof(Rotation));
    id_Scale       = reg("Scale",            sizeof(Scale),             alignof(Scale));
    id_WorldMatrix = reg("WorldMatrix",      sizeof(WorldMatrix),       alignof(WorldMatrix));
    id_DirtyMatrix = reg("DirtyMatrixIntent",sizeof(DirtyMatrixIntent), alignof(DirtyMatrixIntent));

    assert(id_Position && id_Rotation && id_Scale && id_WorldMatrix && id_DirtyMatrix);

    // TransformSystem (Phase_StateUpdate): only recompute matrices for dirty entities
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

        /**
         * @brief System callback that recomputes world matrices for entities with dirty transform components.
         * 
         * This lambda does the heavy lifting of calculating new transform matrices whenever something moves or rotates.
         * It keeps our world data fresh and accurate for the renderer.
         * 
         * @param it The ECS iterator containing the entities to process.
         * 
         * @example
         * // This is usually called automatically by the ECS runner
         * s.callback(it);
         */
        s.callback = [](ecs_iter_t* it) {
            assert(it != nullptr);
            assert(it->count >= 0);
            static bool logged_once = false;
            if (!logged_once) {
                EngineLog("TransformSystem callback executing...");
                EngineLog("Updating world matrices for dirty entities.");
                logged_once = true;
            }

            const Position*    pos   = ecs_field(it, Position,          0);
            const Rotation*    rot   = ecs_field(it, Rotation,          1);
            const Scale*       scl   = ecs_field(it, Scale,             2);
            WorldMatrix*       wm    = ecs_field(it, WorldMatrix,       3);
            DirtyMatrixIntent* dirty = ecs_field(it, DirtyMatrixIntent, 4);

            for (int i = 0; i < it->count; ++i) {
                if (!dirty[i].active) continue;
                dirty[i].active = 0;

                Eigen::Affine3f trs = Eigen::Affine3f::Identity();
                trs.translate(Eigen::Vector3f(pos[i].value.x(), pos[i].value.y(), pos[i].value.z()));
                trs.rotate(
                    Eigen::AngleAxisf(rot[i].value.x(), Eigen::Vector3f::UnitX()) *
                    Eigen::AngleAxisf(rot[i].value.y(), Eigen::Vector3f::UnitY()) *
                    Eigen::AngleAxisf(rot[i].value.z(), Eigen::Vector3f::UnitZ()));
                trs.scale(Eigen::Vector3f(scl[i].value.x(), scl[i].value.y(), scl[i].value.z()));
                wm[i].value = trs.matrix();
            }
        };

        ecs_system_init(world, &s);
    }

#ifndef NDEBUG
    EngineLog("[Transform] SoA components and reactive system registered");
#endif
}

} // namespace Transform
} // namespace Engine
