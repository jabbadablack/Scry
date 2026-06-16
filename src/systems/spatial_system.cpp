#include <engine/spatial.h>
#include <engine/transform.h>
#include <engine/pipeline.h>
#include <engine/engine.h>
#include <cassert>
#include <cstdio>
#include <cmath>

namespace Engine {
namespace Spatial {

ecs_entity_t id_ChunkCoord = 0;
ecs_entity_t id_ChunkHash  = 0;

void Init(ecs_world_t* world) {
    assert(world != nullptr);
    assert(id_ChunkCoord == 0);

    auto reg = [&](const char* name, size_t sz, size_t align) -> ecs_entity_t {
        ecs_entity_desc_t ed = {}; ed.name = name;
        ecs_component_desc_t cd = {};
        cd.entity = ecs_entity_init(world, &ed);
        cd.type.size      = static_cast<ecs_size_t>(sz);
        cd.type.alignment = static_cast<ecs_size_t>(align);
        return ecs_component_init(world, &cd);
    };
    id_ChunkCoord = reg("ChunkCoord", sizeof(ChunkCoord), alignof(ChunkCoord));
    id_ChunkHash  = reg("ChunkHash",  sizeof(ChunkHash),  alignof(ChunkHash));
    assert(id_ChunkCoord && id_ChunkHash);

    {
        ecs_entity_desc_t ed = {}; ed.name = "SpatialSystem";
        ecs_entity_t sys_ent = ecs_entity_init(world, &ed);
        ecs_add_pair(world, sys_ent, EcsDependsOn, Engine::Pipeline::Phase_StateUpdate);

        ecs_system_desc_t s = {};
        s.entity = sys_ent;
        s.query.terms[0].id    = Engine::Transform::id_Position;
        s.query.terms[0].inout = EcsIn;
        s.query.terms[1].id    = id_ChunkCoord;
        s.query.terms[1].inout = EcsInOut;
        s.query.terms[2].id    = id_ChunkHash;
        s.query.terms[2].inout = EcsInOut;
        s.multi_threaded       = true;

        s.callback = [](ecs_iter_t* it) {
            const Engine::Transform::Position* pos   = (const Engine::Transform::Position*)ecs_field(it, Engine::Transform::Position, 0);
            ChunkCoord*                        coord = (ChunkCoord*)ecs_field(it, ChunkCoord,                  1);
            ChunkHash*                         hash  = (ChunkHash*)ecs_field(it, ChunkHash,                   2);

            for (int i = 0; i < it->count; ++i) {
                const int32_t new_cx = (int32_t)floorf(pos[i].value[0] / CHUNK_SIZE);
                const int32_t new_cy = (int32_t)floorf(pos[i].value[2] / CHUNK_SIZE);

                if (new_cx != coord[i].x || new_cy != coord[i].y) {
                    coord[i].x  = new_cx;
                    coord[i].y  = new_cy;
                    hash[i].hash = CalculateChunkHash(new_cx, new_cy);
                }
            }
        };
        ecs_system_init(world, &s);
    }
}

} // namespace Spatial
} // namespace Engine
