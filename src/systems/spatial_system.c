#include <engine/spatial.h>
#include <engine/transform.h>
#include <engine/pipeline.h>
#include <engine/engine.h>
#include <engine/debug/profiler.h>
#include <flecs.h>
#include <assert.h>
#include <math.h>

ENGINE_API uint64_t id_ScryChunkCoord = 0;
ENGINE_API uint64_t id_ScryChunkHash  = 0;

static void SpatialSystemCallback(ecs_iter_t* it) {
    SCRY_PROFILE_ZONE(Phase_React_Spatial);
    ScryPosition* pos = ecs_field(it, ScryPosition, 0);
    ScryChunkCoord* coord = ecs_field(it, ScryChunkCoord, 1);
    ScryChunkHash* hash = ecs_field(it, ScryChunkHash, 2);

    for (int i = 0; i < it->count; ++i) {
        const int32_t new_cx = (int32_t)floorf(pos[i].value[0] / SCRY_CHUNK_SIZE);
        const int32_t new_cy = (int32_t)floorf(pos[i].value[2] / SCRY_CHUNK_SIZE);

        if (new_cx != coord[i].x || new_cy != coord[i].y) {
            coord[i].x = new_cx;
            coord[i].y = new_cy;
            hash[i].hash = ScrySpatial_CalculateChunkHash(new_cx, new_cy);
        }
    }
    SCRY_PROFILE_ZONE_END(Phase_React_Spatial);
}

void ScrySpatial_Init(struct ecs_world_t* world) {
    assert(world != NULL);

    {
        ecs_entity_desc_t ed = { .name = "ScryChunkCoord" };
        ecs_component_desc_t cd = {
            .entity = ecs_entity_init(world, &ed),
            .type.size = sizeof(ScryChunkCoord),
            .type.alignment = _Alignof(ScryChunkCoord)
        };
        id_ScryChunkCoord = ecs_component_init(world, &cd);
    }

    {
        ecs_entity_desc_t ed = { .name = "ScryChunkHash" };
        ecs_component_desc_t cd = {
            .entity = ecs_entity_init(world, &ed),
            .type.size = sizeof(ScryChunkHash),
            .type.alignment = _Alignof(ScryChunkHash)
        };
        id_ScryChunkHash = ecs_component_init(world, &cd);
    }

    {
        ecs_system_desc_t s = {0};
        s.entity = ecs_entity_init(world, &(ecs_entity_desc_t){ .name = "SpatialSystem" });
        s.query.terms[0].id = (ecs_entity_t)id_ScryPosition;    s.query.terms[0].inout = EcsIn;
        s.query.terms[1].id = (ecs_entity_t)id_ScryChunkCoord;  s.query.terms[1].inout = EcsInOut;
        s.query.terms[2].id = (ecs_entity_t)id_ScryChunkHash;   s.query.terms[2].inout = EcsInOut;
        s.callback = SpatialSystemCallback;
        s.multi_threaded = true;

        ecs_add_pair(world, s.entity, EcsDependsOn, (ecs_entity_t)ScryPhase_React);
        ecs_system_init(world, &s);
    }
}
