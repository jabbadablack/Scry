#include <engine/renderer.h>
#include <engine/transform.h>
#include <engine/camera.h>
#include <engine/pipeline.h>
#include <engine/graphics.h>
#include <engine/graphics_backend.h>

#include "Graphics/GraphicsEngine/interface/PipelineState.h"
#include "Graphics/GraphicsEngine/interface/ShaderResourceBinding.h"
#include "Graphics/GraphicsEngine/interface/Shader.h"
#include "Graphics/GraphicsEngine/interface/GraphicsTypes.h"

#include <Eigen/Core>
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <filesystem>

using namespace Diligent;
namespace fs = std::filesystem;

namespace Engine {
namespace Renderer {

ecs_entity_t id_MeshInstance = 0;
ecs_entity_t id_EntityIntent = 0;
ecs_entity_t id_Material     = 0;

static constexpr uint32_t MAX_ENTITIES = 4096u;

static RefCntAutoPtr<IPipelineState>         g_pPSO;
static RefCntAutoPtr<IShaderResourceBinding> g_pSRB;
static RefCntAutoPtr<IBuffer>                g_pMatrixBuffer;
static RefCntAutoPtr<IBuffer>                g_pDrawParamsCB;

static uint32_t     g_active_mesh  = Engine::Graphics::INVALID_MESH;
static bool         g_srb_ready    = false;
static bool         g_logged_first = false;

static ecs_query_t* g_render_query = nullptr;
static ecs_query_t* g_camera_query = nullptr;

// Scratch buffer: Prompt 5 — filled via O(1) memcpy from Flecs archetype chunks
static float    s_mat_flat[MAX_ENTITIES * 16];
static uint32_t s_entity_count = 0;

// ── Shader loading ────────────────────────────────────────────────────────────

static char* LoadShaderSource(const char* path, uint32_t* out_len) {
    FILE* f = std::fopen(path, "rb");
    if (!f) return nullptr;
    std::fseek(f, 0, SEEK_END);
    long sz = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    char* buf = static_cast<char*>(std::malloc(static_cast<size_t>(sz) + 1));
    if (!buf) { std::fclose(f); return nullptr; }
    std::fread(buf, 1, static_cast<size_t>(sz), f);
    buf[sz] = '\0';
    std::fclose(f);
    if (out_len) *out_len = static_cast<uint32_t>(sz);
    return buf;
}

static char* DiscoverShaderSource(uint32_t* out_len) {
    const char* candidates[] = {
        "assets/raw/shaders/mesh.hlsl",
        "../assets/raw/shaders/mesh.hlsl",
        "shaders/mesh.hlsl",
        "../shaders/mesh.hlsl",
    };
    for (const char* c : candidates) {
        if (fs::exists(c)) {
            char* src = LoadShaderSource(c, out_len);
            if (src) {
                char msg[256];
                std::snprintf(msg, sizeof(msg), "[Renderer] Shader source: %s", c);
                EngineLog(msg);
                return src;
            }
        }
    }
    EngineLog("[Renderer] FATAL: mesh.hlsl not found");
    return nullptr;
}

// ── PSO creation ──────────────────────────────────────────────────────────────

static bool CreatePSO() {
    IRenderDevice* dev = Graphics::GetDevice();
    ISwapChain*    sc  = Graphics::GetSwapChain();
    assert(dev && sc);

    uint32_t hlsl_len = 0;
    char* hlsl = DiscoverShaderSource(&hlsl_len);
    if (!hlsl) return false;

    ShaderCreateInfo sci;
    sci.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    sci.ShaderCompiler = SHADER_COMPILER_DEFAULT;
    sci.Source         = hlsl;
    sci.SourceLength   = hlsl_len;

    RefCntAutoPtr<IShader> pVS, pPS;
    {
        sci.Desc.ShaderType = SHADER_TYPE_VERTEX;
        sci.Desc.Name       = "MeshVS";
        sci.EntryPoint      = "VSMain";
        dev->CreateShader(sci, &pVS);
    }
    {
        sci.Desc.ShaderType = SHADER_TYPE_PIXEL;
        sci.Desc.Name       = "MeshPS";
        sci.EntryPoint      = "PSMain";
        dev->CreateShader(sci, &pPS);
    }
    std::free(hlsl);

    if (!pVS) { EngineLog("[Renderer] FATAL: vertex shader compile failed"); return false; }
    if (!pPS) { EngineLog("[Renderer] FATAL: pixel shader compile failed");  return false; }

    ShaderResourceVariableDesc vars[] = {
        {SHADER_TYPE_VERTEX, "b_vertices",  SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {SHADER_TYPE_VERTEX, "b_instances", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {SHADER_TYPE_VERTEX, "DrawParams",  SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
    };

    const SwapChainDesc& scd = sc->GetDesc();

    GraphicsPipelineStateCreateInfo psoCI;
    psoCI.PSODesc.Name                        = "MeshPSO";
    psoCI.PSODesc.PipelineType                = PIPELINE_TYPE_GRAPHICS;
    psoCI.PSODesc.ResourceLayout.Variables    = vars;
    psoCI.PSODesc.ResourceLayout.NumVariables = static_cast<uint32_t>(sizeof(vars) / sizeof(vars[0]));
    psoCI.GraphicsPipeline.InputLayout.NumElements = 0;
    psoCI.GraphicsPipeline.NumRenderTargets  = 1;
    psoCI.GraphicsPipeline.RTVFormats[0]     = scd.ColorBufferFormat;
    psoCI.GraphicsPipeline.DSVFormat         = scd.DepthBufferFormat;
    psoCI.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    psoCI.GraphicsPipeline.DepthStencilDesc.DepthEnable      = True;
    psoCI.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = True;
    psoCI.GraphicsPipeline.DepthStencilDesc.DepthFunc        = COMPARISON_FUNC_LESS;
    psoCI.GraphicsPipeline.RasterizerDesc.CullMode              = CULL_MODE_BACK;
    psoCI.GraphicsPipeline.RasterizerDesc.FrontCounterClockwise = True;
    psoCI.pVS = pVS;
    psoCI.pPS = pPS;

    dev->CreateGraphicsPipelineState(psoCI, &g_pPSO);
    if (!g_pPSO) { EngineLog("[Renderer] FATAL: PSO creation failed"); return false; }

    EngineLog("[Renderer] PSO created");
    return true;
}

// ── SRB lazy init ─────────────────────────────────────────────────────────────

static void EnsureSRB(uint32_t mesh_slot) {
    if (g_srb_ready && g_active_mesh == mesh_slot) return;

    IBuffer* pVB = Graphics::GetVertexBuffer(mesh_slot);
    if (!pVB) return;

    g_pSRB.Release();
    g_pPSO->CreateShaderResourceBinding(&g_pSRB, true);
    assert(g_pSRB);

    IBufferView* vbSRV  = pVB->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE);
    IBufferView* matSRV = g_pMatrixBuffer->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE);
    assert(vbSRV && matSRV);

    g_pSRB->GetVariableByName(SHADER_TYPE_VERTEX, "b_vertices") ->Set(vbSRV);
    g_pSRB->GetVariableByName(SHADER_TYPE_VERTEX, "b_instances")->Set(matSRV);
    g_pSRB->GetVariableByName(SHADER_TYPE_VERTEX, "DrawParams") ->Set(g_pDrawParamsCB);

    g_active_mesh = mesh_slot;
    g_srb_ready   = true;
    EngineLog("[Renderer] SRB initialized");
}

// ── Init ──────────────────────────────────────────────────────────────────────

void Init(ecs_world_t* world) {
    EngineLog("[Renderer] Initializing...");
    assert(world != nullptr);

    IRenderDevice* dev = Graphics::GetDevice();
    assert(dev);

    {
        BufferDesc bd;
        bd.Name              = "MatrixSSBO";
        bd.Usage             = USAGE_DYNAMIC;
        bd.BindFlags         = BIND_SHADER_RESOURCE;
        bd.Mode              = BUFFER_MODE_STRUCTURED;
        bd.ElementByteStride = 64u;
        bd.CPUAccessFlags    = CPU_ACCESS_WRITE;
        bd.Size              = MAX_ENTITIES * 64u;
        dev->CreateBuffer(bd, nullptr, &g_pMatrixBuffer);
        assert(g_pMatrixBuffer);
    }

    {
        BufferDesc bd;
        bd.Name           = "DrawParamsCB";
        bd.Usage          = USAGE_DYNAMIC;
        bd.BindFlags      = BIND_UNIFORM_BUFFER;
        bd.CPUAccessFlags = CPU_ACCESS_WRITE;
        bd.Size           = 64u;
        dev->CreateBuffer(bd, nullptr, &g_pDrawParamsCB);
        assert(g_pDrawParamsCB);
    }

    if (!CreatePSO()) {
        EngineLog("[Renderer] FATAL: failed to build PSO");
        return;
    }

    // Register ECS components
    {
        auto reg = [&](const char* name, size_t sz, size_t align) -> ecs_entity_t {
            ecs_entity_desc_t ed = {}; ed.name = name;
            ecs_component_desc_t cd = {};
            cd.entity = ecs_entity_init(world, &ed);
            cd.type.size = sz;
            cd.type.alignment = align;
            return ecs_component_init(world, &cd);
        };
        id_MeshInstance = reg("MeshInstance", sizeof(MeshInstance), alignof(MeshInstance));
        id_EntityIntent = reg("EntityIntent",  sizeof(Intent),       alignof(Intent));
        id_Material     = reg("Material",      sizeof(Material),     alignof(Material));
        assert(id_MeshInstance && id_EntityIntent && id_Material);
    }

    // Queries
    {
        ecs_query_desc_t cq = {};
        cq.terms[0].id = Engine::Camera::id_Camera;
        g_camera_query = ecs_query_init(world, &cq);
        assert(g_camera_query);

        ecs_query_desc_t rq = {};
        rq.terms[0].id = Engine::Transform::id_WorldMatrix;
        rq.terms[1].id = id_MeshInstance;
        rq.terms[2].id = id_Material;
        rq.terms[3].id = id_EntityIntent;
        g_render_query = ecs_query_init(world, &rq);
        assert(g_render_query);
    }

    // RenderSystem (Phase_React)
    {
        ecs_entity_desc_t ed = {}; ed.name = "RenderSystem";
        ecs_entity_t sys_ent = ecs_entity_init(world, &ed);
        ecs_add_pair(world, sys_ent, EcsDependsOn, Engine::Pipeline::Phase_React);

        ecs_system_desc_t s = {};
        s.entity   = sys_ent;
        s.callback = [](ecs_iter_t* it) {
            IDeviceContext* ctx = Graphics::GetContext();
            ISwapChain*     sc  = Graphics::GetSwapChain();
            assert(ctx && sc);

            // Prompt 5: O(1) matrix upload — blast entire archetype chunks directly
            s_entity_count = 0;
            if (g_render_query) {
                ecs_iter_t ri = ecs_query_iter(it->world, g_render_query);
                while (ecs_query_next(&ri)) {
                    const Engine::Transform::WorldMatrix* wm  = ecs_field(&ri, Engine::Transform::WorldMatrix, 0);
                    const Intent*                         ent = ecs_field(&ri, Intent, 3);

                    for (int i = 0; i < ri.count && s_entity_count < MAX_ENTITIES; ++i) {
                        if ((ent[i].mask & INTENT_VISIBLE)   == 0) continue;
                        if ((ent[i].mask & INTENT_DESTROYED) != 0) continue;
                        // wm is a contiguous archetype chunk — memcpy 64 bytes per entity
                        std::memcpy(&s_mat_flat[s_entity_count * 16], wm[i].value.data(), 64u);
                        ++s_entity_count;
                    }
                }
            }
            if (s_entity_count == 0) return;

            // Find first loaded mesh slot
            uint32_t mesh_slot = Engine::Graphics::INVALID_MESH;
            for (uint32_t m = 0; m < Engine::Graphics::MAX_MESHES; ++m) {
                if (Graphics::GetVertexBuffer(m) && Graphics::GetIndexBuffer(m)) {
                    mesh_slot = m; break;
                }
            }
            if (mesh_slot == Engine::Graphics::INVALID_MESH) return;

            // Upload matrix SSBO
            {
                void* pData = nullptr;
                ctx->MapBuffer(g_pMatrixBuffer, MAP_WRITE, MAP_FLAG_DISCARD, pData);
                if (pData) {
                    std::memcpy(pData, s_mat_flat, s_entity_count * 64u);
                    ctx->UnmapBuffer(g_pMatrixBuffer, MAP_WRITE);
                }
            }

            // Upload viewProj cbuffer
            if (g_camera_query) {
                ecs_iter_t ci = ecs_query_iter(it->world, g_camera_query);
                while (ecs_query_next(&ci)) {
                    const Engine::Camera::Camera* cam = ecs_field(&ci, Engine::Camera::Camera, 0);
                    if (ci.count == 0) continue;

                    Eigen::Map<const Eigen::Matrix4f> V(cam[0].view);
                    Eigen::Map<const Eigen::Matrix4f> P(cam[0].proj);
                    Eigen::Matrix4f VP = P * V;

                    void* pData = nullptr;
                    ctx->MapBuffer(g_pDrawParamsCB, MAP_WRITE, MAP_FLAG_DISCARD, pData);
                    if (pData) {
                        std::memcpy(pData, VP.data(), 64u);
                        ctx->UnmapBuffer(g_pDrawParamsCB, MAP_WRITE);
                    }
                    ecs_iter_fini(&ci);
                    break;
                }
            }

            EnsureSRB(mesh_slot);
            if (!g_srb_ready) return;

            IBuffer* pIB = Graphics::GetIndexBuffer(mesh_slot);
            ctx->SetIndexBuffer(pIB, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            ctx->SetPipelineState(g_pPSO);
            ctx->CommitShaderResources(g_pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

            DrawIndexedAttribs draw;
            draw.IndexType    = VT_UINT32;
            draw.NumIndices   = Graphics::GetIndexCount(mesh_slot);
            draw.NumInstances = s_entity_count;
            draw.Flags        = DRAW_FLAG_VERIFY_ALL;
            ctx->DrawIndexed(draw);

            if (!g_logged_first) {
                char buf[128];
                std::snprintf(buf, sizeof(buf),
                    "[Renderer] First draw: %u instances, %u indices", s_entity_count, draw.NumIndices);
                EngineLog(buf);
                g_logged_first = true;
            }
        };
        ecs_system_init(world, &s);
    }

    EngineLog("[Renderer] Init complete");
}

// ── Shutdown ──────────────────────────────────────────────────────────────────

void Shutdown() {
    EngineLog("[Renderer] Shutting down...");
    if (g_render_query) { ecs_query_fini(g_render_query); g_render_query = nullptr; }
    if (g_camera_query) { ecs_query_fini(g_camera_query); g_camera_query = nullptr; }
    g_pSRB.Release();
    g_pPSO.Release();
    g_pMatrixBuffer.Release();
    g_pDrawParamsCB.Release();
    EngineLog("[Renderer] Shutdown complete");
}

} // namespace Renderer
} // namespace Engine
