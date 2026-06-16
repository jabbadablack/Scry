#include <engine/engine.h>
#include <engine/pipeline.h>
#include <engine/ecs.h>
#include <engine/json.h>
#include <engine/plugin.h>
#include <engine/graphics.h>
#include <engine/renderer.h>
#include <engine/transform.h>
#include <engine/camera.h>
#include <cassert>
#include <flecs.h>
#include <cstdio>
#include <exception>
#include <filesystem>

namespace fs = std::filesystem;

static void AppLog(const char* msg) {
    std::printf("[AppLog] %s\n", msg);
    std::fflush(stdout);
}

static Engine::Graphics::MeshAllocation DiscoverAndLoadMesh(const char* filename) {
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

    char err_msg[512];
    std::snprintf(err_msg, sizeof(err_msg),
        "[Init] FATAL: Could not find asset %s in any candidate path.", filename);
    EngineLog(err_msg);
    std::snprintf(err_msg, sizeof(err_msg),
        "[Init] Current working directory: %s", fs::current_path().string().c_str());
    EngineLog(err_msg);

    return {0, 0, 0};
}

static void OnInit(Context* ctx) {
    assert(ctx != nullptr);
    Engine::JSON::LoadProjectConfig(ctx, nullptr);

    ecs_world_t* world = GetWorld(ctx);

    Engine::Graphics::MeshAllocation alloc = DiscoverAndLoadMesh("suzanne.scrymesh");
    if (alloc.indexCount == 0) return;

    // Spawn 10000 entities in a 100×100 grid
    for (int i = 0; i < 10000; ++i) {
        int row = i / 100;
        int col = i % 100;

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

        Engine::Transform::WorldMatrix wm = { Engine::Math::ScryMat4::Identity() };
        ecs_set_id(world, ent, Engine::Transform::id_WorldMatrix, sizeof(wm), &wm);

        Engine::Transform::DirtyMatrixIntent dirty = { 1 };
        ecs_set_id(world, ent, Engine::Transform::id_DirtyMatrix, sizeof(dirty), &dirty);

        Engine::Renderer::MeshData md = { alloc };
        ecs_set_id(world, ent, Engine::Renderer::id_MeshData, sizeof(md), &md);

        Engine::Renderer::AABB aabb;
        aabb.min = Engine::Math::ScryVec3(-2.0f, -2.0f, -2.0f);
        aabb.max = Engine::Math::ScryVec3( 2.0f,  2.0f,  2.0f);
        ecs_set_id(world, ent, Engine::Renderer::id_AABB, sizeof(aabb), &aabb);

        Engine::Renderer::Intent intent = { Engine::Renderer::INTENT_VISIBLE };
        ecs_set_id(world, ent, Engine::Renderer::id_EntityIntent, sizeof(intent), &intent);

        Engine::Renderer::Material mat = { 0, {1.0f, 1.0f, 1.0f, 1.0f} };
        ecs_set_id(world, ent, Engine::Renderer::id_Material, sizeof(mat), &mat);
    }

    // Create camera
    {
        ecs_entity_desc_t ed = {};
        ed.name = "MainCamera";
        ecs_entity_t cam_ent = ecs_entity_init(world, &ed);

        Engine::Camera::Camera cam = {};
        cam.position  = {0, 5, -15};
        cam.pitch     = 0.2f;
        cam.yaw       = 0.0f;
        for (int i = 0; i < 16; ++i) cam.view[i] = cam.proj[i] = 0.0f;
        cam.view[0] = cam.view[5] = cam.view[10] = cam.view[15] = 1.0f;
        cam.proj[0] = cam.proj[5] = cam.proj[10] = cam.proj[15] = 1.0f;

        ecs_set_id(world, cam_ent, Engine::Camera::id_Camera, sizeof(cam), &cam);
    }

    // RotateSystem: yaw each entity and mark transform dirty
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
            Engine::Transform::Rotation*          rot   = ecs_field(it, Engine::Transform::Rotation,          0);
            Engine::Transform::DirtyMatrixIntent* dirty = ecs_field(it, Engine::Transform::DirtyMatrixIntent, 1);
            for (int i = 0; i < it->count; ++i) {
                rot[i].value.y() += it->delta_time;
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
        if (err != SUCCESS) {
            std::fprintf(stderr, "[Main] Engine failed to start with error code: %d\n", (int)err);
            std::printf("Press ENTER to exit...");
            std::getchar();
            return 1;
        }
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "[Main] Fatal exception: %s\n", ex.what());
        std::printf("Press ENTER to exit...");
        std::getchar();
        return 1;
    }

    return 0;
}
