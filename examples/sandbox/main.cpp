#include <engine/engine.h>
#include <engine/pipeline.hpp>
#include <engine/ecs.hpp>
#include <engine/json.hpp>
#include <engine/plugin.hpp>
#include <engine/graphics.hpp>
#include <engine/renderer.hpp>
#include <engine/transform.hpp>
#include <engine/camera.hpp>
#include <libassert/assert.hpp>
#include <flecs.h>
#include <cstdio>
#include <exception>
#include <filesystem>

namespace fs = std::filesystem;

// ── Logging callback ──────────────────────────────────────────────────────────

static void AppLog(const char* msg) {
    std::printf("[AppLog] %s\n", msg);
    std::fflush(stdout);
}

// ── Asset Discovery ───────────────────────────────────────────────────────────

static uint32_t DiscoverAndLoadMesh(const char* filename) {
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
    std::snprintf(err_msg, sizeof(err_msg), "[Init] FATAL: Could not find asset %s in any candidate path.", filename);
    EngineLog(err_msg);
    std::snprintf(err_msg, sizeof(err_msg), "[Init] Current working directory: %s", fs::current_path().string().c_str());
    EngineLog(err_msg);

    return Engine::Graphics::INVALID_MESH;
}

// ── Lifecycle callbacks ───────────────────────────────────────────────────────

static void OnInit(Context* ctx) {
    DEBUG_ASSERT(ctx != nullptr);
    Engine::JSON::LoadProjectConfig(ctx, nullptr);

    ecs_world_t* world = GetWorld(ctx);

    // 1. Load mesh
    uint32_t suzanne_handle = DiscoverAndLoadMesh("suzanne.scrymesh");
    if (suzanne_handle == Engine::Graphics::INVALID_MESH) {
        return;
    }

    // 2. Create 100 entities in a 10×10 grid
    for (int i = 0; i < 1000; ++i) {
        int row = i / 10;
        int col = i % 10;

        char name[32];
        std::snprintf(name, sizeof(name), "Entity_%d", i);
        ecs_entity_desc_t e = {};
        e.name = name;
        ecs_entity_t ent = ecs_entity_init(world, &e);

        Engine::Transform::TransformComp tf = {};
        tf.position = { (float)(col - 5) * 3.0f, 0.0f, (float)(row - 5) * 3.0f };
        tf.rotation = { 0, 0, 0 };
        tf.scale    = { 1, 1, 1 };
        ecs_set_id(world, ent, Engine::Transform::id_Transform, sizeof(tf), &tf);

        Engine::Transform::WorldMatrix wm = { Engine::Math::ScryMat4::Identity() };
        ecs_set_id(world, ent, Engine::Transform::id_WorldMatrix, sizeof(wm), &wm);

        Engine::Renderer::MeshInstance mi = { suzanne_handle };
        ecs_set_id(world, ent, Engine::Renderer::id_MeshInstance, sizeof(mi), &mi);

        Engine::Renderer::Intent intent = { Engine::Renderer::INTENT_VISIBLE };
        ecs_set_id(world, ent, Engine::Renderer::id_EntityIntent, sizeof(intent), &intent);

        Engine::Renderer::Material mat = { 0, {1.0f, 1.0f, 1.0f, 1.0f} };
        ecs_set_id(world, ent, Engine::Renderer::id_Material, sizeof(mat), &mat);
    }

    // 3. Create Camera
    {
        ecs_entity_desc_t ed = {};
        ed.name = "MainCamera";
        ecs_entity_t cam_ent = ecs_entity_init(world, &ed);
        
        Engine::Camera::Camera cam = {};
        cam.position  = {0, 5, -15};
        cam.pitch     = 0.2f;
        cam.yaw       = 0.0f;
        for(int i=0; i<16; ++i) { cam.view[i] = cam.proj[i] = 0.0f; }
        cam.view[0] = cam.view[5] = cam.view[10] = cam.view[15] = 1.0f;
        cam.proj[0] = cam.proj[5] = cam.proj[10] = cam.proj[15] = 1.0f;

        ecs_set_id(world, cam_ent, Engine::Camera::id_Camera, sizeof(cam), &cam);
    }

    // 5. Rotation system
    {
        ecs_entity_desc_t ed = {};
        ed.name = "RotateSystem";
        ecs_entity_t sys_ent = ecs_entity_init(world, &ed);
        ecs_add_pair(world, sys_ent, EcsDependsOn, Engine::Pipeline::Phase_Intent);

        ecs_system_desc_t s = {};
        s.entity = sys_ent;
        s.query.terms[0].id = Engine::Transform::id_Transform;
        s.query.terms[0].inout = EcsInOut;

        s.callback = [](ecs_iter_t* it) {
            Engine::Transform::TransformComp* tf = ecs_field(it, Engine::Transform::TransformComp, 0);
            for (int i = 0; i < it->count; ++i) {
                tf[i].rotation.y() += it->delta_time;
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

// ── Entry point ───────────────────────────────────────────────────────────────

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
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[Main] Fatal exception: %s\n", e.what());
        std::printf("Press ENTER to exit...");
        std::getchar();
        return 1;
    }

    return 0;
}
