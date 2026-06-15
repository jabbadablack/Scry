#include <engine/renderer.hpp>
#include <engine/transform.hpp>
#include <engine/camera.hpp>
#include <engine/pipeline.hpp>
#include <engine/math.hpp>

#include <bgfx/bgfx.h>
#include <bx/math.h>
#include <libassert/assert.hpp>
#include <cstring>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <vector>
#include <map>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace fs = std::filesystem;

namespace Engine {
namespace Graphics {
    bgfx::VertexBufferHandle GetVertexBuffer(uint32_t handle);
    bgfx::IndexBufferHandle GetIndexBuffer(uint32_t handle);
}
}

namespace Engine {
namespace Renderer {

ecs_entity_t id_MeshInstance = 0;
ecs_entity_t id_EntityIntent = 0;
ecs_entity_t id_Material     = 0;

static bgfx::ProgramHandle g_default_program = BGFX_INVALID_HANDLE;
static bgfx::UniformHandle u_baseColor       = BGFX_INVALID_HANDLE;
static ecs_query_t*        g_render_query    = nullptr;
static ecs_query_t*        g_camera_query    = nullptr;
static bool g_first_frame_submitted = false;

static const bgfx::Memory* LoadShaderFile(const char* filepath) {
    DEBUG_ASSERT(filepath != nullptr);
    DEBUG_ASSERT(std::strlen(filepath) > 0);

    FILE* file = std::fopen(filepath, "rb");
    if (!file) {
        std::fprintf(stderr, "[Renderer] ERROR: Could not open shader file %s\n", filepath);
        return nullptr;
    }
    std::fseek(file, 0, SEEK_END);
    long size = std::ftell(file);
    DEBUG_ASSERT(size > 0);
    std::fseek(file, 0, SEEK_SET);

    const bgfx::Memory* mem = bgfx::alloc(static_cast<uint32_t>(size));
    DEBUG_ASSERT(mem != nullptr);
    DEBUG_ASSERT(mem->data != nullptr);

    size_t read_bytes = std::fread(mem->data, 1, static_cast<size_t>(size), file);
    DEBUG_ASSERT(read_bytes == static_cast<size_t>(size));
    (void)read_bytes;
    std::fclose(file);
    return mem;
}

static uint32_t DiscoverAndLoadShader(const char* filename, bgfx::ShaderHandle& out_handle) {
    DEBUG_ASSERT(filename != nullptr);
    std::string paths[] = {
        std::string("assets/cooked/shaders/") + filename,
        std::string("../assets/cooked/shaders/") + filename,
        std::string("../../assets/cooked/shaders/") + filename,
        std::string("bin/assets/cooked/shaders/") + filename,
        std::string("build/bin/assets/cooked/shaders/") + filename,
        std::string("../build/bin/assets/cooked/shaders/") + filename
    };

    for (const auto& path : paths) {
        if (fs::exists(path)) {
            const bgfx::Memory* mem = LoadShaderFile(path.c_str());
            if (mem) {
                out_handle = bgfx::createShader(mem);
                DEBUG_ASSERT(bgfx::isValid(out_handle));
                return 1;
            }
        }
    }
    return 0;
}

void Init(ecs_world_t* world) {
    EngineLog("[Renderer] Initializing material system...");
    DEBUG_ASSERT(world != nullptr);

    // ── Load Shaders ─────────────────────────────────────────────────────────
    bgfx::ShaderHandle vsh = BGFX_INVALID_HANDLE;
    bgfx::ShaderHandle fsh = BGFX_INVALID_HANDLE;
    
    uint32_t vs_ok = DiscoverAndLoadShader("vs_mesh.bin", vsh);
    uint32_t fs_ok = DiscoverAndLoadShader("fs_mesh.bin", fsh);
    DEBUG_ASSERT(vs_ok == 1);
    DEBUG_ASSERT(fs_ok == 1);

    if (bgfx::isValid(vsh) && bgfx::isValid(fsh)) {
        g_default_program = bgfx::createProgram(vsh, fsh, true);
    }
    DEBUG_ASSERT(bgfx::isValid(g_default_program));

    u_baseColor = bgfx::createUniform("u_baseColor", bgfx::UniformType::Vec4);
    DEBUG_ASSERT(bgfx::isValid(u_baseColor));

    // ── Register Components ──────────────────────────────────────────────────
    {
        ecs_entity_desc_t ed = {};
        ed.name = "MeshInstance";
        ecs_entity_t ent = ecs_entity_init(world, &ed);
        ecs_component_desc_t cd = {};
        cd.entity = ent;
        cd.type.size = sizeof(MeshInstance);
        cd.type.alignment = alignof(MeshInstance);
        id_MeshInstance = ecs_component_init(world, &cd);
        DEBUG_ASSERT(id_MeshInstance != 0);

        ed.name = "EntityIntent";
        ent = ecs_entity_init(world, &ed);
        cd.entity = ent;
        cd.type.size = sizeof(Intent);
        cd.type.alignment = alignof(Intent);
        id_EntityIntent = ecs_component_init(world, &cd);
        DEBUG_ASSERT(id_EntityIntent != 0);

        ed.name = "Material";
        ent = ecs_entity_init(world, &ed);
        cd.entity = ent;
        cd.type.size = sizeof(Material);
        cd.type.alignment = alignof(Material);
        id_Material = ecs_component_init(world, &cd);
        DEBUG_ASSERT(id_Material != 0);
    }

    // ── Initialize queries ──────────────────────────────────────────────
    {
        ecs_query_desc_t cq = {};
        cq.terms[0].id = Engine::Camera::id_Camera;
        g_camera_query = ecs_query_init(world, &cq);
        DEBUG_ASSERT(g_camera_query != nullptr);

        ecs_query_desc_t rq = {};
        rq.terms[0].id = Engine::Transform::id_WorldMatrix;
        rq.terms[1].id = id_MeshInstance;
        rq.terms[2].id = id_Material;
        rq.terms[3].id = id_EntityIntent;
        g_render_query = ecs_query_init(world, &rq);
        DEBUG_ASSERT(g_render_query != nullptr);
    }

    // ── Render System (Phase_React) ──────────────────────────────────────────
    {
        ecs_entity_desc_t ed = {};
        ed.name = "RenderSystem";
        ecs_entity_t sys_ent = ecs_entity_init(world, &ed);
        ecs_add_pair(world, sys_ent, EcsDependsOn, Engine::Pipeline::Phase_React);

        ecs_system_desc_t s = {};
        s.entity = sys_ent;
        s.callback = [](ecs_iter_t* it) {
            DEBUG_ASSERT(it != nullptr);

            // 1. Setup global view state
            const bgfx::Stats* stats = bgfx::getStats();
            DEBUG_ASSERT(stats != nullptr);
            bgfx::setViewRect(0, 0, 0, (uint16_t)stats->width, (uint16_t)stats->height);
            bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x111111FF, 1.0f, 0);

            if (g_camera_query) {
                ecs_iter_t cam_it = ecs_query_iter(it->world, g_camera_query);
                while (ecs_query_next(&cam_it)) { // Iter until end to avoid Flecs leak
                    const Engine::Camera::Camera* cam = ecs_field(&cam_it, Engine::Camera::Camera, 0);
                    DEBUG_ASSERT(cam != nullptr);
                    bgfx::setViewTransform(0, cam[0].view, cam[0].proj);
                }
            }

            // 2. Data-oriented grouping: Group entities by (Mesh, Material)
            struct Batch {
                uint32_t mesh_id;
                uint16_t program;
                float    color[4];
                std::vector<const Math::ScryMat4*> matrices;
            };
            std::map<std::pair<uint32_t, uint16_t>, Batch> batches;

            ecs_iter_t render_it = ecs_query_iter(it->world, g_render_query);
            while (ecs_query_next(&render_it)) {
                const Engine::Transform::WorldMatrix* wm = ecs_field(&render_it, Engine::Transform::WorldMatrix, 0);
                const MeshInstance*                   mi = ecs_field(&render_it, MeshInstance, 1);
                const Material*                       mat = ecs_field(&render_it, Material, 2);
                const Intent*                         intent = ecs_field(&render_it, Intent, 3);

                DEBUG_ASSERT(wm != nullptr && mi != nullptr && mat != nullptr && intent != nullptr);

                for (int i = 0; i < render_it.count; ++i) {
                    if ((intent[i].mask & INTENT_VISIBLE) && !(intent[i].mask & INTENT_DESTROYED)) {
                        auto key = std::make_pair(mi[i].mesh_id, mat[i].program_handle);
                        auto& b = batches[key];
                        b.mesh_id = mi[i].mesh_id;
                        b.program = mat[i].program_handle;
                        std::memcpy(b.color, mat[i].base_color, sizeof(float)*4);
                        b.matrices.push_back(&wm[i].value);
                    }
                }
            }

            // 3. Submit batches
            for (auto& pair : batches) {
                auto& b = pair.second;
                uint32_t count = static_cast<uint32_t>(b.matrices.size());
                DEBUG_ASSERT(count > 0);

                bgfx::InstanceDataBuffer idb;
                const uint16_t stride = 64; 
                if (count <= bgfx::getAvailInstanceDataBuffer(count, stride)) {
                    bgfx::allocInstanceDataBuffer(&idb, count, stride);

                    uint8_t* data = idb.data;
                    for (const auto* mtx : b.matrices) {
                        std::memcpy(data, mtx->data(), stride);
                        data += stride;
                    }

                    bgfx::VertexBufferHandle vbh = Graphics::GetVertexBuffer(b.mesh_id);
                    bgfx::IndexBufferHandle  ibh = Graphics::GetIndexBuffer(b.mesh_id);
                    bgfx::ProgramHandle      prog = { b.program };

                    if (!bgfx::isValid(prog)) prog = g_default_program;

                    if (bgfx::isValid(vbh) && bgfx::isValid(ibh) && bgfx::isValid(prog)) {
                        bgfx::setVertexBuffer(0, vbh);
                        bgfx::setIndexBuffer(ibh);
                        bgfx::setInstanceDataBuffer(&idb);
                        bgfx::setUniform(u_baseColor, b.color);
                        bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS);
                        bgfx::submit(0, prog);

                        if (!g_first_frame_submitted) {
                            char log[128];
                            std::snprintf(log, sizeof(log), "[Renderer] First draw submitted: %u instances of mesh %u", count, b.mesh_id);
                            EngineLog(log);
                            g_first_frame_submitted = true;
                        }
                    }
                }
            }
        };
        ecs_system_init(world, &s);
    }
    EngineLog("[Renderer] Material system and RenderSystem ready");
}

void Shutdown() {
    EngineLog("[Renderer] Cleaning up...");
    if (bgfx::isValid(g_default_program)) bgfx::destroy(g_default_program);
    if (bgfx::isValid(u_baseColor))       bgfx::destroy(u_baseColor);
    if (g_camera_query) ecs_query_fini(g_camera_query);
    if (g_render_query) ecs_query_fini(g_render_query);
    EngineLog("[Renderer] Shutdown complete");
}

} // namespace Renderer
} // namespace Engine
