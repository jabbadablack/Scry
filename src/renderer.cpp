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

// Instance record: mat4 (64 B) + RGBA color (16 B) = 80 B = 5 × vec4
struct alignas(16) InstanceRecord {
    float mat[16];
    float color[4];
};
static_assert(sizeof(InstanceRecord) == 80u, "stride must be 80");

// ── Programs ─────────────────────────────────────────────────────────────────
static bgfx::ProgramHandle g_default_program = BGFX_INVALID_HANDLE;
static bgfx::ProgramHandle g_compute_program  = BGFX_INVALID_HANDLE;

// ── SSBO handles (single-threaded BGFX, zero-allocation per frame) ─────────
static bgfx::DynamicVertexBufferHandle g_matrixSSBO  = BGFX_INVALID_HANDLE;
static bgfx::DynamicVertexBufferHandle g_intentSSBO  = BGFX_INVALID_HANDLE;
static bgfx::IndirectBufferHandle      g_indirectCmd = BGFX_INVALID_HANDLE;
static bgfx::VertexLayout              g_vec4_layout;
static bgfx::VertexLayout              g_uint_layout;

// ── Uniforms ─────────────────────────────────────────────────────────────────
static bgfx::UniformHandle g_u_drawParams  = BGFX_INVALID_HANDLE;
static bgfx::UniformHandle g_u_meshParams  = BGFX_INVALID_HANDLE;

// ── Queries ──────────────────────────────────────────────────────────────────
static ecs_query_t* g_render_query = nullptr;
static ecs_query_t* g_camera_query = nullptr;

// ── Per-frame scratch (static = zero heap allocation per frame) ────────────
static Math::ScryMat4 s_entity_mat   [MAX_ENTITIES];
static float          s_entity_color [MAX_ENTITIES][4];
static uint32_t       s_entity_meshId[MAX_ENTITIES];
static uint32_t       s_entity_intent[MAX_ENTITIES];
static uint32_t       s_entity_count;

// Counting-sort workspace
static uint32_t s_mesh_count [MAX_MESHES];
static uint32_t s_mesh_base  [MAX_MESHES];
static uint32_t s_mesh_cursor[MAX_MESHES];

// Sorted instance records (by mesh id), uploaded as flat SSBO each frame
static InstanceRecord s_sorted[MAX_ENTITIES];

static bool g_logged_first_draw = false;

// ── Shader discovery ─────────────────────────────────────────────────────────

static const bgfx::Memory* LoadShaderFile(const char* path) {
    FILE* f = std::fopen(path, "rb");
    if (!f) return nullptr;
    std::fseek(f, 0, SEEK_END);
    long sz = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    const bgfx::Memory* mem = bgfx::alloc(static_cast<uint32_t>(sz));
    std::fread(mem->data, 1, static_cast<size_t>(sz), f);
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

// ── Init ─────────────────────────────────────────────────────────────────────

void Init(ecs_world_t* world) {
    EngineLog("[Renderer] Initializing...");
    DEBUG_ASSERT(world != nullptr);

    // ── Load draw program ─────────────────────────────────────────────────
    bgfx::ShaderHandle vsh = BGFX_INVALID_HANDLE;
    bgfx::ShaderHandle fsh = BGFX_INVALID_HANDLE;
    DEBUG_ASSERT(DiscoverShader("vs_mesh.bin", vsh));
    DEBUG_ASSERT(DiscoverShader("fs_mesh.bin", fsh));
    g_default_program = bgfx::createProgram(vsh, fsh, true);
    DEBUG_ASSERT(bgfx::isValid(g_default_program));

    // ── Load compute program (cs_indirect) ───────────────────────────────
    bgfx::ShaderHandle csh = BGFX_INVALID_HANDLE;
    if (DiscoverShader("cs_indirect.bin", csh)) {
        g_compute_program = bgfx::createProgram(csh, true);
        EngineLog("[Renderer] cs_indirect loaded");
    }

    // ── SSBO layouts ─────────────────────────────────────────────────────
    g_vec4_layout.begin()
        .add(bgfx::Attrib::TexCoord0, 4, bgfx::AttribType::Float)
        .end();  // stride = 16 bytes

    g_uint_layout.begin()
        .add(bgfx::Attrib::TexCoord0, 1, bgfx::AttribType::Float)
        .end();  // stride = 4 bytes (reinterpreted as uint in shader)

    // Instance SSBO: MAX_ENTITIES × 5 vec4 elements (mat4 + color per instance)
    g_matrixSSBO = bgfx::createDynamicVertexBuffer(
        MAX_ENTITIES * 5u, g_vec4_layout,
        BGFX_BUFFER_COMPUTE_READ | BGFX_BUFFER_ALLOW_RESIZE);
    DEBUG_ASSERT(bgfx::isValid(g_matrixSSBO));

    // Intent SSBO: MAX_ENTITIES uint32 flags for future GPU culling pass
    g_intentSSBO = bgfx::createDynamicVertexBuffer(
        MAX_ENTITIES, g_uint_layout,
        BGFX_BUFFER_COMPUTE_READ | BGFX_BUFFER_ALLOW_RESIZE);
    DEBUG_ASSERT(bgfx::isValid(g_intentSSBO));

    // Indirect draw buffer: one 5-uint slot per mesh (GPU culling future pass)
    g_indirectCmd = bgfx::createIndirectBuffer(MAX_MESHES);
    DEBUG_ASSERT(bgfx::isValid(g_indirectCmd));

    // Uniforms
    g_u_drawParams = bgfx::createUniform("u_drawParams",  bgfx::UniformType::Vec4);
    g_u_meshParams = bgfx::createUniform("u_meshParams",  bgfx::UniformType::Vec4);
    DEBUG_ASSERT(bgfx::isValid(g_u_drawParams));
    DEBUG_ASSERT(bgfx::isValid(g_u_meshParams));

    // ── Register Components ───────────────────────────────────────────────
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

    // ── Queries ──────────────────────────────────────────────────────────
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
        s.entity = sys_ent;
        s.callback = [](ecs_iter_t* it) {
            DEBUG_ASSERT(it != nullptr);

            // ── View / camera ─────────────────────────────────────────────
            const bgfx::Stats* stats = bgfx::getStats();
            bgfx::setViewRect(0, 0, 0, (uint16_t)stats->width, (uint16_t)stats->height);
            bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x111111FF, 1.0f, 0);

            if (g_camera_query) {
                ecs_iter_t ci = ecs_query_iter(it->world, g_camera_query);
                while (ecs_query_next(&ci)) {
                    const Engine::Camera::Camera* cam = ecs_field(&ci, Engine::Camera::Camera, 0);
                    bgfx::setViewTransform(0, cam[0].view, cam[0].proj);
                }
            }

            // ── Phase 1: Collect entities ─────────────────────────────────
            s_entity_count = 0;
            if (g_render_query) {
                ecs_iter_t ri = ecs_query_iter(it->world, g_render_query);
                while (ecs_query_next(&ri)) {
                    const Engine::Transform::WorldMatrix* wm  = ecs_field(&ri, Engine::Transform::WorldMatrix, 0);
                    const MeshInstance*                   mi  = ecs_field(&ri, MeshInstance, 1);
                    const Material*                       mat = ecs_field(&ri, Material, 2);
                    const Intent*                         ent = ecs_field(&ri, Intent, 3);

                    for (int i = 0; i < ri.count && s_entity_count < MAX_ENTITIES; ++i) {
                        s_entity_mat   [s_entity_count] = wm[i].value;
                        s_entity_meshId[s_entity_count] = mi[i].mesh_id;
                        s_entity_intent[s_entity_count] = ent[i].mask;
                        std::memcpy(s_entity_color[s_entity_count], mat[i].base_color, 4 * sizeof(float));
                        ++s_entity_count;
                    }
                }
            }

            // ── Phase 2: Count visible instances per mesh ─────────────────
            std::memset(s_mesh_count, 0, sizeof(s_mesh_count));
            for (uint32_t i = 0; i < s_entity_count; ++i) {
                uint32_t m = s_entity_intent[i];
                if ((m & INTENT_VISIBLE) && !(m & INTENT_DESTROYED)) {
                    uint32_t mid = s_entity_meshId[i];
                    if (mid < MAX_MESHES) ++s_mesh_count[mid];
                }
            }

            // ── Phase 3: Prefix sum → base instance offset per mesh ───────
            s_mesh_base[0] = 0;
            for (uint32_t m = 1; m < MAX_MESHES; ++m)
                s_mesh_base[m] = s_mesh_base[m-1] + s_mesh_count[m-1];

            uint32_t total = s_mesh_base[MAX_MESHES-1] + s_mesh_count[MAX_MESHES-1];
            if (total == 0) return;

            // ── Phase 4: Scatter — build sorted InstanceRecord array ──────
            std::memset(s_mesh_cursor, 0, sizeof(s_mesh_cursor));
            for (uint32_t i = 0; i < s_entity_count; ++i) {
                uint32_t m = s_entity_intent[i];
                if (!((m & INTENT_VISIBLE) && !(m & INTENT_DESTROYED))) continue;
                uint32_t mid = s_entity_meshId[i];
                if (mid >= MAX_MESHES) continue;

                uint32_t dst = s_mesh_base[mid] + s_mesh_cursor[mid]++;
                InstanceRecord& rec = s_sorted[dst];
                std::memcpy(rec.mat,   s_entity_mat[i].data(), 64);
                std::memcpy(rec.color, s_entity_color[i],      16);
            }

            // ── Phase 5a: Upload sorted instances + intents to SSBOs ──────
            bgfx::update(g_matrixSSBO, 0,
                bgfx::makeRef(s_sorted,        total         * sizeof(InstanceRecord)));
            bgfx::update(g_intentSSBO, 0,
                bgfx::makeRef(s_entity_intent, s_entity_count * sizeof(uint32_t)));

            // ── Phase 5b: Draw — SSBO vertex pulling, zero alloc ──────────
            constexpr uint64_t kState =
                BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z |
                BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_CULL_CW | BGFX_STATE_MSAA;

            for (uint32_t meshId = 0; meshId < MAX_MESHES; ++meshId) {
                uint32_t count = s_mesh_count[meshId];
                if (count == 0) continue;

                bgfx::VertexBufferHandle vbh = Graphics::GetVertexBuffer(meshId);
                bgfx::IndexBufferHandle  ibh = Graphics::GetIndexBuffer(meshId);
                if (!bgfx::isValid(vbh) || !bgfx::isValid(ibh)) continue;

                // Instance base offset in g_matrixSSBO for this mesh
                float dp[4] = { float(s_mesh_base[meshId]), 0.0f, 0.0f, 0.0f };
                bgfx::setUniform(g_u_drawParams, dp);

                // Bind vertex data SSBO (VBH created with BGFX_BUFFER_COMPUTE_READ)
                bgfx::setBuffer(0, vbh,          bgfx::Access::Read);
                // Bind instance data SSBO (sorted mat4+color, 5 vec4 per instance)
                bgfx::setBuffer(1, g_matrixSSBO, bgfx::Access::Read);
                bgfx::setIndexBuffer(ibh);
                bgfx::setInstanceCount(count);
                bgfx::setState(kState);
                bgfx::submit(0, g_default_program);
            }

            if (!g_logged_first_draw) {
                char buf[128];
                std::snprintf(buf, sizeof(buf),
                    "[Renderer] First draw: %u visible instances (SSBO path)",
                    total);
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
    if (bgfx::isValid(g_compute_program)) bgfx::destroy(g_compute_program);
    if (bgfx::isValid(g_matrixSSBO))      bgfx::destroy(g_matrixSSBO);
    if (bgfx::isValid(g_intentSSBO))      bgfx::destroy(g_intentSSBO);
    if (bgfx::isValid(g_indirectCmd))     bgfx::destroy(g_indirectCmd);
    if (bgfx::isValid(g_u_drawParams))    bgfx::destroy(g_u_drawParams);
    if (bgfx::isValid(g_u_meshParams))    bgfx::destroy(g_u_meshParams);
    if (g_camera_query) ecs_query_fini(g_camera_query);
    if (g_render_query) ecs_query_fini(g_render_query);
    EngineLog("[Renderer] Shutdown complete");
}

} // namespace Renderer
} // namespace Engine
