#include <engine/renderer/renderer.h>
#include <engine/renderer/core.h>
#include <engine/renderer/backend.h>
#include <engine/transform.h>
#include <engine/camera.h>
#include <engine/spatial.h>
#include <engine/pipeline.h>
#include <engine/engine.h>

#include "Graphics/GraphicsEngine/interface/PipelineState.h"
#include "Graphics/GraphicsEngine/interface/ShaderResourceBinding.h"
#include "Graphics/GraphicsEngine/interface/Shader.h"
#include "Graphics/GraphicsEngine/interface/GraphicsTypes.h"

#include <cglm/cglm.h>
#include <cglm/struct.h>
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

ecs_entity_t id_MeshData     = 0;
ecs_entity_t id_AABB         = 0;
ecs_entity_t id_EntityIntent = 0;
ecs_entity_t id_Material     = 0;

static constexpr uint32_t MAX_ENTITIES    = 16384u;
static constexpr uint32_t MAX_LOD_GROUPS  = 256u;
static constexpr uint32_t LOD_LEVELS      = 3u;

static RefCntAutoPtr<IPipelineState>         g_pPSO;
static RefCntAutoPtr<IShaderResourceBinding> g_pSRB;
static RefCntAutoPtr<IBuffer>                g_pDrawParamsCB;

static RefCntAutoPtr<IPipelineState>         g_pCullPSO;
static RefCntAutoPtr<IShaderResourceBinding> g_pCullSRB;

static RefCntAutoPtr<IBuffer> g_RawMatrixSSBO;
static RefCntAutoPtr<IBuffer> g_pAABBBuffer;
static RefCntAutoPtr<IBuffer> g_EntityMeshIdBuffer;
static RefCntAutoPtr<IBuffer> g_pCullParamsCB;

static RefCntAutoPtr<IBuffer> g_IndirectArgsBuffer;
static RefCntAutoPtr<IBuffer> g_VisibleMatrixSSBO;

static ecs_query_t* g_render_query = nullptr;
static ecs_query_t* g_camera_query = nullptr;

struct alignas(16) ShaderAABB {
    float aabb_min[3]; float pad0;
    float aabb_max[3]; float pad1;
};

static float      s_mat_flat[MAX_ENTITIES * 16];
static ShaderAABB s_aabb_flat[MAX_ENTITIES];
static uint32_t   s_mesh_ids[MAX_ENTITIES];
static uint32_t   s_entity_count = 0;

struct IndirectCmd {
    uint32_t indexCount;
    uint32_t instanceCount;
    uint32_t firstIndex;
    uint32_t baseVertex;
    uint32_t firstInstance;
};

struct CullParamsCPU {
    float    frustumPlanes[6][4];
    float    cameraWorldPos[4];
    uint32_t entityCount;
    uint32_t _pad[3];
};

static char* LoadShaderSource(const char* path, uint32_t* out_len) {
    FILE* f = std::fopen(path, "rb");
    if (!f) return nullptr;
    std::fseek(f, 0, SEEK_END);
    long sz = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    char* buf = (char*)std::malloc((size_t)sz + 1);
    if (!buf) { std::fclose(f); return nullptr; }
    std::fread(buf, 1, (size_t)sz, f);
    buf[sz] = '\0';
    std::fclose(f);
    if (out_len) *out_len = (uint32_t)sz;
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
            if (src) return src;
        }
    }
    return nullptr;
}

static void ExtractFrustumPlanes(const float* vp, float planes[6][4]) {
    // cglm matrix is float[4][4] column-major.
    // Transpose to Row-Contiguous format to use standard Gribb-Hartmann.
    mat4 vpt;
    glm_mat4_transpose_to((float (*)[4])vp, vpt);

    // Left:   row3 + row0
    glm_vec4_add(vpt[3], vpt[0], planes[0]);
    // Right:  row3 - row0
    glm_vec4_sub(vpt[3], vpt[0], planes[1]);
    // Bottom: row3 + row1
    glm_vec4_add(vpt[3], vpt[1], planes[2]);
    // Top:    row3 - row1
    glm_vec4_sub(vpt[3], vpt[1], planes[3]);
    // Near:   row2 (for 0 to 1 depth, Vulkan style)
    glm_vec4_copy(vpt[2], planes[4]);
    // Far:    row3 - row2 (for 0 to 1 depth, Vulkan style)
    glm_vec4_sub(vpt[3], vpt[2], planes[5]);

    // Plane normalization: divide by length of normal (A, B, C)
    for (int i = 0; i < 6; ++i) {
        float len = std::sqrt(planes[i][0] * planes[i][0] + 
                              planes[i][1] * planes[i][1] + 
                              planes[i][2] * planes[i][2]);
        if (len > 1e-6f) {
            float invLen = 1.0f / len;
            planes[i][0] *= invLen;
            planes[i][1] *= invLen;
            planes[i][2] *= invLen;
            planes[i][3] *= invLen;
        }
    }
}

static bool CreateGraphicsPSO() {
    IRenderDevice* dev = Graphics::GetDevice();
    ISwapChain*    sc  = Graphics::GetSwapChain();
    
    uint32_t hlsl_len = 0;
    char* hlsl = DiscoverShader("mesh.hlsl", &hlsl_len);
    if (!hlsl) return false;

    ShaderCreateInfo sci;
    sci.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    sci.Source         = hlsl;
    sci.SourceLength   = hlsl_len;

    RefCntAutoPtr<IShader> pVS, pPS;
    { sci.Desc.ShaderType = SHADER_TYPE_VERTEX; sci.Desc.Name = "MeshVS"; sci.EntryPoint = "VSMain"; dev->CreateShader(sci, &pVS); }
    { sci.Desc.ShaderType = SHADER_TYPE_PIXEL;  sci.Desc.Name = "MeshPS"; sci.EntryPoint = "PSMain"; dev->CreateShader(sci, &pPS); }
    std::free(hlsl);

    if (!pVS || !pPS) return false;

    ShaderResourceVariableDesc vars[] = {
        {SHADER_TYPE_VERTEX, "b_vertices",  SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {SHADER_TYPE_VERTEX, "b_instances", SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
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
    psoCI.GraphicsPipeline.RasterizerDesc.FrontCounterClockwise = False;
    psoCI.pVS = pVS;
    psoCI.pPS = pPS;

    dev->CreateGraphicsPipelineState(psoCI, &g_pPSO);
    return g_pPSO != nullptr;
}

static bool CreateComputePSO() {
    IRenderDevice* dev = Graphics::GetDevice();
    
    uint32_t hlsl_len = 0;
    char* hlsl = DiscoverShader("cull.hlsl", &hlsl_len);
    if (!hlsl) return false;

    ShaderCreateInfo sci;
    sci.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    sci.Source         = hlsl;
    sci.SourceLength   = hlsl_len;
    sci.Desc.ShaderType = SHADER_TYPE_COMPUTE;
    sci.Desc.Name       = "CullCS";
    sci.EntryPoint      = "CSMain";

    RefCntAutoPtr<IShader> pCS;
    dev->CreateShader(sci, &pCS);
    std::free(hlsl);

    if (!pCS) return false;

    ShaderResourceVariableDesc cullVars[] = {
        {SHADER_TYPE_COMPUTE, "b_matrices",          SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {SHADER_TYPE_COMPUTE, "b_bounds",            SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {SHADER_TYPE_COMPUTE, "b_lodGroups",         SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
        {SHADER_TYPE_COMPUTE, "b_entityLodIds",      SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {SHADER_TYPE_COMPUTE, "b_indirectArgs",      SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {SHADER_TYPE_COMPUTE, "b_outVisibleMatrices",SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
        {SHADER_TYPE_COMPUTE, "CullParams",          SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
    };

    ComputePipelineStateCreateInfo cpsoCI;
    cpsoCI.PSODesc.Name                        = "CullPSO";
    cpsoCI.PSODesc.PipelineType                = PIPELINE_TYPE_COMPUTE;
    cpsoCI.PSODesc.ResourceLayout.Variables    = cullVars;
    cpsoCI.PSODesc.ResourceLayout.NumVariables = 7u;
    cpsoCI.pCS = pCS;

    dev->CreateComputePipelineState(cpsoCI, &g_pCullPSO);
    return g_pCullPSO != nullptr;
}

static int compare_chunk_hashes(ecs_entity_t, const void* a, ecs_entity_t, const void* b) {
    const uint64_t ha = ((const Engine::Spatial::ChunkHash*)a)->hash;
    const uint64_t hb = ((const Engine::Spatial::ChunkHash*)b)->hash;
    return (ha > hb) - (ha < hb);
}

void Init(ecs_world_t* world) {
    IRenderDevice* dev = Graphics::GetDevice();

    {
        BufferDesc bd;
        bd.Name              = "RawMatrixSSBO";
        bd.Usage             = USAGE_DYNAMIC;
        bd.BindFlags         = BIND_SHADER_RESOURCE;
        bd.Mode              = BUFFER_MODE_STRUCTURED;
        bd.ElementByteStride = 64u;
        bd.CPUAccessFlags    = CPU_ACCESS_WRITE;
        bd.Size              = MAX_ENTITIES * 64u;
        dev->CreateBuffer(bd, nullptr, &g_RawMatrixSSBO);
    }
    {
        BufferDesc bd;
        bd.Name           = "DrawParamsCB";
        bd.Usage          = USAGE_DYNAMIC;
        bd.BindFlags      = BIND_UNIFORM_BUFFER;
        bd.CPUAccessFlags = CPU_ACCESS_WRITE;
        bd.Size           = 64u;
        dev->CreateBuffer(bd, nullptr, &g_pDrawParamsCB);
    }
    {
        BufferDesc bd;
        bd.Name              = "AABBBuffer";
        bd.Usage             = USAGE_DYNAMIC;
        bd.BindFlags         = BIND_SHADER_RESOURCE;
        bd.Mode              = BUFFER_MODE_STRUCTURED;
        bd.ElementByteStride = 32u;
        bd.CPUAccessFlags    = CPU_ACCESS_WRITE;
        bd.Size              = MAX_ENTITIES * 32u;
        dev->CreateBuffer(bd, nullptr, &g_pAABBBuffer);
    }
    {
        BufferDesc bd;
        bd.Name              = "EntityMeshIdBuffer";
        bd.Usage             = USAGE_DYNAMIC;
        bd.BindFlags         = BIND_SHADER_RESOURCE;
        bd.Mode              = BUFFER_MODE_STRUCTURED;
        bd.ElementByteStride = 4u;
        bd.CPUAccessFlags    = CPU_ACCESS_WRITE;
        bd.Size              = MAX_ENTITIES * 4u;
        dev->CreateBuffer(bd, nullptr, &g_EntityMeshIdBuffer);
    }
    {
        BufferDesc bd;
        bd.Name           = "CullParamsCB";
        bd.Usage          = USAGE_DYNAMIC;
        bd.BindFlags      = BIND_UNIFORM_BUFFER;
        bd.CPUAccessFlags = CPU_ACCESS_WRITE;
        bd.Size           = 128u;
        dev->CreateBuffer(bd, nullptr, &g_pCullParamsCB);
    }
    {
        BufferDesc bd;
        bd.Name              = "IndirectArgs";
        bd.Usage             = USAGE_DEFAULT;
        bd.BindFlags         = BIND_INDIRECT_DRAW_ARGS | BIND_UNORDERED_ACCESS;
        bd.Mode              = BUFFER_MODE_STRUCTURED;
        bd.ElementByteStride = 20u;
        bd.Size              = MAX_LOD_GROUPS * LOD_LEVELS * 20u;
        dev->CreateBuffer(bd, nullptr, &g_IndirectArgsBuffer);
    }
    {
        BufferDesc bd;
        bd.Name              = "VisibleMatrixSSBO";
        bd.Usage             = USAGE_DEFAULT;
        bd.BindFlags         = BIND_UNORDERED_ACCESS | BIND_SHADER_RESOURCE;
        bd.Mode              = BUFFER_MODE_STRUCTURED;
        bd.ElementByteStride = 64u;
        bd.Size              = LOD_LEVELS * MAX_ENTITIES * 64u;
        dev->CreateBuffer(bd, nullptr, &g_VisibleMatrixSSBO);
    }

    CreateGraphicsPSO();
    CreateComputePSO();

    {
        IBufferView* visUAV = g_VisibleMatrixSSBO->GetDefaultView(BUFFER_VIEW_UNORDERED_ACCESS);
        IBufferView* visSRV = g_VisibleMatrixSSBO->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE);
        IBufferView* lodSRV = Graphics::GetLODGroupBuffer()->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE);
        g_pCullPSO->GetStaticVariableByName(SHADER_TYPE_COMPUTE, "b_outVisibleMatrices")->Set(visUAV);
        g_pCullPSO->GetStaticVariableByName(SHADER_TYPE_COMPUTE, "b_lodGroups")         ->Set(lodSRV);
        g_pPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "b_instances")              ->Set(visSRV);
    }

    {
        g_pPSO->CreateShaderResourceBinding(&g_pSRB, true);
        IBufferView* vbSRV = Graphics::GetGlobalVertexBuffer()->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE);
        g_pSRB->GetVariableByName(SHADER_TYPE_VERTEX, "b_vertices") ->Set(vbSRV);
        g_pSRB->GetVariableByName(SHADER_TYPE_VERTEX, "DrawParams") ->Set(g_pDrawParamsCB);
    }

    {
        g_pCullPSO->CreateShaderResourceBinding(&g_pCullSRB, true);
        IBufferView* matSRV   = g_RawMatrixSSBO->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE);
        IBufferView* aabbSRV  = g_pAABBBuffer->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE);
        IBufferView* meshSRV  = g_EntityMeshIdBuffer->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE);
        IBufferView* iArgUAV  = g_IndirectArgsBuffer->GetDefaultView(BUFFER_VIEW_UNORDERED_ACCESS);
        g_pCullSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "b_matrices")    ->Set(matSRV);
        g_pCullSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "b_bounds")      ->Set(aabbSRV);
        g_pCullSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "b_entityLodIds")->Set(meshSRV);
        g_pCullSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "b_indirectArgs")->Set(iArgUAV);
        g_pCullSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "CullParams")    ->Set(g_pCullParamsCB);
    }

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

    {
        ecs_query_desc_t cq = {};
        cq.terms[0].id = Engine::Camera::id_Camera;
        g_camera_query = ecs_query_init(world, &cq);

        ecs_query_desc_t rq = {};
        rq.terms[0].id    = Engine::Transform::id_WorldMatrix;
        rq.terms[1].id    = id_MeshData;
        rq.terms[2].id    = id_EntityIntent;
        rq.terms[3].id    = id_AABB;
        rq.terms[4].id    = Engine::Spatial::id_ChunkCoord;
        rq.terms[4].inout = EcsIn;
        rq.terms[5].id    = Engine::Spatial::id_ChunkHash;
        rq.terms[5].inout = EcsIn;
        rq.order_by          = Engine::Spatial::id_ChunkHash;
        rq.order_by_callback = compare_chunk_hashes;
        g_render_query = ecs_query_init(world, &rq);
    }

    {
        ecs_entity_desc_t ed = {}; ed.name = "Pass_Cull";
        ecs_entity_t sys_ent = ecs_entity_init(world, &ed);
        ecs_add_pair(world, sys_ent, EcsDependsOn, Engine::Pipeline::Phase_React);

        ecs_system_desc_t s = {};
        s.entity          = sys_ent;
        s.multi_threaded  = false;
        s.callback = [](ecs_iter_t* it) {
            IDeviceContext* ctx = Graphics::GetContext();
            const uint32_t numGroups = Graphics::GetLODGroupCount();
            if (numGroups == 0) return;

            int32_t cam_cx = 0, cam_cy = 0;
            float   cam_pos[3] = {};
            mat4    vp = GLM_MAT4_IDENTITY_INIT;
            if (g_camera_query) {
                ecs_iter_t ci = ecs_query_iter(it->world, g_camera_query);
                while (ecs_query_next(&ci)) {
                    const Engine::Camera::Camera* cam = (const Engine::Camera::Camera*)ecs_field(&ci, Engine::Camera::Camera, 0);
                    if (ci.count == 0) continue;
                    cam_cx    = (int32_t)floorf(cam[0].position[0] / Engine::Spatial::CHUNK_SIZE);
                    cam_cy    = (int32_t)floorf(cam[0].position[2] / Engine::Spatial::CHUNK_SIZE);
                    glm_vec3_copy((float*)cam[0].position, cam_pos);
                    
                    glm_mat4_mul((float (*)[4])cam[0].proj, (float (*)[4])cam[0].view, vp);

                    void* p = nullptr;
                    ctx->MapBuffer(g_pDrawParamsCB, MAP_WRITE, MAP_FLAG_DISCARD, p);
                    if (p) { std::memcpy(p, vp, 64u); ctx->UnmapBuffer(g_pDrawParamsCB, MAP_WRITE); }
                    ecs_iter_fini(&ci);
                    break;
                }
            }

            constexpr int RENDER_DISTANCE = 4;
            s_entity_count = 0;

            if (g_render_query) {
                ecs_iter_t ri = ecs_query_iter(it->world, g_render_query);
                while (ecs_query_next(&ri)) {
                    const Engine::Transform::WorldMatrix*      wm     = (const Engine::Transform::WorldMatrix*)ecs_field(&ri, Engine::Transform::WorldMatrix, 0);
                    const MeshData*                            md     = (const MeshData*)ecs_field(&ri, MeshData, 1);
                    const Intent*                              intent = (const Intent*)ecs_field(&ri, Intent,   2);
                    const AABB*                                ab     = (const AABB*)ecs_field(&ri, AABB,     3);
                    const Engine::Spatial::ChunkCoord*         chunk  = (const Engine::Spatial::ChunkCoord*)ecs_field(&ri, Engine::Spatial::ChunkCoord, 4);

                    for (int i = 0; i < ri.count && s_entity_count < MAX_ENTITIES; ++i) {
                        if ((intent[i].mask & INTENT_VISIBLE)   == 0) continue;
                        if ((intent[i].mask & INTENT_DESTROYED) != 0) continue;
                        if (std::abs(chunk[i].x - cam_cx) > RENDER_DISTANCE) continue;
                        if (std::abs(chunk[i].y - cam_cy) > RENDER_DISTANCE) continue;

                        std::memcpy(&s_mat_flat[s_entity_count * 16], wm[i].value, 64u);

                        glm_vec3_copy((float*)ab[i].min, s_aabb_flat[s_entity_count].aabb_min);
                        s_aabb_flat[s_entity_count].pad0 = 0.f;
                        glm_vec3_copy((float*)ab[i].max, s_aabb_flat[s_entity_count].aabb_max);
                        s_aabb_flat[s_entity_count].pad1 = 0.f;

                        s_mesh_ids[s_entity_count] = md[i].lod_group_id;
                        ++s_entity_count;
                    }
                }
            }
            if (s_entity_count == 0) return;

            {
                void* p = nullptr;
                ctx->MapBuffer(g_RawMatrixSSBO, MAP_WRITE, MAP_FLAG_DISCARD, p);
                if (p) { std::memcpy(p, s_mat_flat, s_entity_count * 64u); ctx->UnmapBuffer(g_RawMatrixSSBO, MAP_WRITE); }
            }
            {
                void* p = nullptr;
                ctx->MapBuffer(g_pAABBBuffer, MAP_WRITE, MAP_FLAG_DISCARD, p);
                if (p) { std::memcpy(p, s_aabb_flat, s_entity_count * 32u); ctx->UnmapBuffer(g_pAABBBuffer, MAP_WRITE); }
            }
            {
                void* p = nullptr;
                ctx->MapBuffer(g_EntityMeshIdBuffer, MAP_WRITE, MAP_FLAG_DISCARD, p);
                if (p) { std::memcpy(p, s_mesh_ids, s_entity_count * 4u); ctx->UnmapBuffer(g_EntityMeshIdBuffer, MAP_WRITE); }
            }

            CullParamsCPU cp = {};
            ExtractFrustumPlanes((float*)vp, cp.frustumPlanes);
            glm_vec3_copy(cam_pos, cp.cameraWorldPos);
            cp.cameraWorldPos[3] = 0.f;
            cp.entityCount = s_entity_count;
            {
                void* p = nullptr;
                ctx->MapBuffer(g_pCullParamsCB, MAP_WRITE, MAP_FLAG_DISCARD, p);
                if (p) { std::memcpy(p, &cp, sizeof(cp)); ctx->UnmapBuffer(g_pCullParamsCB, MAP_WRITE); }
            }

            {
                const uint32_t totalSlots = numGroups * LOD_LEVELS;
                IndirectCmd resetCmds[MAX_LOD_GROUPS * LOD_LEVELS];
                for (uint32_t g = 0; g < numGroups; ++g) {
                    const Graphics::LODGroup* lg = Graphics::GetLODGroup(g);
                    if (!lg) continue;
                    for (uint32_t lod = 0; lod < LOD_LEVELS; ++lod) {
                        const uint32_t slot = g * LOD_LEVELS + lod;
                        resetCmds[slot].indexCount = lg->lods[lod].indexCount;
                        resetCmds[slot].instanceCount = 0u;
                        resetCmds[slot].firstIndex = lg->lods[lod].firstIndex;
                        resetCmds[slot].baseVertex = lg->lods[lod].baseVertex;
                        resetCmds[slot].firstInstance = slot * MAX_ENTITIES;
                    }
                }
                ctx->UpdateBuffer(g_IndirectArgsBuffer, 0,
                    totalSlots * (uint32_t)sizeof(IndirectCmd),
                    resetCmds,
                    RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            }

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

    {
        ecs_entity_desc_t ed = {}; ed.name = "Pass_Opaque";
        ecs_entity_t sys_ent = ecs_entity_init(world, &ed);
        ecs_add_pair(world, sys_ent, EcsDependsOn, Engine::Pipeline::Phase_React);

        ecs_system_desc_t s = {};
        s.entity          = sys_ent;
        s.multi_threaded  = false;
        s.callback = [](ecs_iter_t* it) {
            (void)it;
            if (s_entity_count == 0) return;
            const uint32_t numGroups = Graphics::GetLODGroupCount();
            if (numGroups == 0) return;

            IDeviceContext* ctx = Graphics::GetContext();
            StateTransitionDesc barrier(g_IndirectArgsBuffer,
                RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_INDIRECT_ARGUMENT,
                STATE_TRANSITION_FLAG_UPDATE_STATE);
            ctx->TransitionResourceStates(1, &barrier);
            ctx->SetIndexBuffer(Graphics::GetGlobalIndexBuffer(), 0,
                                RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            ctx->SetPipelineState(g_pPSO);
            ctx->CommitShaderResources(g_pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

            DrawIndexedIndirectAttribs draw;
            draw.IndexType      = VT_UINT32;
            draw.pAttribsBuffer = g_IndirectArgsBuffer;
            draw.DrawCount      = numGroups * LOD_LEVELS;
            draw.Flags          = DRAW_FLAG_VERIFY_ALL;
            ctx->DrawIndexedIndirect(draw);
        };
        ecs_system_init(world, &s);
    }
}

void Shutdown() {
    if (g_render_query) ecs_query_fini(g_render_query);
    if (g_camera_query) ecs_query_fini(g_camera_query);
    g_pCullSRB.Release();
    g_pCullPSO.Release();
    g_pSRB.Release();
    g_pPSO.Release();
    g_IndirectArgsBuffer.Release();
    g_VisibleMatrixSSBO.Release();
    g_EntityMeshIdBuffer.Release();
    g_pAABBBuffer.Release();
    g_pCullParamsCB.Release();
    g_RawMatrixSSBO.Release();
    g_pDrawParamsCB.Release();
}

} // namespace Renderer
} // namespace Engine
