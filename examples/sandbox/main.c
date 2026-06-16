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
#include <engine/input.h>
#include <flecs.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static void AppLog(const char* msg) {
    printf("[AppLog] %s\n", msg);
    fflush(stdout);
}

static ScryLODGroup DiscoverAndLoadMesh(const char* filename) {
    const char* paths[] = {
        "assets/cooked/",
        "../assets/cooked/",
        "../../assets/cooked/",
        "bin/assets/cooked/",
        "../bin/assets/cooked/",
        "../../bin/assets/cooked/",
        "build/bin/assets/cooked/",
        "../build/bin/assets/cooked/"
    };

    char full_path[512];
    for (int i = 0; i < 8; ++i) {
        snprintf(full_path, sizeof(full_path), "%s%s", paths[i], filename);
        FILE* f = fopen(full_path, "rb");
        if (f) {
            fclose(f);
            char log_msg[512];
            snprintf(log_msg, sizeof(log_msg), "[Init] Found asset at: %s", full_path);
            Scry_Log(log_msg);
            return ScryGraphics_LoadMesh(full_path);
        }
    }

    ScryLODGroup failed = { .group_id = UINT32_MAX };
    return failed;
}

static void RotateSystemCallback(ecs_iter_t* it) {
    if (it->field_count < 2) {
        printf("[RotateSystem] Error: field_count = %d\n", it->field_count);
        return;
    }
    ScryRotation* rot = ecs_field(it, ScryRotation, 0);
    ScryDirtyMatrixIntent* dirty = ecs_field(it, ScryDirtyMatrixIntent, 1);
    for (int i = 0; i < it->count; ++i) {
        rot[i].value[1] += it->delta_time;
        dirty[i].active = 1;
    }
}

static void OnInit(ScryContext* ctx) {
    assert(ctx != NULL);
    ScryJSON_LoadProjectConfig(ctx, NULL);

    struct ecs_world_t* world = Scry_GetWorld(ctx);

    ScryLODGroup lodGroup = DiscoverAndLoadMesh("suzanne.scrymesh");
    if (lodGroup.group_id == UINT32_MAX) return;

    printf("[Init] id_ScryPosition: %llu\n", (unsigned long long)id_ScryPosition);
    printf("[Init] id_ScryMeshData: %llu\n", (unsigned long long)id_ScryMeshData);
    fflush(stdout);

    for (int i = 0; i < 10000; ++i) {
        int row = i / 100;
        int col = i % 100;

        ecs_entity_t ent = ecs_new(world);

        ecs_set_id(world, ent, id_ScryPosition, sizeof(ScryPosition),
            &(ScryPosition){ .value = {(float)(col - 25) * 3.0f, 0.0f, (float)(row - 25) * 3.0f} });
        ecs_set_id(world, ent, id_ScryRotation, sizeof(ScryRotation),
            &(ScryRotation){ .value = {0.0f, 0.0f, 0.0f} });
        ecs_set_id(world, ent, id_ScryScale, sizeof(ScryScale),
            &(ScryScale){ .value = {1.0f, 1.0f, 1.0f} });

        // DirtyMatrix=1 forces the transform system to compute the world matrix on the first frame.
        ecs_set_id(world, ent, id_ScryDirtyMatrix, sizeof(ScryDirtyMatrixIntent),
            &(ScryDirtyMatrixIntent){ .active = 1 });

        ScryWorldMatrix wm;
        glm_mat4_identity(wm.value);
        ecs_set_id(world, ent, id_ScryWorldMatrix, sizeof(ScryWorldMatrix), &wm);

        ecs_set_id(world, ent, id_ScryChunkCoord, sizeof(ScryChunkCoord),
            &(ScryChunkCoord){ .x = 0, .y = 0 });
        ecs_set_id(world, ent, id_ScryChunkHash, sizeof(ScryChunkHash),
            &(ScryChunkHash){ .hash = 0 });

        // 32-byte GPU-aligned AABB matching ShaderAABB in cull.hlsl (vec3 + float pad).
        ecs_set_id(world, ent, id_ScryAABB, sizeof(ScryAABB),
            &(ScryAABB){ .min = {-2.0f, -2.0f, -2.0f}, .pad0 = 0.0f,
                         .max = { 2.0f,  2.0f,  2.0f}, .pad1 = 0.0f });

        ecs_set_id(world, ent, id_ScryMeshData, sizeof(ScryMeshData),
            &(ScryMeshData){ .lod_group_id = lodGroup.group_id });
        ecs_set_id(world, ent, id_ScryEntityIntent, sizeof(ScryRendererIntent),
            &(ScryRendererIntent){ .mask = SCRY_INTENT_VISIBLE });
        ecs_set_id(world, ent, id_ScryMaterial, sizeof(ScryMaterial),
            &(ScryMaterial){ .program_handle = 0, .base_color = {1.0f, 1.0f, 1.0f, 1.0f} });
    }

    {
        ecs_entity_desc_t ed = { .name = "MainCamera" };
        ecs_entity_t cam_ent = ecs_entity_init(world, &ed);
        ScryCamera cam = {0};
        glm_vec3_copy((vec3){0, 5, -15}, cam.position);
        cam.pitch = 0.2f;
        cam.yaw = 0.0f;
        glm_mat4_identity((float (*)[4])cam.view);
        glm_mat4_identity((float (*)[4])cam.proj);
        ecs_set_id(world, cam_ent, id_ScryCamera, sizeof(cam), &cam);
    }

    {
        ecs_entity_desc_t ed = { .name = "RotateSystem" };
        ecs_entity_t sys_ent = ecs_entity_init(world, &ed);
        ecs_add_pair(world, sys_ent, EcsDependsOn, (ecs_entity_t)ScryPhase_Intent);

        ecs_system_desc_t sd = {
            .entity = sys_ent,
            .query.terms[0] = { .id = (ecs_entity_t)id_ScryRotation, .inout = EcsInOut },
            .query.terms[1] = { .id = (ecs_entity_t)id_ScryDirtyMatrix, .inout = EcsInOut },
            .callback = RotateSystemCallback
        };
        ecs_system_init(world, &sd);
    }

    Scry_Log("[Init] Scene ready.");
}

static void OnShutdown(ScryContext* ctx) {
    (void)ctx;
    Scry_Log("[Shutdown] Sandbox complete");
    ScryPlugin_UnloadPlugins();
}

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    ScryAppConfig config = {0};
    config.title = "Scry Sandbox (C)";
    config.window_width = 1280;
    config.window_height = 720;
    config.OnInit = OnInit;
    config.OnShutdown = OnShutdown;
    config.OnLog = AppLog;
    config.global_memory_pool_size = 256 * 1024;
    config.thread_count = 1;

    ScryError err = Scry_Run(&config);
    return (err == SCRY_SUCCESS) ? 0 : 1;
}
