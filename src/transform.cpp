#include <engine/transform.hpp>
#include <engine/pipeline.hpp>
#include <engine/renderer.hpp>
#include <cassert>
#include <Eigen/Geometry>
#include <cstdio>

namespace Engine {
namespace Transform {

ecs_entity_t id_Transform  = 0;
ecs_entity_t id_WorldMatrix = 0;

void Init(ecs_world_t* world) {
    assert(world != nullptr);

    // ── Register TransformComp component ─────────────────────────────────────
    {
        ecs_entity_desc_t e    = {};
        e.name                 = "Transform";
        ecs_component_desc_t c = {};
        c.entity               = ecs_entity_init(world, &e);
        c.type.size            = sizeof(TransformComp);
        c.type.alignment       = alignof(TransformComp);
        id_Transform           = ecs_component_init(world, &c);
        assert(id_Transform != 0);
    }

    // ── Register WorldMatrix component ────────────────────────────────────────
    {
        ecs_entity_desc_t e    = {};
        e.name                 = "WorldMatrix";
        ecs_component_desc_t c = {};
        c.entity               = ecs_entity_init(world, &e);
        c.type.size            = sizeof(WorldMatrix);
        c.type.alignment       = alignof(WorldMatrix);
        id_WorldMatrix         = ecs_component_init(world, &c);
        assert(id_WorldMatrix != 0);
    }

    // ── Register Phase_StateUpdate transform system ───────────────────────────
    {
        ecs_entity_desc_t e        = {};
        e.name                     = "TransformSystem";
        const ecs_entity_t sys_ent = ecs_entity_init(world, &e);
        assert(sys_ent != 0);
        ecs_add_pair(world, sys_ent, EcsDependsOn, Engine::Pipeline::Phase_StateUpdate);

        ecs_system_desc_t s    = {};
        s.entity               = sys_ent;
        s.query.terms[0].id    = id_Transform;
        s.query.terms[0].inout = EcsIn;
        s.query.terms[1].id    = id_WorldMatrix;
        s.query.terms[1].inout = EcsInOut;
        
        s.callback = [](ecs_iter_t* it) {
            const TransformComp* tf = ecs_field(it, TransformComp, 0);
            WorldMatrix*         wm = ecs_field(it, WorldMatrix, 1);
            
            assert(tf != nullptr && wm != nullptr);

            for (int i = 0; i < it->count; ++i) {
                Eigen::Affine3f trs = Eigen::Affine3f::Identity();
                trs.translate(Eigen::Vector3f(tf[i].position.x(), tf[i].position.y(), tf[i].position.z()));
                trs.rotate(
                    Eigen::AngleAxisf(tf[i].rotation.x(), Eigen::Vector3f::UnitX()) *
                    Eigen::AngleAxisf(tf[i].rotation.y(), Eigen::Vector3f::UnitY()) *
                    Eigen::AngleAxisf(tf[i].rotation.z(), Eigen::Vector3f::UnitZ()));
                trs.scale(Eigen::Vector3f(tf[i].scale.x(), tf[i].scale.y(), tf[i].scale.z()));

                wm[i].value = trs.matrix();
            }
        };

        const ecs_entity_t sys = ecs_system_init(world, &s);
        assert(sys != 0);
    }

#ifndef NDEBUG
    EngineLog("[Transform] Components and system registered");
#endif
}

} // namespace Transform
} // namespace Engine
