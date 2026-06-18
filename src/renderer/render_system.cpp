#include <engine/renderer/renderer.h>
#include <engine/renderer/core.h>
#include <engine/renderer/backend.h>
#include <engine/debug/debug_draw.h>
#include <engine/transform.h>
#include <engine/camera.h>
#include <engine/spatial.h>
#include <engine/pipeline.h>
#include <engine/engine.h>
#include <flecs.h>

#include "Graphics/GraphicsEngine/interface/PipelineState.h"
#include "Graphics/GraphicsEngine/interface/ShaderResourceBinding.h"
#include "Graphics/GraphicsEngine/interface/Shader.h"
#include "Graphics/GraphicsEngine/interface/GraphicsTypes.h"
#include "Graphics/GraphicsEngine/interface/CommandList.h"

#include <cglm/cglm.h>
#include <cglm/struct.h>
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cmath>

using namespace Diligent;

extern "C" {

ENGINE_API uint64_t id_ScryMeshData     = 0;
ENGINE_API uint64_t id_ScryEntityIntent = 0;
ENGINE_API uint64_t id_ScryMaterial     = 0;

static const uint32_t MAX_VISIBLE_ENTITIES = 16384u;
static const int      RENDER_RADIUS        = 3;
static const uint32_t MAX_LOD_GROUPS  = 256u;
static const uint32_t LOD_LEVELS      = 3u;

static RefCntAutoPtr<IPipelineState>         g_pPSO;
static RefCntAutoPtr<IShaderResourceBinding> g_pSRB;
static RefCntAutoPtr<IBuffer>                g_pDrawParamsCB;

static RefCntAutoPtr<IPipelineState>         g_pCullPSO;
static RefCntAutoPtr<IShaderResourceBinding> g_pCullSRB;

static RefCntAutoPtr<IBuffer> g_RawMatrixSSBO;
static RefCntAutoPtr<IBuffer> g_EntityMeshIdBuffer;
static RefCntAutoPtr<IBuffer> g_pCullParamsCB;

static RefCntAutoPtr<IBuffer> g_IndirectArgsBuffer;
static RefCntAutoPtr<IBuffer> g_VisibleMatrixSSBO;

static ecs_query_t* g_render_query = NULL;
static ecs_query_t* g_camera_query = NULL;

static RefCntAutoPtr<ICommandList> g_pCommandLists[4];
static uint32_t                    g_NumCommandLists = 0;

typedef struct IndirectCmd {
    uint32_t indexCount;
    uint32_t instanceCount;
    uint32_t firstIndex;
    uint32_t baseVertex;
    uint32_t firstInstance;
} IndirectCmd;

typedef struct CullParamsCPU {
    float    frustumPlanes[6][4];
    float    cameraWorldPos[4];
    uint32_t entityCount;
    uint32_t _pad[3];
} CullParamsCPU;

static uint32_t s_entity_count = 0;

// Scratch buffer for resetting IndirectArgs each frame (instanceCount cleared, mesh data preserved).
static IndirectCmd s_clear_cmds[MAX_LOD_GROUPS * LOD_LEVELS];

static char* LoadShaderSource(const char* path, uint32_t* out_len) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = (char*)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t readCount = fread(buf, 1, (size_t)sz, f);
    buf[readCount] = '\0';
    fclose(f);
    if (out_len) *out_len = (uint32_t)readCount;
    return buf;
}

static char* DiscoverShader(const char* filename, uint32_t* out_len) {
    const char* prefixes[] = {
        "assets/raw/shaders/",
        "../assets/raw/shaders/",
        "../../assets/raw/shaders/",
    };
    char path[512];
    for (int i = 0; i < 3; ++i) {
        snprintf(path, sizeof(path), "%s%s", prefixes[i], filename);
        FILE* f = fopen(path, "rb");
        if (f) {
            fclose(f);
            return LoadShaderSource(path, out_len);
        }
    }
    return NULL;
}

static void Renderer_DrawMDI(uint32_t entity_count) {
    if (entity_count == 0) return;
    const uint32_t numGroups = ScryGraphics_GetLODGroupCount();
    if (numGroups == 0 || !g_pPSO || !g_pSRB) return;

    IDeviceContext* ctx = (IDeviceContext*)ScryGraphics_GetContext();

    StateTransitionDesc barrier(g_IndirectArgsBuffer,
        RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_INDIRECT_ARGUMENT,
        STATE_TRANSITION_FLAG_UPDATE_STATE);
    ctx->TransitionResourceStates(1, &barrier);

    ctx->SetIndexBuffer((IBuffer*)ScryGraphics_GetGlobalIndexBuffer(), 0,
        RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    ctx->SetPipelineState(g_pPSO);
    ctx->CommitShaderResources(g_pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    DrawIndexedIndirectAttribs draw;
    draw.IndexType      = VT_UINT32;
    draw.pAttribsBuffer = g_IndirectArgsBuffer;
    draw.DrawCount      = numGroups * LOD_LEVELS;
    draw.DrawArgsStride = static_cast<Uint32>(sizeof(IndirectCmd));
    draw.Flags          = DRAW_FLAG_VERIFY_ALL;
    ctx->DrawIndexedIndirect(draw);
}

static void PassCullCallback(ecs_iter_t* it) {
    if (it->count == 0 || !g_render_query) return;
    if (!g_pCullPSO || !g_pCullSRB) return;

    const ScryCamera* cam = ecs_field(it, ScryCamera, 0);
    IDeviceContext* ctx = (IDeviceContext*)ScryGraphics_GetContext();

    // Zero-copy: map GPU buffers directly and stream ECS data into them.
    PVoid pMat  = nullptr; ctx->MapBuffer(g_RawMatrixSSBO,      MAP_WRITE, MAP_FLAG_DISCARD, pMat);
    PVoid pMesh = nullptr; ctx->MapBuffer(g_EntityMeshIdBuffer, MAP_WRITE, MAP_FLAG_DISCARD, pMesh);

    s_entity_count = 0;
    if (pMat && pMesh) {
        float*    dest_mat  = static_cast<float*>(pMat);
        uint32_t* dest_mesh = static_cast<uint32_t*>(pMesh);

        int cam_chunk_x = (int)(cam[0].position[0] / SCRY_CHUNK_SIZE);
        int cam_chunk_z = (int)(cam[0].position[2] / SCRY_CHUNK_SIZE);

        ecs_iter_t ri = ecs_query_iter(it->world, g_render_query);
        while (ecs_query_next(&ri)) {
            ScryWorldMatrix*    wm     = ecs_field(&ri, ScryWorldMatrix,    0);
            ScryMeshData*       md     = ecs_field(&ri, ScryMeshData,       1);
            ScryRendererIntent* intent = ecs_field(&ri, ScryRendererIntent, 2);
            ScryChunkCoord*     cc     = ecs_field(&ri, ScryChunkCoord,     3);
            if (s_entity_count >= MAX_VISIBLE_ENTITIES) { ecs_iter_fini(&ri); break; }
            for (int i = 0; i < ri.count; ++i) {
                if (s_entity_count >= MAX_VISIBLE_ENTITIES) break;
                if (!(intent[i].mask & SCRY_INTENT_VISIBLE)) continue;
                int dx = cc[i].x - cam_chunk_x;
                int dz = cc[i].y - cam_chunk_z;
                if (dx < -RENDER_RADIUS || dx > RENDER_RADIUS || dz < -RENDER_RADIUS || dz > RENDER_RADIUS) continue;
                memcpy(dest_mat, wm[i].value, 64u);
                dest_mat += 16;
                *dest_mesh++ = md[i].lod_group_id;
                ++s_entity_count;
            }
        }
    }

    if (pMat)  ctx->UnmapBuffer(g_RawMatrixSSBO,      MAP_WRITE);
    if (pMesh) ctx->UnmapBuffer(g_EntityMeshIdBuffer, MAP_WRITE);

    if (s_entity_count == 0) {
        printf("[Renderer] WARNING: 0 entities matched the cull query!\n");
        fflush(stdout);
        return;
    }

    // Upload view-projection matrix into DrawParamsCB for the vertex shader.
    // USAGE_DYNAMIC buffers must be mapped at least once per frame before CommitShaderResources.
    {
        mat4 vp;
        glm_mat4_mul((float (*)[4])cam[0].proj, (float (*)[4])cam[0].view, vp);
        DebugDraw_SetViewProj((float*)vp);
        PVoid p = nullptr; ctx->MapBuffer(g_pDrawParamsCB, MAP_WRITE, MAP_FLAG_DISCARD, p);
        if (p) { memcpy(p, vp, 64u); ctx->UnmapBuffer(g_pDrawParamsCB, MAP_WRITE); }
    }

    // Upload cull params (frustum planes + camera position come from the camera component).
    {
        CullParamsCPU params = {};
        memcpy(params.frustumPlanes, cam[0].frustum_planes, sizeof(cam[0].frustum_planes));
        params.cameraWorldPos[0] = cam[0].position[0];
        params.cameraWorldPos[1] = cam[0].position[1];
        params.cameraWorldPos[2] = cam[0].position[2];
        params.cameraWorldPos[3] = 1.0f;
        params.entityCount = s_entity_count;
        PVoid p = nullptr; ctx->MapBuffer(g_pCullParamsCB, MAP_WRITE, MAP_FLAG_DISCARD, p);
        if (p) { memcpy(p, &params, sizeof(params)); ctx->UnmapBuffer(g_pCullParamsCB, MAP_WRITE); }
    }

    // Reset instanceCount in IndirectArgs to 0; preserve indexCount/firstIndex/baseVertex/firstInstance
    // so that the mesh draw command is valid even when 0 instances pass culling.
    {
        const uint32_t numGroups  = ScryGraphics_GetLODGroupCount();
        const uint32_t totalSlots = numGroups * LOD_LEVELS;
        const uint32_t visMatCap  = LOD_LEVELS * MAX_VISIBLE_ENTITIES;
        const uint32_t perSlot    = (totalSlots > 0) ? (visMatCap / totalSlots) : 0u;
        for (uint32_t g = 0; g < numGroups; ++g) {
            const ScryLODGroup* lg = ScryGraphics_GetLODGroup(g);
            for (uint32_t l = 0; l < LOD_LEVELS; ++l) {
                uint32_t slot = g * LOD_LEVELS + l;
                s_clear_cmds[slot].indexCount    = lg->lods[l].indexCount;
                s_clear_cmds[slot].instanceCount = 0;
                s_clear_cmds[slot].firstIndex    = lg->lods[l].firstIndex;
                s_clear_cmds[slot].baseVertex    = lg->lods[l].baseVertex;
                s_clear_cmds[slot].firstInstance = slot * perSlot;
            }
        }
        if (totalSlots > 0) {
            ctx->UpdateBuffer(g_IndirectArgsBuffer, 0,
                totalSlots * static_cast<uint32_t>(sizeof(IndirectCmd)),
                s_clear_cmds, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        }
    }

    // Dispatch GPU frustum culling + LOD selection compute shader.
    ctx->SetPipelineState(g_pCullPSO);
    ctx->CommitShaderResources(g_pCullSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    DispatchComputeAttribs disp;
    disp.ThreadGroupCountX = (s_entity_count + 63u) / 64u;
    disp.ThreadGroupCountY = 1u;
    disp.ThreadGroupCountZ = 1u;
    ctx->DispatchCompute(disp);
}

static void PassOpaqueCallback(ecs_iter_t* it) {
    (void)it;
    Renderer_DrawMDI(s_entity_count);
}

static void PassCleanupCallback(ecs_iter_t* it) {
    (void)it;
    for (uint32_t i = 0; i < g_NumCommandLists; ++i)
        g_pCommandLists[i].Release();
    g_NumCommandLists = 0;
}

static int compare_chunk_hashes(ecs_entity_t e1, const void* p1, ecs_entity_t e2, const void* p2) {
    (void)e1; (void)e2;
    const uint64_t h1 = ((const ScryChunkHash*)p1)->hash;
    const uint64_t h2 = ((const ScryChunkHash*)p2)->hash;
    return (h1 > h2) - (h1 < h2);
}

static uint64_t InternalRegComp(ecs_world_t* world, const char* name, size_t sz, size_t align) {
    ecs_entity_desc_t ed = {};
    ed.name = name;
    ecs_entity_t ent = ecs_entity_init(world, &ed);
    ecs_component_desc_t cd = {};
    cd.entity = ent;
    cd.type.size = (ecs_size_t)sz;
    cd.type.alignment = (ecs_size_t)align;
    return (uint64_t)ecs_component_init(world, &cd);
}

void ScryRenderer_Init(struct ecs_world_t* world) {
    IRenderDevice* dev = (IRenderDevice*)ScryGraphics_GetDevice();

    {
        BufferDesc bd;
        bd.Name              = "RawMatrixSSBO";
        bd.Usage             = USAGE_DYNAMIC;
        bd.BindFlags         = BIND_SHADER_RESOURCE;
        bd.Mode              = BUFFER_MODE_STRUCTURED;
        bd.ElementByteStride = 64u;
        bd.CPUAccessFlags    = CPU_ACCESS_WRITE;
        bd.Size              = MAX_VISIBLE_ENTITIES * 64u;
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
        bd.Name              = "EntityMeshIdBuffer";
        bd.Usage             = USAGE_DYNAMIC;
        bd.BindFlags         = BIND_SHADER_RESOURCE;
        bd.Mode              = BUFFER_MODE_STRUCTURED;
        bd.ElementByteStride = 4u;
        bd.CPUAccessFlags    = CPU_ACCESS_WRITE;
        bd.Size              = MAX_VISIBLE_ENTITIES * 4u;
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
        bd.Size              = LOD_LEVELS * MAX_VISIBLE_ENTITIES * 64u;
        dev->CreateBuffer(bd, nullptr, &g_VisibleMatrixSSBO);
    }

    // PSOs
    {
        ISwapChain* sc = (ISwapChain*)ScryGraphics_GetSwapChain();
        uint32_t hlsl_len = 0;
        char* hlsl = DiscoverShader("mesh.hlsl", &hlsl_len);
        if (hlsl) {
            ShaderCreateInfo sci;
            sci.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
            sci.Source = hlsl; sci.SourceLength = hlsl_len;
            RefCntAutoPtr<IShader> pVS, pPS;
            { sci.Desc.ShaderType = SHADER_TYPE_VERTEX; sci.Desc.Name = "MeshVS"; sci.EntryPoint = "VSMain"; dev->CreateShader(sci, &pVS); }
            { sci.Desc.ShaderType = SHADER_TYPE_PIXEL;  sci.Desc.Name = "MeshPS"; sci.EntryPoint = "PSMain"; dev->CreateShader(sci, &pPS); }
            free(hlsl);
            if (pVS && pPS) {
                ShaderResourceVariableDesc vars[] = {
                    {SHADER_TYPE_VERTEX, "b_vertices",  SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
                    {SHADER_TYPE_VERTEX, "b_instances", SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
                    {SHADER_TYPE_VERTEX, "b_lodGroups", SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
                    {SHADER_TYPE_VERTEX, "DrawParams",  SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
                };
                GraphicsPipelineStateCreateInfo psoCI;
                psoCI.PSODesc.Name = "MeshPSO"; psoCI.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
                psoCI.PSODesc.ResourceLayout.Variables = vars; psoCI.PSODesc.ResourceLayout.NumVariables = 4u;
                psoCI.GraphicsPipeline.NumRenderTargets = 1;
                psoCI.GraphicsPipeline.RTVFormats[0] = sc->GetDesc().ColorBufferFormat;
                psoCI.GraphicsPipeline.DSVFormat = sc->GetDesc().DepthBufferFormat;
                psoCI.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
                psoCI.GraphicsPipeline.DepthStencilDesc.DepthEnable = True;
                psoCI.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = True;
                psoCI.GraphicsPipeline.DepthStencilDesc.DepthFunc = COMPARISON_FUNC_LESS;
                psoCI.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_BACK;
                psoCI.GraphicsPipeline.RasterizerDesc.FrontCounterClockwise = False;
                psoCI.pVS = pVS; psoCI.pPS = pPS;
                dev->CreateGraphicsPipelineState(psoCI, &g_pPSO);
            }
        }
    }
    {
        uint32_t hlsl_len = 0;
        char* hlsl = DiscoverShader("cull.hlsl", &hlsl_len);
        if (hlsl) {
            ShaderCreateInfo sci;
            sci.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
            sci.Source = hlsl; sci.SourceLength = hlsl_len;
            sci.Desc.ShaderType = SHADER_TYPE_COMPUTE; sci.Desc.Name = "CullCS"; sci.EntryPoint = "CSMain";
            RefCntAutoPtr<IShader> pCS; dev->CreateShader(sci, &pCS);
            free(hlsl);
            if (pCS) {
                ShaderResourceVariableDesc cullVars[] = {
                    {SHADER_TYPE_COMPUTE, "b_matrices",          SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
                    {SHADER_TYPE_COMPUTE, "b_lodGroups",         SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
                    {SHADER_TYPE_COMPUTE, "b_entityLodIds",      SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
                    {SHADER_TYPE_COMPUTE, "b_indirectArgs",      SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
                    {SHADER_TYPE_COMPUTE, "b_outVisibleMatrices",SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
                    {SHADER_TYPE_COMPUTE, "CullParams",          SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
                };
                ComputePipelineStateCreateInfo cpsoCI;
                cpsoCI.PSODesc.Name = "CullPSO"; cpsoCI.PSODesc.PipelineType = PIPELINE_TYPE_COMPUTE;
                cpsoCI.PSODesc.ResourceLayout.Variables = cullVars; cpsoCI.PSODesc.ResourceLayout.NumVariables = 6u;
                cpsoCI.pCS = pCS;
                dev->CreateComputePipelineState(cpsoCI, &g_pCullPSO);
            }
        }
    }

    if (g_pCullPSO && g_pPSO) {
        IBufferView* visUAV = g_VisibleMatrixSSBO->GetDefaultView(BUFFER_VIEW_UNORDERED_ACCESS);
        IBufferView* visSRV = g_VisibleMatrixSSBO->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE);
        IBufferView* lodSRV = ((IBuffer*)ScryGraphics_GetLODGroupBuffer())->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE);
        g_pCullPSO->GetStaticVariableByName(SHADER_TYPE_COMPUTE, "b_outVisibleMatrices")->Set(visUAV);
        g_pCullPSO->GetStaticVariableByName(SHADER_TYPE_COMPUTE, "b_lodGroups")         ->Set(lodSRV);
        g_pPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "b_instances")              ->Set(visSRV);
        g_pPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "b_lodGroups")              ->Set(lodSRV);

        g_pPSO->CreateShaderResourceBinding(&g_pSRB, true);
        IBufferView* vbSRV = ((IBuffer*)ScryGraphics_GetGlobalVertexBuffer())->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE);
        g_pSRB->GetVariableByName(SHADER_TYPE_VERTEX, "b_vertices") ->Set(vbSRV);
        g_pSRB->GetVariableByName(SHADER_TYPE_VERTEX, "DrawParams") ->Set(g_pDrawParamsCB);

        g_pCullPSO->CreateShaderResourceBinding(&g_pCullSRB, true);
        IBufferView* matSRV  = g_RawMatrixSSBO->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE);
        IBufferView* meshSRV = g_EntityMeshIdBuffer->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE);
        IBufferView* iArgUAV = g_IndirectArgsBuffer->GetDefaultView(BUFFER_VIEW_UNORDERED_ACCESS);
        g_pCullSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "b_matrices")    ->Set(matSRV);
        g_pCullSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "b_entityLodIds")->Set(meshSRV);
        g_pCullSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "b_indirectArgs")->Set(iArgUAV);
        g_pCullSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "CullParams")    ->Set(g_pCullParamsCB);
    }

    id_ScryMeshData = InternalRegComp(world, "ScryMeshData", sizeof(ScryMeshData), alignof(ScryMeshData));
    id_ScryEntityIntent = InternalRegComp(world, "ScryEntityIntent", sizeof(ScryRendererIntent), alignof(ScryRendererIntent));
    id_ScryMaterial = InternalRegComp(world, "ScryMaterial", sizeof(ScryMaterial), alignof(ScryMaterial));

    {
        ecs_query_desc_t qd = {};
        qd.terms[0].id = (ecs_entity_t)id_ScryCamera;
        g_camera_query = ecs_query_init(world, &qd);

        ecs_query_desc_t rd = {};
        rd.terms[0].id = (ecs_entity_t)id_ScryWorldMatrix;
        rd.terms[1].id = (ecs_entity_t)id_ScryMeshData;
        rd.terms[2].id = (ecs_entity_t)id_ScryEntityIntent;
        rd.terms[3].id = (ecs_entity_t)id_ScryChunkCoord; rd.terms[3].inout = EcsIn;
        rd.terms[4].id = (ecs_entity_t)id_ScryChunkHash;  rd.terms[4].inout = EcsIn;
        
        rd.order_by = (ecs_entity_t)id_ScryChunkHash;
        rd.order_by_callback = compare_chunk_hashes;
        g_render_query = ecs_query_init(world, &rd);
    }

    {
        ecs_entity_desc_t ed = {};
        ed.name = "Pass_Cull";

        ecs_system_desc_t sd = {};
        sd.entity = ecs_entity_init(world, &ed);
        sd.query.terms[0].id = (ecs_entity_t)id_ScryCamera; sd.query.terms[0].inout = EcsIn;
        sd.callback = PassCullCallback;
        sd.phase = (ecs_entity_t)ScryPhase_Resolve;
        sd.multi_threaded = false;
        ecs_system_init(world, &sd);
    }

    {
        ecs_entity_desc_t ed = {};
        ed.name = "Pass_Opaque";

        ecs_system_desc_t sd = {};
        sd.entity = ecs_entity_init(world, &ed);
        sd.query.terms[0].id = (ecs_entity_t)id_ScryCamera; sd.query.terms[0].inout = EcsIn;
        sd.callback = PassOpaqueCallback;
        sd.phase = (ecs_entity_t)ScryPhase_Resolve;
        sd.multi_threaded = false;
        ecs_system_init(world, &sd);
    }

    {
        ecs_entity_desc_t ed = {};
        ed.name = "Pass_Cleanup";

        ecs_system_desc_t sd = {};
        sd.entity = ecs_entity_init(world, &ed);
        sd.callback = PassCleanupCallback;
        sd.phase = (ecs_entity_t)ScryPhase_Cleanup;
        sd.multi_threaded = false;
        ecs_system_init(world, &sd);
    }
}

void ScryRenderer_Shutdown(void) {
    if (g_render_query) ecs_query_fini(g_render_query);
    if (g_camera_query) ecs_query_fini(g_camera_query);
    g_pCullSRB.Release(); g_pCullPSO.Release();
    g_pSRB.Release(); g_pPSO.Release();
    g_IndirectArgsBuffer.Release(); g_VisibleMatrixSSBO.Release();
    g_EntityMeshIdBuffer.Release();
    g_pCullParamsCB.Release(); g_RawMatrixSSBO.Release();
    g_pDrawParamsCB.Release();
}

} // extern "C"
