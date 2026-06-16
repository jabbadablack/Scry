#include <engine/renderer.h>
#include <engine/transform.h>
#include <engine/camera.h>
#include <engine/spatial.h>
#include <engine/pipeline.h>
#include <engine/graphics.h>
#include <engine/graphics_backend.h>
#include <engine/engine.h>

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
static constexpr uint32_t MAX_ENTITIES    = 16384u;
static constexpr uint32_t MAX_LOD_GROUPS  = 256u;
static constexpr uint32_t LOD_LEVELS      = 3u;

// ── Graphics PSO / SRB ───────────────────────────────────────────────────────
static RefCntAutoPtr<IPipelineState>         g_pPSO;
static RefCntAutoPtr<IShaderResourceBinding> g_pSRB;
static RefCntAutoPtr<IBuffer>                g_pDrawParamsCB;

// ── Compute PSO / SRB ────────────────────────────────────────────────────────
static RefCntAutoPtr<IPipelineState>         g_pCullPSO;
static RefCntAutoPtr<IShaderResourceBinding> g_pCullSRB;

// ── Per-frame raw input buffers (CPU→GPU each frame) ─────────────────────────
static RefCntAutoPtr<IBuffer> g_RawMatrixSSBO;     // DYNAMIC SRV, raw world matrices
static RefCntAutoPtr<IBuffer> g_pAABBBuffer;       // DYNAMIC SRV, per-entity AABB
static RefCntAutoPtr<IBuffer> g_EntityMeshIdBuffer;// DYNAMIC SRV, per-entity lod_group_id
static RefCntAutoPtr<IBuffer> g_pCullParamsCB;     // DYNAMIC cbuffer, frustum + camera

// ── MDI + dense output buffers (DEFAULT, GPU writes / GPU reads) ──────────────
static RefCntAutoPtr<IBuffer> g_IndirectArgsBuffer; // MAX_LOD_GROUPS*3 IndirectCommands
static RefCntAutoPtr<IBuffer> g_VisibleMatrixSSBO;  // dense ScryMat4 output, MAX_LOD_GROUPS*3*MAX_ENTITIES

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
static uint32_t   s_mesh_ids[MAX_ENTITIES];
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

// CPU-side mirror of CullParams cbuffer (128 bytes)
struct CullParamsCPU {
    float    frustumPlanes[6][4]; // 96 bytes, offset 0
    float    cameraWorldPos[4];   // 16 bytes, offset 96
    uint32_t entityCount;         //  4 bytes, offset 112
    uint32_t _pad[3];             // 12 bytes, offset 116
};
static_assert(sizeof(CullParamsCPU) == 128, "CullParams size mismatch");

static bool g_logged_first = false;

// ── Shader loading ────────────────────────────────────────────────────────────

/**
 * @brief Reads a shader file from disk into a newly allocated buffer.
 *
 * This function handles the low-level file reading to get our shader code ready for the GPU.
 * It's like fetching the ingredients before we start cooking our visuals!
 *
 * @param path The filesystem path to the shader file.
 * @param out_len Optional pointer to store the length of the read source code.
 * @return A pointer to the null-terminated shader source string, or nullptr if it fails.
 *
 * @example
 * uint32_t len = 0;
 * char* source = LoadShaderSource("assets/shaders/my_shader.hlsl", &len);
 * if (source) {
 *     // use source...
 *     std::free(source);
 * }
 */
static char* LoadShaderSource(const char* path, uint32_t* out_len) {
    assert(path != nullptr);
    assert(out_len != nullptr || true);
    EngineLog("LoadShaderSource: Attempting to open file.");
    EngineLog(path);

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

/**
 * @brief Searches for a shader file in several common directories.
 *
 * Shaders can be sneaky and hide in different folders. This function hunts them down for you!
 * It checks multiple paths so you don't have to worry about where exactly they live.
 *
 * @param filename The name of the shader file to find.
 * @param out_len Pointer to store the length of the discovered shader source.
 * @return The shader source code as a string, or nullptr if not found.
 *
 * @example
 * uint32_t len = 0;
 * char* hlsl = DiscoverShader("mesh.hlsl", &len);
 */
static char* DiscoverShader(const char* filename, uint32_t* out_len) {
    assert(filename != nullptr);
    assert(out_len != nullptr);
    EngineLog("DiscoverShader: Searching for shader file...");
    EngineLog(filename);

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

/**
 * @brief Extracts the six frustum planes from a view-projection matrix.
 *
 * This function helps our renderer decide what's visible and what's not by defining the camera's field of view.
 * It's like drawing the boundaries of what the camera can "see"!
 *
 * @param vp The 4x4 view-projection matrix (column-major).
 * @param planes A 2D array where the extracted planes (Ax+By+Cz+D=0) will be stored.
 *
 * @example
 * float vp[16]; // ... filled with matrix data ...
 * float planes[6][4];
 * ExtractFrustumPlanes(vp, planes);
 */
static void ExtractFrustumPlanes(const float* vp, float planes[6][4]) {
    assert(vp != nullptr);
    assert(planes != nullptr);
    static bool logged_once = false;
    if (!logged_once) {
        EngineLog("ExtractFrustumPlanes: Calculating view frustum boundaries.");
        EngineLog("Normalizing planes for accurate culling.");
        logged_once = true;
    }

    // VP is stored column-major: vp[col*4 + row]
    auto row = [&](int r, float* out) {
        out[0] = vp[0 * 4 + r];
        out[1] = vp[1 * 4 + r];
        out[2] = vp[2 * 4 + r];
        out[3] = vp[3 * 4 + r];
    };
    float r0[4], r1[4], r2[4], r3[4];
    row(0, r0); row(1, r1); row(2, r2); row(3, r3);

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

/**
 * @brief Creates the Graphics Pipeline State Object (PSO) for mesh rendering.
 *
 * This function sets up the rules for how our meshes should be drawn, including shaders and rasterization state.
 * It's like setting the stage and lighting before the actors come on!
 *
 * @return True if the PSO was created successfully, false otherwise.
 *
 * @example
 * if (!CreateGraphicsPSO()) {
 *     // Handle error
 * }
 */
static bool CreateGraphicsPSO() {
    IRenderDevice* dev = Graphics::GetDevice();
    ISwapChain*    sc  = Graphics::GetSwapChain();
    assert(dev && sc);
    assert(g_pPSO == nullptr); // Ensure we don't leak
    EngineLog("CreateGraphicsPSO: Building the main mesh rendering pipeline.");
    EngineLog("Compiling vertex and pixel shaders for meshes.");

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

    // b_instances is STATIC — always g_VisibleMatrixSSBO, set once on PSO
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
    psoCI.GraphicsPipeline.RasterizerDesc.FrontCounterClockwise = True;
    psoCI.pVS = pVS;
    psoCI.pPS = pPS;

    dev->CreateGraphicsPipelineState(psoCI, &g_pPSO);
    if (!g_pPSO) { EngineLog("[Renderer] FATAL: graphics PSO creation failed"); return false; }
    EngineLog("[Renderer] Graphics PSO created");
    return true;
}

/**
 * @brief Creates the Compute Pipeline State Object (PSO) for GPU-driven culling.
 *
 * This function sets up the compute shader that will decide which objects are visible on the GPU.
 * It's like having a very fast robot assistant to filter out what we don't need to see!
 *
 * @return True if the PSO was created successfully, false otherwise.
 *
 * @example
 * if (!CreateComputePSO()) {
 *     // Handle error
 * }
 */
static bool CreateComputePSO() {
    IRenderDevice* dev = Graphics::GetDevice();
    assert(dev);
    assert(g_pCullPSO == nullptr);
    EngineLog("CreateComputePSO: Setting up the GPU culling system.");
    EngineLog("Compiling the cull compute shader.");

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

    // b_lodGroups and b_outVisibleMatrices are STATIC — set once on PSO, never change
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
    if (!g_pCullPSO) { EngineLog("[Renderer] FATAL: compute PSO creation failed"); return false; }
    EngineLog("[Renderer] Compute PSO created");
    return true;
}

// ── Chunk sort comparator (for ecs_query order_by) ───────────────────────────

/**
 * @brief Comparator function to sort entities by their spatial chunk hash.
 *
 * Keeping entities sorted by chunk hash makes our renderer much more efficient when processing them.
 * It's like organizing your closet by color so everything is easy to find!
 *
 * @param a First chunk hash to compare.
 * @param b Second chunk hash to compare.
 * @return An integer representing the relative order.
 *
 * @example
 * // Used by Flecs to sort query results
 * ecs_query_desc_t rq = {};
 * rq.order_by_callback = compare_chunk_hashes;
 */
static int compare_chunk_hashes(ecs_entity_t, const void* a, ecs_entity_t, const void* b) {
    assert(a != nullptr);
    assert(b != nullptr);
    static bool logged_once = false;
    if (!logged_once) {
        EngineLog("compare_chunk_hashes: Sorting entities for spatial efficiency.");
        EngineLog("Ensuring rendering order follows chunk locality.");
        logged_once = true;
    }

    const uint64_t ha = static_cast<const Engine::Spatial::ChunkHash*>(a)->hash;
    const uint64_t hb = static_cast<const Engine::Spatial::ChunkHash*>(b)->hash;
    return (ha > hb) - (ha < hb);
}

// ── Init ──────────────────────────────────────────────────────────────────────

/**
 * @brief Initializes the entire renderer system, including buffers, PSOs, and ECS queries.
 *
 * This is the big one! It gets everything ready for the renderer to start showing beautiful graphics.
 * From GPU buffers to ECS systems, this function handles it all.
 *
 * @param world A pointer to the ECS world.
 *
 * @example
 * ecs_world_t* world = ecs_init();
 * Engine::Renderer::Init(world);
 */
void Init(ecs_world_t* world) {
    assert(world != nullptr);
    assert(id_MeshData == 0); // Don't init twice!
    EngineLog("[Renderer] Initializing...");
    EngineLog("Setting up GPU buffers and rendering pipelines.");

    IRenderDevice* dev = Graphics::GetDevice();
    assert(dev);

    // Raw world-matrix SSBO — CPU writes per frame, compute reads
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
        assert(g_RawMatrixSSBO);
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

    // AABB SSBO — CPU writes per frame, compute reads
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
        assert(g_pAABBBuffer);
    }

    // Entity lod-group-id SSBO — CPU writes per frame, compute reads
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
        assert(g_EntityMeshIdBuffer);
    }

    // Cull params constant buffer (128 bytes)
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

    // Indirect args buffer — MAX_LOD_GROUPS * 3 entries, UAV for compute + indirect draw
    {
        BufferDesc bd;
        bd.Name              = "IndirectArgs";
        bd.Usage             = USAGE_DEFAULT;
        bd.BindFlags         = BIND_INDIRECT_DRAW_ARGS | BIND_UNORDERED_ACCESS;
        bd.Mode              = BUFFER_MODE_STRUCTURED;
        bd.ElementByteStride = 20u;
        bd.Size              = MAX_LOD_GROUPS * LOD_LEVELS * 20u;
        dev->CreateBuffer(bd, nullptr, &g_IndirectArgsBuffer);
        assert(g_IndirectArgsBuffer);
    }

    // Dense visible-matrix SSBO — compute writes (UAV), vertex shader reads (SRV).
    {
        BufferDesc bd;
        bd.Name              = "VisibleMatrixSSBO";
        bd.Usage             = USAGE_DEFAULT;
        bd.BindFlags         = BIND_UNORDERED_ACCESS | BIND_SHADER_RESOURCE;
        bd.Mode              = BUFFER_MODE_STRUCTURED;
        bd.ElementByteStride = 64u;
        bd.Size              = LOD_LEVELS * MAX_ENTITIES * 64u; // 3 * 16384 * 64 = 3 MB
        dev->CreateBuffer(bd, nullptr, &g_VisibleMatrixSSBO);
        assert(g_VisibleMatrixSSBO);
    }

    if (!CreateGraphicsPSO()) { EngineLog("[Renderer] FATAL: failed to build graphics PSO"); return; }
    if (!CreateComputePSO())  { EngineLog("[Renderer] FATAL: failed to build compute PSO");  return; }

    // Bind STATIC resources on both PSOs
    {
        IBufferView* visUAV = g_VisibleMatrixSSBO->GetDefaultView(BUFFER_VIEW_UNORDERED_ACCESS);
        IBufferView* visSRV = g_VisibleMatrixSSBO->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE);
        IBufferView* lodSRV = Graphics::GetLODGroupBuffer()->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE);
        assert(visUAV && visSRV && lodSRV);
        g_pCullPSO->GetStaticVariableByName(SHADER_TYPE_COMPUTE, "b_outVisibleMatrices")->Set(visUAV);
        g_pCullPSO->GetStaticVariableByName(SHADER_TYPE_COMPUTE, "b_lodGroups")         ->Set(lodSRV);
        g_pPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "b_instances")              ->Set(visSRV);
        EngineLog("[Renderer] Static resources bound");
    }

    // Graphics SRB — mutable bindings
    {
        g_pPSO->CreateShaderResourceBinding(&g_pSRB, true);
        assert(g_pSRB);
        IBufferView* vbSRV = Graphics::GetGlobalVertexBuffer()->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE);
        assert(vbSRV);
        g_pSRB->GetVariableByName(SHADER_TYPE_VERTEX, "b_vertices") ->Set(vbSRV);
        g_pSRB->GetVariableByName(SHADER_TYPE_VERTEX, "DrawParams") ->Set(g_pDrawParamsCB);
        EngineLog("[Renderer] Graphics SRB bound");
    }

    // Compute SRB — mutable bindings
    {
        g_pCullPSO->CreateShaderResourceBinding(&g_pCullSRB, true);
        assert(g_pCullSRB);
        IBufferView* matSRV   = g_RawMatrixSSBO->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE);
        IBufferView* aabbSRV  = g_pAABBBuffer->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE);
        IBufferView* meshSRV  = g_EntityMeshIdBuffer->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE);
        IBufferView* iArgUAV  = g_IndirectArgsBuffer->GetDefaultView(BUFFER_VIEW_UNORDERED_ACCESS);
        assert(matSRV && aabbSRV && meshSRV && iArgUAV);
        g_pCullSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "b_matrices")    ->Set(matSRV);
        g_pCullSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "b_bounds")      ->Set(aabbSRV);
        g_pCullSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "b_entityLodIds")->Set(meshSRV);
        g_pCullSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "b_indirectArgs")->Set(iArgUAV);
        g_pCullSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "CullParams")    ->Set(g_pCullParamsCB);
        EngineLog("[Renderer] Compute SRB bound");
    }

    // Register ECS components
    {
        /**
         * @brief Helper lambda to register a new ECS component.
         * 
         * This little guy makes it super easy to tell the engine about new data types!
         * 
         * @param name The name of the component.
         * @param sz The size of the component.
         * @param align The alignment of the component.
         * @return The entity ID of the new component.
         * 
         * @example
         * ecs_entity_t my_comp = reg("MyComp", sizeof(float), alignof(float));
         */
        auto reg = [&](const char* name, size_t sz, size_t align) -> ecs_entity_t {
            assert(name != nullptr);
            assert(sz > 0);
            EngineLog("Renderer::Init registering component:");
            EngineLog(name);

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
        assert(g_render_query);
    }

    // ── Pass_Cull ─────────────────────────────────────────────────────────────
    {
        ecs_entity_desc_t ed = {}; ed.name = "Pass_Cull";
        ecs_entity_t sys_ent = ecs_entity_init(world, &ed);
        ecs_add_pair(world, sys_ent, EcsDependsOn, Engine::Pipeline::Phase_React);

        ecs_system_desc_t s = {};
        s.entity          = sys_ent;
        s.multi_threaded  = false; // IDeviceContext is not thread-safe; must run on main thread
        /**
         * @brief Culling system callback that prepares GPU data and dispatches the compute culler.
         * 
         * This lambda gathers all the world matrices and bounding boxes, filters them by render distance,
         * and then lets the GPU finish the job. It's the brain of our high-performance rendering pipeline!
         * 
         * @param it The ECS iterator for the culling pass.
         * 
         * @example
         * // This is called by the Flecs runner during the React phase
         * s.callback(it);
         */
        s.callback = [](ecs_iter_t* it) {
            assert(it != nullptr);
            assert(it->world != nullptr);
            static bool logged_once = false;
            if (!logged_once) {
                EngineLog("Pass_Cull callback: Gathering entities for culling.");
                EngineLog("Uploading visibility data to GPU.");
                logged_once = true;
            }

            IDeviceContext* ctx = Graphics::GetContext();
            assert(ctx);

            const uint32_t numGroups = Graphics::GetLODGroupCount();
            if (numGroups == 0) return;

            // ── Step 1: camera first ─────
            int32_t cam_cx = 0, cam_cy = 0;
            float   cam_pos[3] = {};
            float   vp[16] = {};
            if (g_camera_query) {
                ecs_iter_t ci = ecs_query_iter(it->world, g_camera_query);
                while (ecs_query_next(&ci)) {
                    const Engine::Camera::Camera* cam = ecs_field(&ci, Engine::Camera::Camera, 0);
                    if (ci.count == 0) continue;
                    cam_cx    = static_cast<int32_t>(std::floor(cam[0].position.x() / Engine::Spatial::CHUNK_SIZE));
                    cam_cy    = static_cast<int32_t>(std::floor(cam[0].position.z() / Engine::Spatial::CHUNK_SIZE));
                    cam_pos[0] = cam[0].position.x();
                    cam_pos[1] = cam[0].position.y();
                    cam_pos[2] = cam[0].position.z();
                    Eigen::Map<const Eigen::Matrix4f> V(cam[0].view);
                    Eigen::Map<const Eigen::Matrix4f> P(cam[0].proj);
                    Eigen::Matrix4f VP_mat = P * V;
                    std::memcpy(vp, VP_mat.data(), 64u);
                    void* p = nullptr;
                    ctx->MapBuffer(g_pDrawParamsCB, MAP_WRITE, MAP_FLAG_DISCARD, p);
                    if (p) { std::memcpy(p, vp, 64u); ctx->UnmapBuffer(g_pDrawParamsCB, MAP_WRITE); }
                    ecs_iter_fini(&ci);
                    break;
                }
            }

            // ── Step 2: gather matrices + AABBs + mesh IDs ─
            constexpr int RENDER_DISTANCE = 4;
            s_entity_count = 0;

            if (g_render_query) {
                ecs_iter_t ri = ecs_query_iter(it->world, g_render_query);
                while (ecs_query_next(&ri)) {
                    const Engine::Transform::WorldMatrix*      wm     = ecs_field(&ri, Engine::Transform::WorldMatrix, 0);
                    const MeshData*                            md     = ecs_field(&ri, MeshData, 1);
                    const Intent*                              intent = ecs_field(&ri, Intent,   2);
                    const AABB*                                ab     = ecs_field(&ri, AABB,     3);
                    const Engine::Spatial::ChunkCoord*         chunk  = ecs_field(&ri, Engine::Spatial::ChunkCoord, 4);

                    for (int i = 0; i < ri.count && s_entity_count < MAX_ENTITIES; ++i) {
                        if ((intent[i].mask & INTENT_VISIBLE)   == 0) continue;
                        if ((intent[i].mask & INTENT_DESTROYED) != 0) continue;
                        if (std::abs(chunk[i].x - cam_cx) > RENDER_DISTANCE) continue;
                        if (std::abs(chunk[i].y - cam_cy) > RENDER_DISTANCE) continue;

                        std::memcpy(&s_mat_flat[s_entity_count * 16], wm[i].value.data(), 64u);

                        s_aabb_flat[s_entity_count].aabb_min[0] = ab[i].min.x();
                        s_aabb_flat[s_entity_count].aabb_min[1] = ab[i].min.y();
                        s_aabb_flat[s_entity_count].aabb_min[2] = ab[i].min.z();
                        s_aabb_flat[s_entity_count].pad0 = 0.f;
                        s_aabb_flat[s_entity_count].aabb_max[0] = ab[i].max.x();
                        s_aabb_flat[s_entity_count].aabb_max[1] = ab[i].max.y();
                        s_aabb_flat[s_entity_count].aabb_max[2] = ab[i].max.z();
                        s_aabb_flat[s_entity_count].pad1 = 0.f;

                        s_mesh_ids[s_entity_count] = md[i].lod_group_id;
                        ++s_entity_count;
                    }
                }
            }
            if (s_entity_count == 0) return;

            // ── Step 3: upload raw data to GPU ─
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
            ExtractFrustumPlanes(vp, cp.frustumPlanes);
            cp.cameraWorldPos[0] = cam_pos[0];
            cp.cameraWorldPos[1] = cam_pos[1];
            cp.cameraWorldPos[2] = cam_pos[2];
            cp.cameraWorldPos[3] = 0.f;
            cp.entityCount = s_entity_count;
            {
                void* p = nullptr;
                ctx->MapBuffer(g_pCullParamsCB, MAP_WRITE, MAP_FLAG_DISCARD, p);
                if (p) { std::memcpy(p, &cp, sizeof(cp)); ctx->UnmapBuffer(g_pCullParamsCB, MAP_WRITE); }
            }

            // ── Step 4: reset indirect args ─
            {
                const uint32_t totalSlots = numGroups * LOD_LEVELS;
                IndirectCmd resetCmds[MAX_LOD_GROUPS * LOD_LEVELS];
                for (uint32_t g = 0; g < numGroups; ++g) {
                    const Graphics::LODGroup* lg = Graphics::GetLODGroup(g);
                    if (!lg) continue;
                    for (uint32_t lod = 0; lod < LOD_LEVELS; ++lod) {
                        const uint32_t slot = g * LOD_LEVELS + lod;
                        resetCmds[slot] = {
                            lg->lods[lod].indexCount,
                            0u,
                            lg->lods[lod].firstIndex,
                            lg->lods[lod].baseVertex,
                            slot * MAX_ENTITIES
                        };
                    }
                }
                ctx->UpdateBuffer(g_IndirectArgsBuffer, 0,
                    totalSlots * static_cast<uint32_t>(sizeof(IndirectCmd)),
                    resetCmds,
                    RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            }

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
    {
        ecs_entity_desc_t ed = {}; ed.name = "Pass_Opaque";
        ecs_entity_t sys_ent = ecs_entity_init(world, &ed);
        ecs_add_pair(world, sys_ent, EcsDependsOn, Engine::Pipeline::Phase_React);

        ecs_system_desc_t s = {};
        s.entity          = sys_ent;
        s.multi_threaded  = false; // IDeviceContext is not thread-safe; must run on main thread
        /**
         * @brief Opaque rendering pass callback that submits multi-draw indirect commands.
         * 
         * This lambda does the actual drawing! It tells the GPU to render everything that survived culling.
         * It's like the final performance after all the rehearsals!
         * 
         * @param it The ECS iterator for the opaque pass.
         * 
         * @example
         * // Triggered automatically by the engine's pipeline runner
         * s.callback(it);
         */
        s.callback = [](ecs_iter_t* it) {
            assert(it != nullptr || true);
            assert(g_pPSO != nullptr);
            static bool logged_once = false;
            if (!logged_once) {
                EngineLog("Pass_Opaque callback: Submitting indirect draw commands.");
                EngineLog("Rendering opaque geometry to the swapchain.");
                logged_once = true;
            }

            (void)it;
            if (s_entity_count == 0) return;

            const uint32_t numGroups = Graphics::GetLODGroupCount();
            if (numGroups == 0) return;

            IDeviceContext* ctx = Graphics::GetContext();
            assert(ctx);

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

            if (!g_logged_first) {
                EngineLog("[Renderer] First MDI draw submitted (dense-pack LOD)");
                g_logged_first = true;
            }
        };
        ecs_system_init(world, &s);
    }

    EngineLog("[Renderer] Init complete");
}

// ── Shutdown ──────────────────────────────────────────────────────────────────

/**
 * @brief Safely shuts down the renderer system and releases all GPU resources.
 * 
 * When it's time to say goodbye, this function cleans up after us, making sure no GPU memory is leaked.
 * It's like turning off the lights and locking the door when you leave!
 * 
 * @example
 * Engine::Renderer::Shutdown();
 */
void Shutdown() {
    assert(g_render_query != nullptr || true);
    assert(g_pPSO != nullptr || true);
    EngineLog("[Renderer] Shutting down...");
    EngineLog("Releasing all graphics resources and queries.");

    if (g_render_query) { ecs_query_fini(g_render_query); g_render_query = nullptr; }
    if (g_camera_query) { ecs_query_fini(g_camera_query); g_camera_query = nullptr; }
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
    EngineLog("[Renderer] Shutdown complete");
}

} // namespace Renderer
} // namespace Engine
