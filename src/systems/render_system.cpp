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
#include <cmath>
#include <filesystem>

using namespace Diligent;
namespace fs = std::filesystem;

namespace Engine {
namespace Renderer {

// ── ECS component IDs ────────────────────────────────────────────────────────
ecs_entity_t id_MeshData     = 0;
ecs_entity_t id_AABB         = 0;
ecs_entity_t id_EntityIntent = 0;
ecs_entity_t id_Material     = 0;

// ── Capacity ─────────────────────────────────────────────────────────────────
static constexpr uint32_t MAX_ENTITIES = 16384u;

// ── Graphics PSO / SRB ───────────────────────────────────────────────────────
static RefCntAutoPtr<IPipelineState>         g_pPSO;
static RefCntAutoPtr<IShaderResourceBinding> g_pSRB;
static RefCntAutoPtr<IBuffer>                g_pMatrixBuffer;
static RefCntAutoPtr<IBuffer>                g_pDrawParamsCB;

// ── Compute PSO / SRB ────────────────────────────────────────────────────────
static RefCntAutoPtr<IPipelineState>         g_pCullPSO;
static RefCntAutoPtr<IShaderResourceBinding> g_pCullSRB;

// ── MDI + cull resources ─────────────────────────────────────────────────────
static RefCntAutoPtr<IBuffer> g_IndirectArgsBuffer;    // 20 bytes, 1 draw cmd
static RefCntAutoPtr<IBuffer> g_pAABBBuffer;           // dynamic SSBO, MAX_ENTITIES * 32
static RefCntAutoPtr<IBuffer> g_pCullParamsCB;         // 128 bytes cbuffer
static RefCntAutoPtr<IBuffer> g_pVisibleInstancesBuffer; // UAV(compute) + SRV(vertex), MAX_ENTITIES * 4

// ── ECS queries ──────────────────────────────────────────────────────────────
static ecs_query_t* g_render_query = nullptr;
static ecs_query_t* g_camera_query = nullptr;

// ── Per-frame scratch ────────────────────────────────────────────────────────
struct alignas(16) ShaderAABB {
    float aabb_min[3]; float pad0;
    float aabb_max[3]; float pad1;
};

static float      s_mat_flat[MAX_ENTITIES * 16];
static ShaderAABB s_aabb_flat[MAX_ENTITIES];
static uint32_t   s_entity_count = 0;

// CPU-side mirror of the HLSL IndirectCommand struct
struct IndirectCmd {
    uint32_t indexCount;
    uint32_t instanceCount;
    uint32_t firstIndex;
    uint32_t baseVertex;
    uint32_t firstInstance;
};
static_assert(sizeof(IndirectCmd) == 20, "IndirectCmd size mismatch");

// CPU-side mirror of CullParams cbuffer
struct CullParamsCPU {
    float    frustumPlanes[6][4]; // 96 bytes
    uint32_t entityCount;
    uint32_t _pad[3];
};
static_assert(sizeof(CullParamsCPU) == 112, "CullParams size mismatch");

static bool g_logged_first = false;

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

static char* DiscoverShader(const char* filename, uint32_t* out_len) {
    const char* prefixes[] = {
        "assets/raw/shaders/",
        "../assets/raw/shaders/",
        "shaders/",
        "../shaders/",
    };
    char path[512];
    for (const char* pre : prefixes) {
        std::snprintf(path, sizeof(path), "%s%s", pre, filename);
        if (fs::exists(path)) {
            char* src = LoadShaderSource(path, out_len);
            if (src) {
                char msg[256];
                std::snprintf(msg, sizeof(msg), "[Renderer] Shader: %s", path);
                EngineLog(msg);
                return src;
            }
        }
    }
    char msg[256];
    std::snprintf(msg, sizeof(msg), "[Renderer] FATAL: %s not found", filename);
    EngineLog(msg);
    return nullptr;
}

// ── Frustum plane extraction (Gribb/Hartmann, column-major VP) ───────────────

static void ExtractFrustumPlanes(const float* vp, float planes[6][4]) {
    // VP is stored column-major: vp[col*4 + row]
    // Row vectors for plane extraction (row-major interpretation of column-major):
    // row i = vp[0*4+i], vp[1*4+i], vp[2*4+i], vp[3*4+i]
    auto row = [&](int r, float* out) {
        out[0] = vp[0 * 4 + r];
        out[1] = vp[1 * 4 + r];
        out[2] = vp[2 * 4 + r];
        out[3] = vp[3 * 4 + r];
    };
    float r0[4], r1[4], r2[4], r3[4];
    row(0, r0); row(1, r1); row(2, r2); row(3, r3);

    // Left:  row3 + row0
    // Right: row3 - row0
    // Bottom:row3 + row1
    // Top:   row3 - row1
    // Near:  row3 + row2  (Vulkan NDC: z in [0,1])
    // Far:   row3 - row2
    auto combine = [](const float* a, const float* b, float s, float* out) {
        out[0] = a[0] + s * b[0];
        out[1] = a[1] + s * b[1];
        out[2] = a[2] + s * b[2];
        out[3] = a[3] + s * b[3];
    };
    combine(r3, r0,  1.f, planes[0]); // left
    combine(r3, r0, -1.f, planes[1]); // right
    combine(r3, r1,  1.f, planes[2]); // bottom
    combine(r3, r1, -1.f, planes[3]); // top
    combine(r3, r2,  1.f, planes[4]); // near
    combine(r3, r2, -1.f, planes[5]); // far

    for (int p = 0; p < 6; ++p) {
        float len = std::sqrtf(planes[p][0]*planes[p][0] +
                               planes[p][1]*planes[p][1] +
                               planes[p][2]*planes[p][2]);
        if (len > 1e-6f) {
            float inv = 1.0f / len;
            planes[p][0] *= inv; planes[p][1] *= inv;
            planes[p][2] *= inv; planes[p][3] *= inv;
        }
    }
}

// ── PSO creation ──────────────────────────────────────────────────────────────

static bool CreateGraphicsPSO() {
    IRenderDevice* dev = Graphics::GetDevice();
    ISwapChain*    sc  = Graphics::GetSwapChain();
    assert(dev && sc);

    uint32_t hlsl_len = 0;
    char* hlsl = DiscoverShader("mesh.hlsl", &hlsl_len);
    if (!hlsl) return false;

    ShaderCreateInfo sci;
    sci.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    sci.ShaderCompiler = SHADER_COMPILER_DEFAULT;
    sci.Source         = hlsl;
    sci.SourceLength   = hlsl_len;

    RefCntAutoPtr<IShader> pVS, pPS;
    { sci.Desc.ShaderType = SHADER_TYPE_VERTEX; sci.Desc.Name = "MeshVS"; sci.EntryPoint = "VSMain"; dev->CreateShader(sci, &pVS); }
    { sci.Desc.ShaderType = SHADER_TYPE_PIXEL;  sci.Desc.Name = "MeshPS"; sci.EntryPoint = "PSMain"; dev->CreateShader(sci, &pPS); }
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
    psoCI.PSODesc.ResourceLayout.NumVariables = 3u;
    psoCI.GraphicsPipeline.InputLayout.NumElements       = 0;
    psoCI.GraphicsPipeline.NumRenderTargets              = 1;
    psoCI.GraphicsPipeline.RTVFormats[0]                 = scd.ColorBufferFormat;
    psoCI.GraphicsPipeline.DSVFormat                     = scd.DepthBufferFormat;
    psoCI.GraphicsPipeline.PrimitiveTopology             = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    psoCI.GraphicsPipeline.DepthStencilDesc.DepthEnable      = True;
    psoCI.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = True;
    psoCI.GraphicsPipeline.DepthStencilDesc.DepthFunc        = COMPARISON_FUNC_LESS;
    psoCI.GraphicsPipeline.RasterizerDesc.CullMode              = CULL_MODE_BACK;
    psoCI.GraphicsPipeline.RasterizerDesc.FrontCounterClockwise = True;
    psoCI.pVS = pVS;
    psoCI.pPS = pPS;

    dev->CreateGraphicsPipelineState(psoCI, &g_pPSO);
    if (!g_pPSO) { EngineLog("[Renderer] FATAL: graphics PSO creation failed"); return false; }
    EngineLog("[Renderer] Graphics PSO created");
    return true;
}

static bool CreateComputePSO() {
    IRenderDevice* dev = Graphics::GetDevice();
    assert(dev);

    uint32_t hlsl_len = 0;
    char* hlsl = DiscoverShader("cull.hlsl", &hlsl_len);
    if (!hlsl) return false;

    ShaderCreateInfo sci;
    sci.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    sci.ShaderCompiler = SHADER_COMPILER_DEFAULT;
    sci.Source         = hlsl;
    sci.SourceLength   = hlsl_len;
    sci.Desc.ShaderType = SHADER_TYPE_COMPUTE;
    sci.Desc.Name       = "CullCS";
    sci.EntryPoint      = "CSMain";

    RefCntAutoPtr<IShader> pCS;
    dev->CreateShader(sci, &pCS);
    std::free(hlsl);

    if (!pCS) { EngineLog("[Renderer] FATAL: cull shader compile failed"); return false; }

    ShaderResourceVariableDesc cullVars[] = {
        {SHADER_TYPE_COMPUTE, "b_matrices",    SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {SHADER_TYPE_COMPUTE, "b_bounds",      SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {SHADER_TYPE_COMPUTE, "b_indirectArgs",SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {SHADER_TYPE_COMPUTE, "CullParams",    SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
    };

    ComputePipelineStateCreateInfo cpsoCI;
    cpsoCI.PSODesc.Name                        = "CullPSO";
    cpsoCI.PSODesc.PipelineType                = PIPELINE_TYPE_COMPUTE;
    cpsoCI.PSODesc.ResourceLayout.Variables    = cullVars;
    cpsoCI.PSODesc.ResourceLayout.NumVariables = 4u;
    cpsoCI.pCS = pCS;

    dev->CreateComputePipelineState(cpsoCI, &g_pCullPSO);
    if (!g_pCullPSO) { EngineLog("[Renderer] FATAL: compute PSO creation failed"); return false; }
    EngineLog("[Renderer] Compute PSO created");
    return true;
}

// ── Init ──────────────────────────────────────────────────────────────────────

void Init(ecs_world_t* world) {
    EngineLog("[Renderer] Initializing...");
    assert(world != nullptr);

    IRenderDevice* dev = Graphics::GetDevice();
    assert(dev);

    // Matrix SSBO (dynamic, written CPU-side each frame)
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

    // ViewProj constant buffer
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

    // AABB SSBO (dynamic, 32 bytes per entity)
    {
        BufferDesc bd;
        bd.Name              = "AABBBuffer";
        bd.Usage             = USAGE_DYNAMIC;
        bd.BindFlags         = BIND_SHADER_RESOURCE;
        bd.Mode              = BUFFER_MODE_STRUCTURED;
        bd.ElementByteStride = 32u; // sizeof(ShaderAABB)
        bd.CPUAccessFlags    = CPU_ACCESS_WRITE;
        bd.Size              = MAX_ENTITIES * 32u;
        dev->CreateBuffer(bd, nullptr, &g_pAABBBuffer);
        assert(g_pAABBBuffer);
    }

    // Cull params constant buffer (112 bytes, padded to 128)
    {
        BufferDesc bd;
        bd.Name           = "CullParamsCB";
        bd.Usage          = USAGE_DYNAMIC;
        bd.BindFlags      = BIND_UNIFORM_BUFFER;
        bd.CPUAccessFlags = CPU_ACCESS_WRITE;
        bd.Size           = 128u;
        dev->CreateBuffer(bd, nullptr, &g_pCullParamsCB);
        assert(g_pCullParamsCB);
    }

    // Indirect args buffer — UAV for compute write, indirect draw args for graphics
    {
        BufferDesc bd;
        bd.Name              = "IndirectArgs";
        bd.Usage             = USAGE_DEFAULT;
        bd.BindFlags         = BIND_INDIRECT_DRAW_ARGS | BIND_UNORDERED_ACCESS;
        bd.Mode              = BUFFER_MODE_STRUCTURED;
        bd.ElementByteStride = 20u; // sizeof(IndirectCmd)
        bd.Size              = 20u;
        dev->CreateBuffer(bd, nullptr, &g_IndirectArgsBuffer);
        assert(g_IndirectArgsBuffer);
    }

    // Visible instances map — compute writes entity IDs, vertex shader reads them
    {
        BufferDesc bd;
        bd.Name              = "VisibleInstances";
        bd.Usage             = USAGE_DEFAULT;
        bd.BindFlags         = BIND_UNORDERED_ACCESS | BIND_SHADER_RESOURCE;
        bd.Mode              = BUFFER_MODE_STRUCTURED;
        bd.ElementByteStride = 4u; // sizeof(uint)
        bd.Size              = MAX_ENTITIES * sizeof(Uint32);
        dev->CreateBuffer(bd, nullptr, &g_pVisibleInstancesBuffer);
        assert(g_pVisibleInstancesBuffer);
    }

    if (!CreateGraphicsPSO()) { EngineLog("[Renderer] FATAL: failed to build graphics PSO"); return; }
    if (!CreateComputePSO())  { EngineLog("[Renderer] FATAL: failed to build compute PSO");  return; }

    // Bind b_visibleInstances as STATIC on both PSOs (set once, never changes per-frame)
    {
        IBufferView* visUAV = g_pVisibleInstancesBuffer->GetDefaultView(BUFFER_VIEW_UNORDERED_ACCESS);
        IBufferView* visSRV = g_pVisibleInstancesBuffer->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE);
        assert(visUAV && visSRV);
        g_pCullPSO->GetStaticVariableByName(SHADER_TYPE_COMPUTE, "b_visibleInstances")->Set(visUAV);
        g_pPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX,  "b_visibleInstances")->Set(visSRV);
        EngineLog("[Renderer] Visible instances buffer bound as static");
    }

    // Graphics SRB — bind once against the global megabuffer
    {
        g_pPSO->CreateShaderResourceBinding(&g_pSRB, true);
        assert(g_pSRB);
        IBufferView* vbSRV  = Graphics::GetGlobalVertexBuffer()->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE);
        IBufferView* matSRV = g_pMatrixBuffer->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE);
        assert(vbSRV && matSRV);
        g_pSRB->GetVariableByName(SHADER_TYPE_VERTEX, "b_vertices") ->Set(vbSRV);
        g_pSRB->GetVariableByName(SHADER_TYPE_VERTEX, "b_instances")->Set(matSRV);
        g_pSRB->GetVariableByName(SHADER_TYPE_VERTEX, "DrawParams") ->Set(g_pDrawParamsCB);
        EngineLog("[Renderer] Graphics SRB bound");
    }

    // Compute SRB
    {
        g_pCullPSO->CreateShaderResourceBinding(&g_pCullSRB, true);
        assert(g_pCullSRB);
        IBufferView* matSRV  = g_pMatrixBuffer->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE);
        IBufferView* aabbSRV = g_pAABBBuffer->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE);
        IBufferView* iArgUAV = g_IndirectArgsBuffer->GetDefaultView(BUFFER_VIEW_UNORDERED_ACCESS);
        assert(matSRV && aabbSRV && iArgUAV);
        g_pCullSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "b_matrices")    ->Set(matSRV);
        g_pCullSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "b_bounds")      ->Set(aabbSRV);
        g_pCullSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "b_indirectArgs")->Set(iArgUAV);
        g_pCullSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "CullParams")    ->Set(g_pCullParamsCB);
        EngineLog("[Renderer] Compute SRB bound");
    }

    // Register ECS components
    {
        auto reg = [&](const char* name, size_t sz, size_t align) -> ecs_entity_t {
            ecs_entity_desc_t ed = {}; ed.name = name;
            ecs_component_desc_t cd = {};
            cd.entity = ecs_entity_init(world, &ed);
            cd.type.size      = static_cast<ecs_size_t>(sz);
            cd.type.alignment = static_cast<ecs_size_t>(align);
            return ecs_component_init(world, &cd);
        };
        id_MeshData     = reg("MeshData",     sizeof(MeshData),  alignof(MeshData));
        id_AABB         = reg("AABB",         sizeof(AABB),      alignof(AABB));
        id_EntityIntent = reg("EntityIntent", sizeof(Intent),    alignof(Intent));
        id_Material     = reg("Material",     sizeof(Material),  alignof(Material));
        assert(id_MeshData && id_AABB && id_EntityIntent && id_Material);
    }

    // ECS queries
    {
        ecs_query_desc_t cq = {};
        cq.terms[0].id = Engine::Camera::id_Camera;
        g_camera_query = ecs_query_init(world, &cq);
        assert(g_camera_query);

        ecs_query_desc_t rq = {};
        rq.terms[0].id = Engine::Transform::id_WorldMatrix;
        rq.terms[1].id = id_MeshData;
        rq.terms[2].id = id_EntityIntent;
        rq.terms[3].id = id_AABB;
        g_render_query = ecs_query_init(world, &rq);
        assert(g_render_query);
    }

    // ── Pass_Cull ─────────────────────────────────────────────────────────────
    // Runs first in Phase_React: gathers matrices+AABBs, uploads cull params,
    // resets indirect args, dispatches compute.
    {
        ecs_entity_desc_t ed = {}; ed.name = "Pass_Cull";
        ecs_entity_t sys_ent = ecs_entity_init(world, &ed);
        ecs_add_pair(world, sys_ent, EcsDependsOn, Engine::Pipeline::Phase_React);

        ecs_system_desc_t s = {};
        s.entity   = sys_ent;
        s.callback = [](ecs_iter_t* it) {
            IDeviceContext* ctx = Graphics::GetContext();
            assert(ctx);

            // Gather matrices, AABBs, and mesh alloc from the render query
            s_entity_count = 0;
            MeshData first_mesh_data = {};
            bool have_mesh = false;

            if (g_render_query) {
                ecs_iter_t ri = ecs_query_iter(it->world, g_render_query);
                while (ecs_query_next(&ri)) {
                    const Engine::Transform::WorldMatrix* wm    = ecs_field(&ri, Engine::Transform::WorldMatrix, 0);
                    const MeshData*                       md    = ecs_field(&ri, MeshData, 1);
                    const Intent*                         intent= ecs_field(&ri, Intent,   2);
                    const AABB*                           ab    = ecs_field(&ri, AABB,     3);

                    for (int i = 0; i < ri.count && s_entity_count < MAX_ENTITIES; ++i) {
                        if ((intent[i].mask & INTENT_VISIBLE)   == 0) continue;
                        if ((intent[i].mask & INTENT_DESTROYED) != 0) continue;

                        std::memcpy(&s_mat_flat[s_entity_count * 16], wm[i].value.data(), 64u);

                        s_aabb_flat[s_entity_count].aabb_min[0] = ab[i].min.x();
                        s_aabb_flat[s_entity_count].aabb_min[1] = ab[i].min.y();
                        s_aabb_flat[s_entity_count].aabb_min[2] = ab[i].min.z();
                        s_aabb_flat[s_entity_count].pad0 = 0.f;
                        s_aabb_flat[s_entity_count].aabb_max[0] = ab[i].max.x();
                        s_aabb_flat[s_entity_count].aabb_max[1] = ab[i].max.y();
                        s_aabb_flat[s_entity_count].aabb_max[2] = ab[i].max.z();
                        s_aabb_flat[s_entity_count].pad1 = 0.f;

                        if (!have_mesh) { first_mesh_data = md[i]; have_mesh = true; }
                        ++s_entity_count;
                    }
                }
            }
            if (s_entity_count == 0 || !have_mesh) return;

            // Upload matrix SSBO
            {
                void* p = nullptr;
                ctx->MapBuffer(g_pMatrixBuffer, MAP_WRITE, MAP_FLAG_DISCARD, p);
                if (p) { std::memcpy(p, s_mat_flat, s_entity_count * 64u); ctx->UnmapBuffer(g_pMatrixBuffer, MAP_WRITE); }
            }

            // Upload AABB SSBO
            {
                void* p = nullptr;
                ctx->MapBuffer(g_pAABBBuffer, MAP_WRITE, MAP_FLAG_DISCARD, p);
                if (p) { std::memcpy(p, s_aabb_flat, s_entity_count * 32u); ctx->UnmapBuffer(g_pAABBBuffer, MAP_WRITE); }
            }

            // Build VP matrix from camera, extract frustum planes, upload CullParamsCB
            float vp[16] = {};
            if (g_camera_query) {
                ecs_iter_t ci = ecs_query_iter(it->world, g_camera_query);
                while (ecs_query_next(&ci)) {
                    const Engine::Camera::Camera* cam = ecs_field(&ci, Engine::Camera::Camera, 0);
                    if (ci.count == 0) continue;
                    Eigen::Map<const Eigen::Matrix4f> V(cam[0].view);
                    Eigen::Map<const Eigen::Matrix4f> P(cam[0].proj);
                    Eigen::Matrix4f VP = P * V;
                    std::memcpy(vp, VP.data(), 64u);

                    void* p = nullptr;
                    ctx->MapBuffer(g_pDrawParamsCB, MAP_WRITE, MAP_FLAG_DISCARD, p);
                    if (p) { std::memcpy(p, vp, 64u); ctx->UnmapBuffer(g_pDrawParamsCB, MAP_WRITE); }

                    ecs_iter_fini(&ci);
                    break;
                }
            }

            CullParamsCPU cp = {};
            ExtractFrustumPlanes(vp, cp.frustumPlanes);
            cp.entityCount = s_entity_count;
            {
                void* p = nullptr;
                ctx->MapBuffer(g_pCullParamsCB, MAP_WRITE, MAP_FLAG_DISCARD, p);
                if (p) { std::memcpy(p, &cp, sizeof(cp)); ctx->UnmapBuffer(g_pCullParamsCB, MAP_WRITE); }
            }

            // Reset indirect args: fill indexCount/firstIndex/baseVertex from mesh alloc,
            // instanceCount=0 (compute will increment it atomically).
            IndirectCmd reset = {
                first_mesh_data.alloc.indexCount,
                0u,
                first_mesh_data.alloc.firstIndex,
                first_mesh_data.alloc.baseVertex,
                0u
            };
            ctx->UpdateBuffer(g_IndirectArgsBuffer, 0, sizeof(reset), &reset,
                              RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

            // Dispatch compute culler
            ctx->SetPipelineState(g_pCullPSO);
            ctx->CommitShaderResources(g_pCullSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

            DispatchComputeAttribs dc;
            dc.ThreadGroupCountX = (s_entity_count + 63u) / 64u;
            dc.ThreadGroupCountY = 1u;
            dc.ThreadGroupCountZ = 1u;
            ctx->DispatchCompute(dc);
        };
        ecs_system_init(world, &s);
    }

    // ── Pass_Opaque ───────────────────────────────────────────────────────────
    // Runs second in Phase_React: barriers, binds, DrawIndexedIndirect.
    {
        ecs_entity_desc_t ed = {}; ed.name = "Pass_Opaque";
        ecs_entity_t sys_ent = ecs_entity_init(world, &ed);
        ecs_add_pair(world, sys_ent, EcsDependsOn, Engine::Pipeline::Phase_React);

        ecs_system_desc_t s = {};
        s.entity   = sys_ent;
        s.callback = [](ecs_iter_t* it) {
            (void)it;
            if (s_entity_count == 0) return;

            IDeviceContext* ctx = Graphics::GetContext();
            assert(ctx);

            // Barrier: indirect args UAV (compute wrote) → INDIRECT_ARGUMENT (GPU reads for draw).
            // Visible instances transitions automatically via CommitShaderResources below.
            StateTransitionDesc barrier(g_IndirectArgsBuffer,
                RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_INDIRECT_ARGUMENT,
                STATE_TRANSITION_FLAG_UPDATE_STATE);
            ctx->TransitionResourceStates(1, &barrier);

            // Set global index buffer
            ctx->SetIndexBuffer(Graphics::GetGlobalIndexBuffer(), 0,
                                RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

            ctx->SetPipelineState(g_pPSO);
            ctx->CommitShaderResources(g_pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

            DrawIndexedIndirectAttribs draw;
            draw.IndexType              = VT_UINT32;
            draw.pAttribsBuffer         = g_IndirectArgsBuffer;
            draw.DrawCount              = 1u;
            draw.Flags                  = DRAW_FLAG_VERIFY_ALL;
            ctx->DrawIndexedIndirect(draw);

            if (!g_logged_first) {
                EngineLog("[Renderer] First MDI draw submitted");
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
    g_pCullSRB.Release();
    g_pCullPSO.Release();
    g_pSRB.Release();
    g_pPSO.Release();
    g_IndirectArgsBuffer.Release();
    g_pVisibleInstancesBuffer.Release();
    g_pAABBBuffer.Release();
    g_pCullParamsCB.Release();
    g_pMatrixBuffer.Release();
    g_pDrawParamsCB.Release();
    EngineLog("[Renderer] Shutdown complete");
}

} // namespace Renderer
} // namespace Engine
