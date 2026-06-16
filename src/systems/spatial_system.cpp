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

/**
 * @brief Sets up the spatial partitioning system by registering chunking components and systems.
 *
 * This function prepares the engine to handle large worlds by dividing them into manageable chunks.
 * It's the foundation of our spatial awareness!
 *
 * @param world A pointer to the ECS world.
 *
 * @example
 * ecs_world_t* world = ecs_init();
 * Engine::Spatial::Init(world);
 */
void Init(ecs_world_t* world) {
    assert(world != nullptr);
    assert(id_ChunkCoord == 0); // Avoid double init
    EngineLog("Spatial::Init: Starting spatial system setup.");
    EngineLog("Registering ChunkCoord and ChunkHash components.");

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

    // SpatialSystem — Phase_StateUpdate, after TransformSystem.
    // Always compares new chunk against stored chunk; only writes if changed.
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
        s.multi_threaded       = true; // per-entity chunk recalc; no shared mutable state

        /**
         * @brief System callback that updates chunk coordinates and hashes based on entity positions.
         * 
         * This lambda keeps track of which chunk every entity belongs to, making spatial queries a breeze!
         * It only updates when an entity actually crosses a chunk boundary.
         * 
         * @param it The ECS iterator.
         * 
         * @example
         * // Triggered by the ECS system runner
         * s.callback(it);
         */
        s.callback = [](ecs_iter_t* it) {
            assert(it != nullptr);
            assert(it->count >= 0);
            static bool logged_once = false;
            if (!logged_once) {
                EngineLog("SpatialSystem callback: Updating entity spatial data.");
                EngineLog("Checking chunk boundaries for entities...");
                logged_once = true;
            }

            const Engine::Transform::Position* pos   = ecs_field(it, Engine::Transform::Position, 0);
            ChunkCoord*                        coord = ecs_field(it, ChunkCoord,                  1);
            ChunkHash*                         hash  = ecs_field(it, ChunkHash,                   2);

            for (int i = 0; i < it->count; ++i) {
                const int32_t new_cx = static_cast<int32_t>(std::floor(pos[i].value.x() / CHUNK_SIZE));
                const int32_t new_cy = static_cast<int32_t>(std::floor(pos[i].value.z() / CHUNK_SIZE));

                if (new_cx != coord[i].x || new_cy != coord[i].y) {
                    coord[i].x  = new_cx;
                    coord[i].y  = new_cy;
                    hash[i].hash = CalculateChunkHash(new_cx, new_cy);
                }
            }
        };
        ecs_system_init(world, &s);
    }

    EngineLog("[Spatial] Chunk grid system initialized");
}

} // namespace Spatial
} // namespace Engine
