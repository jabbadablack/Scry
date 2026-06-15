#include <engine/renderer.hpp>
#include <engine/transform.hpp>
#include <engine/camera.hpp>
#include <engine/pipeline.hpp>
#include <engine/math.hpp>
#include "graphics_internal.hpp"

#include <libassert/assert.hpp>
#include <cstring>
#include <cstdio>

using namespace Diligent;

namespace Engine {
namespace Renderer {

ecs_entity_t id_MeshInstance = 0;
ecs_entity_t id_EntityIntent = 0;

EntityState* g_MappedStateBuffer  = nullptr;
uint32_t*    g_MappedIntentBuffer = nullptr;

static RefCntAutoPtr<IBuffer>             g_StateBuffer;
static RefCntAutoPtr<IBuffer>             g_IntentBuffer;
static RefCntAutoPtr<IBuffer>             g_IndirectArgsBuffer;
static RefCntAutoPtr<IBuffer>             g_ViewProjCB;

static RefCntAutoPtr<IPipelineState>      g_pso;
static RefCntAutoPtr<IShaderResourceBinding> g_srb;

static RefCntAutoPtr<IPipelineState>      g_ComputePSO;
static RefCntAutoPtr<IShaderResourceBinding> g_ComputeSRB;

// ── Compute Shader: Culling & Indirect Args ───────────────────────────────────
static const char* k_cs_src = R"HLSL(
struct EntityState {
    float4x4 transform;
    uint4    mesh_data; // x: mesh_id
};

struct DrawIndexedIndirectCommand {
    uint IndexCount;
    uint InstanceCount;
    uint FirstIndexLocation;
    int  BaseVertex;
    uint FirstInstanceLocation;
};

StructuredBuffer<EntityState> State       : register(t0);
StructuredBuffer<uint>        Intent      : register(t1);
RWStructuredBuffer<DrawIndexedIndirectCommand> IndirectArgs : register(u0);

[numthreads(64, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    if (DTid.x >= 10000) return;

    uint intent = Intent[DTid.x];
    if ((intent & 1) && !(intent & 2)) // INTENT_VISIBLE && !INTENT_DESTROYED
    {
        uint mesh_id = State[DTid.x].mesh_data.x;
        if (mesh_id < 256)
        {
            InterlockedAdd(IndirectArgs[mesh_id].InstanceCount, 1);
        }
    }
}
)HLSL";

// ── Pull-Model Vertex Shader ──────────────────────────────────────────────────
static const char* k_vs_src = R"HLSL(
cbuffer cbViewProj : register(b0, space0)
{
    row_major float4x4 g_ViewProj;
};

struct EntityState {
    float4x4 transform;
    uint4    mesh_data; // x: mesh_id
};

StructuredBuffer<EntityState> g_States : register(t0);

struct VSIn
{
    float3 Pos    : ATTRIB0;
    float3 Normal : ATTRIB1;
    float2 UV     : ATTRIB2;
};

struct PSIn
{
    float4 Pos    : SV_POSITION;
    float3 Normal : NORMAL;
    float2 UV     : TEXCOORD;
};

void main(in VSIn vsin, in uint InstanceID : SV_InstanceID, out PSIn psout)
{
    float4x4 World = transpose(g_States[InstanceID].transform);
    float4 worldPos = mul(float4(vsin.Pos, 1.0), World);
    psout.Pos       = mul(worldPos, g_ViewProj);
    psout.Normal    = normalize(mul(float4(vsin.Normal, 0.0), World).xyz);
    psout.UV        = vsin.UV;
}
)HLSL";

static const char* k_ps_src = R"HLSL(
struct PSIn
{
    float4 Pos    : SV_POSITION;
    float3 Normal : NORMAL;
    float2 UV     : TEXCOORD;
};

float4 main(in PSIn psin) : SV_TARGET
{
    float3 light = normalize(float3(0.5, 1.0, -0.5));
    float  diff  = saturate(dot(normalize(psin.Normal), light)) * 0.85 + 0.15;
    return float4(diff, diff, diff, 1.0);
}
)HLSL";

// ── PSO creation ──────────────────────────────────────────────────────────────

static bool CreatePSOs(IRenderDevice* device, ISwapChain* swap_chain) {
    // Graphics PSO
    {
        RefCntAutoPtr<IShader> vs, ps;
        {
            ShaderCreateInfo sci;
            sci.SourceLanguage  = SHADER_SOURCE_LANGUAGE_HLSL;
            sci.Desc.ShaderType = SHADER_TYPE_VERTEX;
            sci.Desc.Name       = "MeshVS";
            sci.Source          = k_vs_src;
            sci.HLSLVersion     = {5, 0};
            device->CreateShader(sci, &vs);
        }
        {
            ShaderCreateInfo sci;
            sci.SourceLanguage  = SHADER_SOURCE_LANGUAGE_HLSL;
            sci.Desc.ShaderType = SHADER_TYPE_PIXEL;
            sci.Desc.Name       = "MeshPS";
            sci.Source          = k_ps_src;
            sci.HLSLVersion     = {5, 0};
            device->CreateShader(sci, &ps);
        }

        LayoutElement layout[] = {
            LayoutElement{0, 0, 3, VT_FLOAT32, false},
            LayoutElement{1, 0, 3, VT_FLOAT32, false},
            LayoutElement{2, 0, 2, VT_FLOAT32, false},
        };

        GraphicsPipelineStateCreateInfo ci;
        ci.PSODesc.Name                                  = "MeshPSO";
        ci.PSODesc.PipelineType                          = PIPELINE_TYPE_GRAPHICS;
        ci.GraphicsPipeline.NumRenderTargets             = 1;
        ci.GraphicsPipeline.RTVFormats[0]                = swap_chain->GetDesc().ColorBufferFormat;
        ci.GraphicsPipeline.DSVFormat                    = swap_chain->GetDesc().DepthBufferFormat;
        ci.GraphicsPipeline.PrimitiveTopology            = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        ci.GraphicsPipeline.RasterizerDesc.CullMode      = CULL_MODE_BACK;
        ci.GraphicsPipeline.DepthStencilDesc.DepthEnable = true;
        ci.GraphicsPipeline.InputLayout.NumElements      = _countof(layout);
        ci.GraphicsPipeline.InputLayout.LayoutElements   = layout;
        ci.pVS = vs;
        ci.pPS = ps;

        ShaderResourceVariableDesc Vars[] = {
            {SHADER_TYPE_VERTEX, "g_States", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
            {SHADER_TYPE_VERTEX, "cbViewProj", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}
        };
        ci.PSODesc.ResourceLayout.Variables    = Vars;
        ci.PSODesc.ResourceLayout.NumVariables = _countof(Vars);

        device->CreateGraphicsPipelineState(ci, &g_pso);
        if (g_pso) {
            g_pso->CreateShaderResourceBinding(&g_srb, true);
            if (g_srb) {
                auto* states_var = g_srb->GetVariableByName(SHADER_TYPE_VERTEX, "g_States");
                if (states_var) states_var->Set(g_StateBuffer->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
                
                auto* vp_var = g_srb->GetVariableByName(SHADER_TYPE_VERTEX, "cbViewProj");
                if (vp_var) vp_var->Set(g_ViewProjCB);
            }
        }
    }

    // Compute PSO
    {
        RefCntAutoPtr<IShader> cs;
        ShaderCreateInfo sci;
        sci.SourceLanguage  = SHADER_SOURCE_LANGUAGE_HLSL;
        sci.Desc.ShaderType = SHADER_TYPE_COMPUTE;
        sci.Desc.Name       = "CullCS";
        sci.Source          = k_cs_src;
        sci.HLSLVersion     = {5, 0};
        device->CreateShader(sci, &cs);

        ComputePipelineStateCreateInfo ci;
        ci.PSODesc.Name         = "CullPSO";
        ci.PSODesc.PipelineType = PIPELINE_TYPE_COMPUTE;
        ci.pCS = cs;

        ShaderResourceVariableDesc Vars[] = {
            {SHADER_TYPE_COMPUTE, "State",        SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
            {SHADER_TYPE_COMPUTE, "Intent",       SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
            {SHADER_TYPE_COMPUTE, "IndirectArgs", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}
        };
        ci.PSODesc.ResourceLayout.Variables    = Vars;
        ci.PSODesc.ResourceLayout.NumVariables = _countof(Vars);

        device->CreateComputePipelineState(ci, &g_ComputePSO);
        if (g_ComputePSO) {
            g_ComputePSO->CreateShaderResourceBinding(&g_ComputeSRB, true);
            if (g_ComputeSRB) {
                auto* state_var = g_ComputeSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "State");
                if (state_var) state_var->Set(g_StateBuffer->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));

                auto* intent_var = g_ComputeSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "Intent");
                if (intent_var) intent_var->Set(g_IntentBuffer->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));

                auto* args_var = g_ComputeSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "IndirectArgs");
                if (args_var) args_var->Set(g_IndirectArgsBuffer->GetDefaultView(BUFFER_VIEW_UNORDERED_ACCESS));
            }
        }
    }

    return true;
}

// ── Init ──────────────────────────────────────────────────────────────────────

void Init(ecs_world_t* world) {
    DEBUG_ASSERT(world != nullptr);

    IRenderDevice*  device      = Graphics::GetDevice();
    ISwapChain*     swap_chain  = Graphics::GetSwapChain();

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

    // ── Allocate SSBOs ───────────────────────────────────────────────────────
    {
        BufferDesc bd;
        bd.Name           = "g_StateBuffer";
        bd.Usage          = USAGE_DYNAMIC;
        bd.BindFlags      = BIND_SHADER_RESOURCE;
        bd.CPUAccessFlags = CPU_ACCESS_WRITE;
        bd.Size           = MAX_ENTITIES * sizeof(EntityState);
        bd.ElementByteStride = sizeof(EntityState);
        bd.Mode           = BUFFER_MODE_STRUCTURED;
        device->CreateBuffer(bd, nullptr, &g_StateBuffer);

        bd.Name           = "g_IntentBuffer";
        bd.Size           = MAX_ENTITIES * sizeof(uint32_t);
        bd.ElementByteStride = sizeof(uint32_t);
        device->CreateBuffer(bd, nullptr, &g_IntentBuffer);

        bd.Name           = "g_IndirectArgsBuffer";
        bd.Usage          = USAGE_DEFAULT;
        bd.BindFlags      = BIND_UNORDERED_ACCESS | BIND_INDIRECT_DRAW_ARGS;
        bd.CPUAccessFlags = CPU_ACCESS_NONE;
        bd.Size           = MAX_MESHES * sizeof(uint32_t) * 5; 
        bd.ElementByteStride = sizeof(uint32_t) * 5;
        bd.Mode           = BUFFER_MODE_STRUCTURED;
        device->CreateBuffer(bd, nullptr, &g_IndirectArgsBuffer);

        bd.Name           = "g_ViewProjCB";
        bd.Usage          = USAGE_DYNAMIC;
        bd.BindFlags      = BIND_UNIFORM_BUFFER;
        bd.CPUAccessFlags = CPU_ACCESS_WRITE;
        bd.Size           = sizeof(Math::ScryMat4);
        bd.Mode           = BUFFER_MODE_UNDEFINED;
        bd.ElementByteStride = 0;
        device->CreateBuffer(bd, nullptr, &g_ViewProjCB);
    }

    CreatePSOs(device, swap_chain);

    // ── Map System (Phase_Intent) ────────────────────────────────────────────
    {
        ecs_entity_desc_t e = {};
        e.name = "MapSystem";
        ecs_entity_t sys_ent = ecs_entity_init(world, &e);
        ecs_add_pair(world, sys_ent, EcsDependsOn, Engine::Pipeline::Phase_Intent);
        
        ecs_system_desc_t s = {};
        s.entity = sys_ent;
        s.callback = [](ecs_iter_t* it) {
            (void)it;
            IDeviceContext* ctx = Graphics::GetContext();
            void* pState = nullptr;
            void* pIntent = nullptr;
            ctx->MapBuffer(g_StateBuffer, MAP_WRITE, MAP_FLAG_DISCARD, pState);
            ctx->MapBuffer(g_IntentBuffer, MAP_WRITE, MAP_FLAG_DISCARD, pIntent);
            g_MappedStateBuffer  = static_cast<EntityState*>(pState);
            g_MappedIntentBuffer = static_cast<uint32_t*>(pIntent);
        };
        ecs_system_init(world, &s);
    }

    // ── Render System (Phase_React) ──────────────────────────────────────────
    {
        ecs_entity_desc_t e = {};
        e.name = "RenderSystem";
        ecs_entity_t sys_ent = ecs_entity_init(world, &e);
        ecs_add_pair(world, sys_ent, EcsDependsOn, Engine::Pipeline::Phase_React);

        ecs_system_desc_t s = {};
        s.entity = sys_ent;
        s.query.terms[0].id = Engine::Camera::id_Camera;
        s.query.terms[0].inout = EcsIn;
        s.callback = [](ecs_iter_t* it) {
            (void)it;
            const Engine::Camera::Camera* cam = ecs_field(it, Engine::Camera::Camera, 0);
            IDeviceContext* ctx = Graphics::GetContext();

            ctx->UnmapBuffer(g_StateBuffer, MAP_WRITE);
            ctx->UnmapBuffer(g_IntentBuffer, MAP_WRITE);
            g_MappedStateBuffer  = nullptr;
            g_MappedIntentBuffer = nullptr;

            // Update ViewProj from Camera
            if (cam) {
                void* pVP = nullptr;
                ctx->MapBuffer(g_ViewProjCB, MAP_WRITE, MAP_FLAG_DISCARD, pVP);
                if (pVP) {
                    *static_cast<Math::ScryMat4*>(pVP) = cam[0].view_proj;
                    ctx->UnmapBuffer(g_ViewProjCB, MAP_WRITE);
                }
            }

            // Zero out IndirectArgs InstanceCount using UpdateBuffer
            static uint32_t zeros[MAX_MESHES * 5] = {0};
            ctx->UpdateBuffer(g_IndirectArgsBuffer, 0, sizeof(zeros), zeros, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

            StateTransitionDesc barriers[] = {
                {g_IndirectArgsBuffer, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_UNORDERED_ACCESS, STATE_TRANSITION_FLAG_UPDATE_STATE}
            };
            ctx->TransitionResourceStates(_countof(barriers), barriers);

            ctx->SetPipelineState(g_ComputePSO);
            ctx->CommitShaderResources(g_ComputeSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            ctx->DispatchCompute(DispatchComputeAttribs(MAX_ENTITIES / 64 + 1, 1, 1));

            barriers[0].NewState = RESOURCE_STATE_INDIRECT_ARGUMENT;
            ctx->TransitionResourceStates(1, barriers);

            ctx->SetPipelineState(g_pso);
            ctx->CommitShaderResources(g_srb, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            
            for (uint32_t m = 0; m < 10; ++m) { 
                const Graphics::MeshBuffers* mesh = Graphics::GetMesh(m); 
                if (mesh) {
                    IBuffer* vbufs[] = { mesh->vertices };
                    Uint64 offsets[] = { 0 };
                    ctx->SetVertexBuffers(0, 1, vbufs, offsets, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
                    ctx->SetIndexBuffer(mesh->indices, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

                    DrawIndexedIndirectAttribs draw;
                    draw.pAttribsBuffer = g_IndirectArgsBuffer;
                    draw.DrawArgsOffset = m * sizeof(uint32_t) * 5;
                    draw.IndexType = VT_UINT32;
                    ctx->DrawIndexedIndirect(draw);
                }
            }
        };
        ecs_system_init(world, &s);
    }
}

void Shutdown() {
    g_ComputeSRB.Release(); g_ComputePSO.Release();
    g_srb.Release(); g_pso.Release();
    g_StateBuffer.Release(); g_IntentBuffer.Release(); g_IndirectArgsBuffer.Release();
    g_ViewProjCB.Release();
}

} // namespace Renderer
} // namespace Engine
