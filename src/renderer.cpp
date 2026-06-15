#include <engine/renderer.hpp>
#include <engine/transform.hpp>
#include <engine/camera.hpp>
#include <engine/pipeline.hpp>
#include <engine/math.hpp>
#include <engine/graphics.hpp>

#include <bgfx/bgfx.h>
#include <bx/math.h>
#include <libassert/assert.hpp>
#include <cstring>
#include <cstdio>
#include <filesystem>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace fs = std::filesystem;

namespace Engine {
namespace Graphics {
    bgfx::VertexBufferHandle GetVertexBuffer(uint32_t handle);
    bgfx::IndexBufferHandle  GetIndexBuffer(uint32_t handle);
    uint32_t                 GetIndexCount(uint32_t handle);
}
}

namespace Engine {
namespace Renderer {

ecs_entity_t id_MeshInstance = 0;
ecs_entity_t id_EntityIntent = 0;
ecs_entity_t id_Material     = 0;

static constexpr uint32_t MAX_ENTITIES = 4096u;
static constexpr uint32_t MAX_MESHES   = Engine::Graphics::MAX_MESHES;

// ── Global uniform handle (extern in renderer.hpp) ────────────────────────
bgfx::UniformHandle u_drawParams = BGFX_INVALID_HANDLE;

// ── Programs ──────────────────────────────────────────────────────────────
static bgfx::ProgramHandle g_default_program = BGFX_INVALID_HANDLE;

// ── SSBO handles ──────────────────────────────────────────────────────────
static bgfx::DynamicVertexBufferHandle g_matrixSSBO  = BGFX_INVALID_HANDLE;
static bgfx::VertexLayout              g_mat4_layout;

// ── Queries ───────────────────────────────────────────────────────────────
static ecs_query_t* g_render_query = nullptr;
static ecs_query_t* g_camera_query = nullptr;

// ── Per-frame scratch (static = zero heap allocation) ─────────────────────
static float     s_mat_flat    [MAX_ENTITIES * 16];
static uint32_t  s_entity_count;

static bool g_logged_first_draw = false;

// ── Shader discovery ──────────────────────────────────────────────────────

static const bgfx::Memory* LoadShaderFile(const char* path) {
    FILE* f = std::fopen(path, "rb");
    if (!f) return nullptr;
    std::fseek(f, 0, SEEK_END);
    long sz = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    const bgfx::Memory* mem = bgfx::alloc(static_cast<uint32_t>(sz + 1));
    std::fread(mem->data, 1, static_cast<size_t>(sz), f);
    mem->data[sz] = '\0';
    std::fclose(f);
    return mem;
}

static bool DiscoverShader(const char* filename, bgfx::ShaderHandle& out) {
    const char* dirs[] = {
        "assets/cooked/shaders/",
        "../assets/cooked/shaders/",
        "bin/assets/cooked/shaders/",
        "build/bin/assets/cooked/shaders/",
        "../build/bin/assets/cooked/shaders/"
    };
    char path[512];
    for (const char* d : dirs) {
        std::snprintf(path, sizeof(path), "%s%s", d, filename);
        if (fs::exists(path)) {
            const bgfx::Memory* mem = LoadShaderFile(path);
            if (mem) { out = bgfx::createShader(mem); return bgfx::isValid(out); }
        }
    }
    return false;
}

// ── Init ──────────────────────────────────────────────────────────────────

void Init(ecs_world_t* world) {
    EngineLog("[Renderer] Initializing...");
    DEBUG_ASSERT(world != nullptr);

    // ── Draw program ──────────────────────────────────────────────────────
    bgfx::ShaderHandle vsh = BGFX_INVALID_HANDLE;
    bgfx::ShaderHandle fsh = BGFX_INVALID_HANDLE;
    if (!DiscoverShader("vs_mesh.bin", vsh)) {
        EngineLog("[Renderer] FATAL: Failed to load vs_mesh.bin. Did shaderc fail?");
        DEBUG_ASSERT(false);
    }
    if (!DiscoverShader("fs_mesh.bin", fsh)) {
        EngineLog("[Renderer] FATAL: Failed to load fs_mesh.bin");
        DEBUG_ASSERT(false);
    }
    g_default_program = bgfx::createProgram(vsh, fsh, true);
    DEBUG_ASSERT(bgfx::isValid(g_default_program));

    // ── Matrix SSBO: 64-byte stride (one mat4 per element) ───────────────
    g_mat4_layout.begin()
        .add(bgfx::Attrib::TexCoord0, 4, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord1, 4, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord2, 4, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord3, 4, bgfx::AttribType::Float)
        .end();

    g_matrixSSBO = bgfx::createDynamicVertexBuffer(
        MAX_ENTITIES, g_mat4_layout,
        BGFX_BUFFER_COMPUTE_READ | BGFX_BUFFER_ALLOW_RESIZE);
    DEBUG_ASSERT(bgfx::isValid(g_matrixSSBO));

    // ── Uniforms ──────────────────────────────────────────────────────────
    u_drawParams = bgfx::createUniform("u_drawParams", bgfx::UniformType::Vec4);
    DEBUG_ASSERT(bgfx::isValid(u_drawParams));

    // ── Register components ───────────────────────────────────────────────
    {
        auto reg = [&](const char* name, size_t sz, size_t align) -> ecs_entity_t {
            ecs_entity_desc_t ed = {}; ed.name = name;
            ecs_entity_t e = ecs_entity_init(world, &ed);
            ecs_component_desc_t cd = {};
            cd.entity = e; cd.type.size = sz; cd.type.alignment = align;
            return ecs_component_init(world, &cd);
        };
        id_MeshInstance = reg("MeshInstance", sizeof(MeshInstance), alignof(MeshInstance));
        id_EntityIntent = reg("EntityIntent",  sizeof(Intent),       alignof(Intent));
        id_Material     = reg("Material",      sizeof(Material),     alignof(Material));
        DEBUG_ASSERT(id_MeshInstance && id_EntityIntent && id_Material);
    }

    // ── Queries ───────────────────────────────────────────────────────────
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

    // ── Render System (Phase_React) ───────────────────────────────────────
    {
        ecs_entity_desc_t ed = {};
        ed.name = "RenderSystem";
        ecs_entity_t sys_ent = ecs_entity_init(world, &ed);
        ecs_add_pair(world, sys_ent, EcsDependsOn, Engine::Pipeline::Phase_React);

        ecs_system_desc_t s = {};
        s.entity   = sys_ent;
        s.callback = [](ecs_iter_t* it) {
            DEBUG_ASSERT(it != nullptr);

            // ── View 0 setup ──────────────────────────────────────────────
            const bgfx::Stats* stats = bgfx::getStats();
            bgfx::setViewRect(0, 0, 0, (uint16_t)stats->width, (uint16_t)stats->height);
            bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x303030FF, 1.0f, 0);
            bgfx::touch(0);

            if (g_camera_query) {
                ecs_iter_t ci = ecs_query_iter(it->world, g_camera_query);
                while (ecs_query_next(&ci)) {
                    const Engine::Camera::Camera* cam = ecs_field(&ci, Engine::Camera::Camera, 0);
                    bgfx::setViewTransform(0, cam[0].view, cam[0].proj);
                }
            }

            // ── Collect visible entities (CPU filter) → flat mat4 array ───
            s_entity_count = 0;
            if (g_render_query) {
                ecs_iter_t ri = ecs_query_iter(it->world, g_render_query);
                while (ecs_query_next(&ri)) {
                    const Engine::Transform::WorldMatrix* wm  = ecs_field(&ri, Engine::Transform::WorldMatrix, 0);
                    const Intent*                         ent = ecs_field(&ri, Intent, 3);

                    for (int i = 0; i < ri.count && s_entity_count < MAX_ENTITIES; ++i) {
                        if ((ent[i].mask & INTENT_VISIBLE) == 0) continue;
                        if ((ent[i].mask & INTENT_DESTROYED) != 0) continue;
                        std::memcpy(&s_mat_flat[s_entity_count * 16], wm[i].value.data(), 64u);
                        ++s_entity_count;
                    }
                }
            }

            if (s_entity_count == 0) return;

            // ── Upload packed mat4 array to matrix SSBO ───────────────────
            bgfx::update(g_matrixSSBO, 0,
                bgfx::makeRef(s_mat_flat, s_entity_count * 64u));

            // ── Find first active mesh ────────────────────────────────────
            bgfx::VertexBufferHandle active_vbh = BGFX_INVALID_HANDLE;
            bgfx::IndexBufferHandle  active_ibh = BGFX_INVALID_HANDLE;

            for (uint32_t m = 0; m < MAX_MESHES; ++m) {
                bgfx::VertexBufferHandle vbh = Graphics::GetVertexBuffer(m);
                bgfx::IndexBufferHandle  ibh = Graphics::GetIndexBuffer(m);
                if (bgfx::isValid(vbh) && bgfx::isValid(ibh)) {
                    active_vbh = vbh;
                    active_ibh = ibh;
                    break;
                }
            }
            if (!bgfx::isValid(active_vbh)) return;

            // ── Draw: SSBO vertex pulling + CPU-counted instances ─────────
            constexpr uint64_t kState =
                BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z |
                BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_CULL_CCW;

            float dp[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
            bgfx::setUniform(u_drawParams, dp);
            bgfx::setVertexBuffer(0, active_vbh);
            bgfx::setBuffer(0, active_vbh,   bgfx::Access::Read);
            bgfx::setBuffer(1, g_matrixSSBO, bgfx::Access::Read);
            bgfx::setIndexBuffer(active_ibh);
            bgfx::setState(kState);
            bgfx::setInstanceCount(s_entity_count);
            bgfx::submit(0, g_default_program);

            if (!g_logged_first_draw) {
                char buf[128];
                std::snprintf(buf, sizeof(buf),
                    "[Renderer] First draw: %u visible entities -> SSBO instanced", s_entity_count);
                EngineLog(buf);
                g_logged_first_draw = true;
            }
        };
        ecs_system_init(world, &s);
    }

    EngineLog("[Renderer] Init complete");
}

void Shutdown() {
    EngineLog("[Renderer] Shutting down...");
    if (bgfx::isValid(g_default_program)) bgfx::destroy(g_default_program);
    if (bgfx::isValid(g_matrixSSBO))      bgfx::destroy(g_matrixSSBO);
    if (bgfx::isValid(u_drawParams))      bgfx::destroy(u_drawParams);
    if (g_camera_query) ecs_query_fini(g_camera_query);
    if (g_render_query) ecs_query_fini(g_render_query);
    EngineLog("[Renderer] Shutdown complete");
}

} // namespace Renderer
} // namespace Engine
