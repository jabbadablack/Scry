#include <engine/engine.h>
#include <engine/pipeline.h>
#include <engine/ecs.h>
#include <engine/json.h>
#include <engine/plugin.h>
#include <engine/renderer/core.h>
#include <engine/renderer/renderer.h>
#include <engine/renderer/mesh.h>
#include <engine/transform.h>
#include <engine/camera.h>
#include <engine/spatial.h>
#include <cassert>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <cglm/cglm.h>

namespace fs = std::filesystem;

static void AppLog(const char* msg) {
    std::printf("[AppLog] %s\n", msg);
    std::fflush(stdout);
}

static Engine::Graphics::LODGroup DiscoverAndLoadMesh(const char* filename) {
    std::string paths[] = {
        std::string("assets/cooked/") + filename,
        std::string("../assets/cooked/") + filename,
        std::string("../../assets/cooked/") + filename,
        std::string("bin/assets/cooked/") + filename,
        std::string("../bin/assets/cooked/") + filename,
        std::string("../../bin/assets/cooked/") + filename,
        std::string("build/bin/assets/cooked/") + filename,
        std::string("../build/bin/assets/cooked/") + filename
    };

    for (const auto& path : paths) {
        if (fs::exists(path)) {
            char log_msg[512];
            std::snprintf(log_msg, sizeof(log_msg), "[Init] Found asset at: %s", path.c_str());
            EngineLog(log_msg);
            return Engine::Graphics::LoadMesh(path.c_str());
        }
    }

    Engine::Graphics::LODGroup failed = {};
    failed.group_id = UINT32_MAX;
    return failed;
}

static void OnInit(Context* ctx) {
    assert(ctx != nullptr);
    Engine::JSON::LoadProjectConfig(ctx, nullptr);

    ecs_world_t* world = GetWorld(ctx);

    Engine::Graphics::LODGroup lodGroup = DiscoverAndLoadMesh("suzanne.scrymesh");
    if (lodGroup.group_id == UINT32_MAX) return;

    for (int i = 0; i < 5000; ++i) {
        int row = i / 50;
        int col = i % 50;

        char name[32];
        std::snprintf(name, sizeof(name), "Entity_%d", i);
        ecs_entity_desc_t e = {};
        e.name = name;
        ecs_entity_t ent = ecs_entity_init(world, &e);

        Engine::Transform::Position pos = { {(float)(col - 50) * 3.0f, 0.0f, (float)(row - 50) * 3.0f} };
        ecs_set_id(world, ent, Engine::Transform::id_Position, sizeof(pos), &pos);

        Engine::Transform::Rotation rot = { {0.0f, 0.0f, 0.0f} };
        ecs_set_id(world, ent, Engine::Transform::id_Rotation, sizeof(rot), &rot);

        Engine::Transform::Scale scl = { {1.0f, 1.0f, 1.0f} };
        ecs_set_id(world, ent, Engine::Transform::id_Scale, sizeof(scl), &scl);

        Engine::Transform::WorldMatrix wm;
        glm_mat4_identity(wm.value);
        ecs_set_id(world, ent, Engine::Transform::id_WorldMatrix, sizeof(wm), &wm);

        Engine::Transform::DirtyMatrixIntent dirty = { 1 };
        ecs_set_id(world, ent, Engine::Transform::id_DirtyMatrix, sizeof(dirty), &dirty);

        Engine::Renderer::MeshData md = { lodGroup.group_id };
        ecs_set_id(world, ent, Engine::Renderer::id_MeshData, sizeof(md), &md);

        Engine::Renderer::AABB aabb;
        glm_vec3_copy((vec3){-2.0f, -2.0f, -2.0f}, aabb.min);
        glm_vec3_copy((vec3){ 2.0f,  2.0f,  2.0f}, aabb.max);
        ecs_set_id(world, ent, Engine::Renderer::id_AABB, sizeof(aabb), &aabb);

        Engine::Renderer::Intent intent = { Engine::Renderer::INTENT_VISIBLE };
        ecs_set_id(world, ent, Engine::Renderer::id_EntityIntent, sizeof(intent), &intent);

        Engine::Renderer::Material mat = { 0, {1.0f, 1.0f, 1.0f, 1.0f} };
        ecs_set_id(world, ent, Engine::Renderer::id_Material, sizeof(mat), &mat);

        Engine::Spatial::ChunkCoord coord = {0, 0};
        ecs_set_id(world, ent, Engine::Spatial::id_ChunkCoord, sizeof(coord), &coord);

        Engine::Spatial::ChunkHash chash = {0};
        ecs_set_id(world, ent, Engine::Spatial::id_ChunkHash, sizeof(chash), &chash);
    }

    {
        ecs_entity_desc_t ed = {};
        ed.name = "MainCamera";
        ecs_entity_t cam_ent = ecs_entity_init(world, &ed);

        Engine::Camera::Camera cam = {};
        glm_vec3_copy((vec3){0, 5, -15}, cam.position);
        cam.pitch     = 0.2f;
        cam.yaw       = 0.0f;
        
        mat4 identity = GLM_MAT4_IDENTITY_INIT;
        glm_mat4_copy(identity, (vec4*)cam.view);
        glm_mat4_copy(identity, (vec4*)cam.proj);

        ecs_set_id(world, cam_ent, Engine::Camera::id_Camera, sizeof(cam), &cam);
    }

    {
        ecs_entity_desc_t ed = {};
        ed.name = "RotateSystem";
        ecs_entity_t sys_ent = ecs_entity_init(world, &ed);
        ecs_add_pair(world, sys_ent, EcsDependsOn, Engine::Pipeline::Phase_Intent);

        ecs_system_desc_t s = {};
        s.entity = sys_ent;
        s.query.terms[0].id    = Engine::Transform::id_Rotation;
        s.query.terms[0].inout = EcsInOut;
        s.query.terms[1].id    = Engine::Transform::id_DirtyMatrix;
        s.query.terms[1].inout = EcsInOut;

        s.callback = [](ecs_iter_t* it) {
            Engine::Transform::Rotation*          rot   = (Engine::Transform::Rotation*)ecs_field(it, Engine::Transform::Rotation,          0);
            Engine::Transform::DirtyMatrixIntent* dirty = (Engine::Transform::DirtyMatrixIntent*)ecs_field(it, Engine::Transform::DirtyMatrixIntent, 1);
            for (int i = 0; i < it->count; ++i) {
                rot[i].value[1] += it->delta_time;
                dirty[i].active   = 1;
            }
        };
        ecs_system_init(world, &s);
    }

    EngineLog("[Init] Scene ready.");
}

static void OnShutdown(Context* ctx) {
    (void)ctx;
    EngineLog("[Shutdown] Sandbox complete");
    Engine::Plugin::UnloadPlugins();
}

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    AppConfig config = {};
    config.title                   = "Scry";
    config.window_width            = 1280;
    config.window_height           = 720;
    config.OnInit                  = OnInit;
    config.OnShutdown              = OnShutdown;
    config.OnLog                   = AppLog;
    config.global_memory_pool_size = 256 * 1024;
    config.thread_count            = 1;

    try {
        const EngineError err = EngineRun(&config);
        if (err != SUCCESS) return 1;
    } catch (const std::exception& ex) {
        return 1;
    }

    return 0;
}
