#include <engine/renderer.hpp>
#include <engine/transform.hpp>
#include <engine/camera.hpp>
#include <engine/pipeline.hpp>
#include <engine/math.hpp>

#include <bgfx/bgfx.h>
#include <libassert/assert.hpp>
#include <cstring>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <vector>

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

static bgfx::ProgramHandle g_program = BGFX_INVALID_HANDLE;
static ecs_query_t* g_camera_query = nullptr;

static const bgfx::Memory* LoadShaderFile(const char* filepath) {
    FILE* file = std::fopen(filepath, "rb");
    if (!file) {
        std::fprintf(stderr, "[Renderer] FATAL: Could not open shader file %s\n", filepath);
        return nullptr;
    }
    std::fseek(file, 0, SEEK_END);
    long size = std::ftell(file);
    if (size <= 0) {
        std::fclose(file);
        return nullptr;
    }
    std::fseek(file, 0, SEEK_SET);

    const bgfx::Memory* mem = bgfx::alloc(static_cast<uint32_t>(size));
    size_t read_bytes = std::fread(mem->data, 1, static_cast<size_t>(size), file);
    (void)read_bytes;
    std::fclose(file);
    return mem;
}

static uint32_t DiscoverAndLoadShader(const char* filename, bgfx::ShaderHandle& out_handle) {
    std::string paths[] = {
        std::string("assets/cooked/shaders/") + filename,
        std::string("../assets/cooked/shaders/") + filename,
        std::string("../../assets/cooked/shaders/") + filename,
        std::string("bin/assets/cooked/shaders/") + filename,
        std::string("../bin/assets/cooked/shaders/") + filename,
        std::string("../../bin/assets/cooked/shaders/") + filename,
        std::string("build/bin/assets/cooked/shaders/") + filename,
        std::string("../build/bin/assets/cooked/shaders/") + filename
    };

    for (const auto& path : paths) {
        if (fs::exists(path)) {
            const bgfx::Memory* mem = LoadShaderFile(path.c_str());
            if (mem) {
                out_handle = bgfx::createShader(mem);
                return 1;
            }
        }
    }

    std::fprintf(stderr, "[Renderer] FATAL: Could not find shader %s in any candidate path.\n", filename);
    return 0;
}

void Init(ecs_world_t* world) {
    DEBUG_ASSERT(world != nullptr);

    // ── Load Shaders ─────────────────────────────────────────────────────────
    bgfx::ShaderHandle vsh = BGFX_INVALID_HANDLE;
    bgfx::ShaderHandle fsh = BGFX_INVALID_HANDLE;
    
    DiscoverAndLoadShader("vs_mesh.bin", vsh);
    DiscoverAndLoadShader("fs_mesh.bin", fsh);

    if (bgfx::isValid(vsh) && bgfx::isValid(fsh)) {
        g_program = bgfx::createProgram(vsh, fsh, true); // true = destroy shaders after program creation
    }

    // ── Register Components ──────────────────────────────────────────────────
    {
        ecs_entity_desc_t e = {};
        e.name = "MeshInstance";
        ecs_entity_t mi_ent = ecs_entity_init(world, &e);
        ecs_component_desc_t mi_c = {};
        mi_c.entity = mi_ent;
        mi_c.type.size = sizeof(MeshInstance);
        mi_c.type.alignment = alignof(MeshInstance);
        id_MeshInstance = ecs_component_init(world, &mi_c);

        e.name = "EntityIntent";
        ecs_entity_t intent_ent = ecs_entity_init(world, &e);
        ecs_component_desc_t intent_c = {};
        intent_c.entity = intent_ent;
        intent_c.type.size = sizeof(Intent);
        intent_c.type.alignment = alignof(Intent);
        id_EntityIntent = ecs_component_init(world, &intent_c);
    }

    // ── Initialize camera query ──────────────────────────────────────────────
    {
        ecs_query_desc_t qdesc = {};
        qdesc.terms[0].id = Engine::Camera::id_Camera;
        g_camera_query = ecs_query_init(world, &qdesc);
    }

    // ── Render System (Phase_React) ──────────────────────────────────────────
    {
        ecs_entity_desc_t e = {};
        e.name = "RenderSystem";
        ecs_entity_t sys_ent = ecs_entity_init(world, &e);
        ecs_add_pair(world, sys_ent, EcsDependsOn, Engine::Pipeline::Phase_React);

        ecs_system_desc_t s = {};
        s.entity = sys_ent;
        s.query.terms[0].id = Engine::Transform::id_WorldMatrix;
        s.query.terms[0].inout = EcsIn;
        s.query.terms[1].id = Engine::Renderer::id_MeshInstance;
        s.query.terms[1].inout = EcsIn;
        s.query.terms[2].id = Engine::Renderer::id_EntityIntent;
        s.query.terms[2].inout = EcsIn;
        
        s.callback = [](ecs_iter_t* it) {
            const Engine::Transform::WorldMatrix* wm = ecs_field(it, Engine::Transform::WorldMatrix, 0);
            const Engine::Renderer::MeshInstance* mi = ecs_field(it, Engine::Renderer::MeshInstance, 1);
            const Engine::Renderer::Intent* intent = ecs_field(it, Engine::Renderer::Intent, 2);
            
            // Get camera for view_proj
            Math::ScryMat4 view_proj = Math::ScryMat4::Identity();
            if (g_camera_query) {
                ecs_iter_t cam_it = ecs_query_iter(it->world, g_camera_query);
                if (ecs_query_next(&cam_it)) {
                    const Engine::Camera::Camera* cam = ecs_field(&cam_it, Engine::Camera::Camera, 0);
                    view_proj = cam[0].view_proj;
                }
            }

            float view_mtx[16] = {
                1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 1.0f
            };
            bgfx::setViewTransform(0, view_mtx, view_proj.data());

            // Count visible
            uint32_t visible_count = 0;
            for (int i = 0; i < it->count; ++i) {
                if ((intent[i].mask & INTENT_VISIBLE) && !(intent[i].mask & INTENT_DESTROYED)) {
                    visible_count++;
                }
            }

            if (visible_count == 0) return;

            // BGFX allocInstanceDataBuffer:
            bgfx::InstanceDataBuffer idb;
            const uint16_t stride = 64; // sizeof(Math::ScryMat4)
            if (visible_count <= bgfx::getAvailInstanceDataBuffer(visible_count, stride)) {
                bgfx::allocInstanceDataBuffer(&idb, visible_count, stride);

                uint8_t* data = idb.data;
                for (int i = 0; i < it->count; ++i) {
                    if ((intent[i].mask & INTENT_VISIBLE) && !(intent[i].mask & INTENT_DESTROYED)) {
                        std::memcpy(data, wm[i].value.data(), stride);
                        data += stride;
                    }
                }

                // Assume all use the same mesh for this batch
                uint32_t current_mesh = mi[0].mesh_id;
                bgfx::VertexBufferHandle vbh = Graphics::GetVertexBuffer(current_mesh);
                bgfx::IndexBufferHandle  ibh = Graphics::GetIndexBuffer(current_mesh);

                if (bgfx::isValid(vbh) && bgfx::isValid(ibh) && bgfx::isValid(g_program)) {
                    bgfx::setVertexBuffer(0, vbh);
                    bgfx::setIndexBuffer(ibh);
                    bgfx::setInstanceDataBuffer(&idb);
                    bgfx::setState(BGFX_STATE_DEFAULT | BGFX_STATE_DEPTH_TEST_LESS);
                    bgfx::submit(0, g_program);
                }
            }
        };
        ecs_system_init(world, &s);
    }
}

void Shutdown() {
    if (bgfx::isValid(g_program)) {
        bgfx::destroy(g_program);
        g_program = BGFX_INVALID_HANDLE;
    }
    if (g_camera_query) {
        ecs_query_fini(g_camera_query);
        g_camera_query = nullptr;
    }
}

} // namespace Renderer
} // namespace Engine
