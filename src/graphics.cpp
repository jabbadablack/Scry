#include <engine/graphics.h>
#include <engine/graphics_backend.h>
#include <engine/CookedAsset.h>

#include "Graphics/GraphicsEngineVulkan/interface/EngineFactoryVk.h"
#include "Platforms/Win32/interface/Win32NativeWindow.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#if defined(_WIN32)
#  define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__linux__)
#  define GLFW_EXPOSE_NATIVE_X11
#endif
#include <GLFW/glfw3native.h>

#include <cassert>
#include <cstdio>
#include <cstring>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#else
#  include <sys/mman.h>
#  include <sys/stat.h>
#  include <fcntl.h>
#  include <unistd.h>
#endif

using namespace Diligent;

namespace Engine {
namespace Graphics {

// ── Diligent globals ──────────────────────────────────────────────────────────
static RefCntAutoPtr<IRenderDevice>  g_pDevice;
static RefCntAutoPtr<IDeviceContext> g_pContext;
static RefCntAutoPtr<ISwapChain>     g_pSwapChain;

IRenderDevice*  GetDevice()    { return g_pDevice;    }
IDeviceContext* GetContext()   { return g_pContext;    }
ISwapChain*     GetSwapChain() { return g_pSwapChain; }

// ── Global megabuffers ────────────────────────────────────────────────────────
static constexpr uint32_t GLOBAL_VB_SIZE = 32u * 1024u * 1024u; // 32 MB
static constexpr uint32_t GLOBAL_IB_SIZE = 16u * 1024u * 1024u; // 16 MB

static RefCntAutoPtr<IBuffer> g_GlobalVertexBuffer;
static RefCntAutoPtr<IBuffer> g_GlobalIndexBuffer;

static uint32_t g_VertexOffset = 0; // next free vertex slot (in vertices, not bytes)
static uint32_t g_IndexOffset  = 0; // next free index  slot (in indices,  not bytes)

IBuffer* GetGlobalVertexBuffer() { return g_GlobalVertexBuffer; }
IBuffer* GetGlobalIndexBuffer()  { return g_GlobalIndexBuffer;  }

// ── LOD group storage ─────────────────────────────────────────────────────────
static constexpr uint32_t MAX_LOD_GROUPS = 256u;

// GPU-side SSBO layout for a LOD group: 3 × MeshLOD = 3 × 16 bytes = 48 bytes per entry.
// Matches the HLSL StructuredBuffer<LODGroupGPU> with stride=48.
struct LODGroupGPU {
    struct MeshLODGPU {
        uint32_t indexCount;
        uint32_t firstIndex;
        uint32_t baseVertex;
        float    threshold;
    } lods[3]; // 48 bytes total
};
static_assert(sizeof(LODGroupGPU) == 48u, "LODGroupGPU stride mismatch");

static LODGroup        g_LODGroups[MAX_LOD_GROUPS];
static uint32_t        g_LODGroupCount = 0;
static RefCntAutoPtr<IBuffer> g_LODGroupBuffer; // GPU SSBO, stride=48, MAX_LOD_GROUPS entries

IBuffer* GetLODGroupBuffer() { return g_LODGroupBuffer; }

const LODGroup* GetLODGroup(uint32_t id) {
    if (id >= g_LODGroupCount) return nullptr;
    return &g_LODGroups[id];
}

uint32_t GetLODGroupCount() { return g_LODGroupCount; }

// ── OS file mapping ───────────────────────────────────────────────────────────
struct MappedFile {
    void*  data = nullptr;
    size_t size = 0;
#ifdef _WIN32
    HANDLE hFile = INVALID_HANDLE_VALUE;
    HANDLE hMap  = NULL;
#else
    int fd = -1;
#endif
};

static bool MapFileReadOnly(const char* path, MappedFile& out) {
#ifdef _WIN32
    out.hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (out.hFile == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER li;
    if (!GetFileSizeEx(out.hFile, &li)) { CloseHandle(out.hFile); return false; }
    out.size = static_cast<size_t>(li.QuadPart);
    out.hMap = CreateFileMappingA(out.hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!out.hMap) { CloseHandle(out.hFile); return false; }
    out.data = MapViewOfFile(out.hMap, FILE_MAP_READ, 0, 0, 0);
    if (!out.data) { CloseHandle(out.hMap); CloseHandle(out.hFile); return false; }
#else
    out.fd = open(path, O_RDONLY);
    if (out.fd < 0) return false;
    struct stat st; fstat(out.fd, &st);
    out.size = static_cast<size_t>(st.st_size);
    out.data = mmap(NULL, out.size, PROT_READ, MAP_PRIVATE, out.fd, 0);
    if (out.data == MAP_FAILED) { close(out.fd); return false; }
#endif
    return true;
}

static void UnmapFile(MappedFile& mf) {
#ifdef _WIN32
    if (mf.data)  UnmapViewOfFile(mf.data);
    if (mf.hMap)  CloseHandle(mf.hMap);
    if (mf.hFile != INVALID_HANDLE_VALUE) CloseHandle(mf.hFile);
#else
    if (mf.data && mf.data != MAP_FAILED) munmap(mf.data, mf.size);
    if (mf.fd >= 0) close(mf.fd);
#endif
}

// ── Init ──────────────────────────────────────────────────────────────────────

bool Init(void* glfw_window_handle) {
    EngineLog("[Graphics] Initializing DiligentCore (Vulkan)...");
    assert(glfw_window_handle != nullptr);

    GLFWwindow* window = static_cast<GLFWwindow*>(glfw_window_handle);
    int w = 0, h = 0;
    glfwGetFramebufferSize(window, &w, &h);
    assert(w > 0 && h > 0);

    IEngineFactoryVk* pFactory = GetEngineFactoryVk();

    EngineVkCreateInfo engineCI;
    engineCI.EnableValidation = false;

    IRenderDevice*  pDevice  = nullptr;
    IDeviceContext* pContext = nullptr;
    pFactory->CreateDeviceAndContextsVk(engineCI, &pDevice, &pContext);
    if (!pDevice || !pContext) {
        EngineLog("[Graphics] FATAL: CreateDeviceAndContextsVk failed");
        return false;
    }
    g_pDevice.Attach(pDevice);
    g_pContext.Attach(pContext);

    SwapChainDesc scDesc;
    scDesc.Width             = static_cast<uint32_t>(w);
    scDesc.Height            = static_cast<uint32_t>(h);
    scDesc.ColorBufferFormat = TEX_FORMAT_BGRA8_UNORM_SRGB;
    scDesc.DepthBufferFormat = TEX_FORMAT_D32_FLOAT;
    scDesc.BufferCount       = 2;

#if defined(_WIN32)
    Win32NativeWindow nativeWindow{ glfwGetWin32Window(window) };
#else
    LinuxNativeWindow nativeWindow;
    nativeWindow.WindowId = glfwGetX11Window(window);
    nativeWindow.pDisplay = glfwGetX11Display();
#endif

    ISwapChain* pSwapChain = nullptr;
    pFactory->CreateSwapChainVk(g_pDevice, g_pContext, scDesc, nativeWindow, &pSwapChain);
    if (!pSwapChain) {
        EngineLog("[Graphics] FATAL: CreateSwapChain failed");
        return false;
    }
    g_pSwapChain.Attach(pSwapChain);

    // Global vertex megabuffer — bindless SSBO for vertex pulling
    {
        BufferDesc bd;
        bd.Name              = "GlobalVB";
        bd.Usage             = USAGE_DEFAULT;
        bd.BindFlags         = BIND_SHADER_RESOURCE | BIND_UNORDERED_ACCESS;
        bd.Mode              = BUFFER_MODE_STRUCTURED;
        bd.ElementByteStride = static_cast<uint32_t>(sizeof(ScryVertex)); // 32
        bd.Size              = GLOBAL_VB_SIZE;
        g_pDevice->CreateBuffer(bd, nullptr, &g_GlobalVertexBuffer);
        if (!g_GlobalVertexBuffer) {
            EngineLog("[Graphics] FATAL: global vertex buffer creation failed");
            return false;
        }
    }

    // Global index buffer
    {
        BufferDesc bd;
        bd.Name      = "GlobalIB";
        bd.Usage     = USAGE_DEFAULT;
        bd.BindFlags = BIND_INDEX_BUFFER;
        bd.Size      = GLOBAL_IB_SIZE;
        g_pDevice->CreateBuffer(bd, nullptr, &g_GlobalIndexBuffer);
        if (!g_GlobalIndexBuffer) {
            EngineLog("[Graphics] FATAL: global index buffer creation failed");
            return false;
        }
    }

    // LOD group SSBO — one entry per loaded mesh, 48 bytes each
    {
        BufferDesc bd;
        bd.Name              = "LODGroupBuffer";
        bd.Usage             = USAGE_DEFAULT;
        bd.BindFlags         = BIND_SHADER_RESOURCE;
        bd.Mode              = BUFFER_MODE_STRUCTURED;
        bd.ElementByteStride = static_cast<uint32_t>(sizeof(LODGroupGPU)); // 48
        bd.Size              = MAX_LOD_GROUPS * static_cast<uint32_t>(sizeof(LODGroupGPU));
        g_pDevice->CreateBuffer(bd, nullptr, &g_LODGroupBuffer);
        if (!g_LODGroupBuffer) {
            EngineLog("[Graphics] FATAL: LOD group buffer creation failed");
            return false;
        }
    }

    {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "[Graphics] DiligentCore Vulkan initialized (%dx%d)", w, h);
        EngineLog(buf);
    }
    return true;
}

void Shutdown() {
    EngineLog("[Graphics] Shutting down DiligentCore...");
    g_LODGroupBuffer.Release();
    g_GlobalVertexBuffer.Release();
    g_GlobalIndexBuffer.Release();
    g_pSwapChain.Release();
    g_pContext.Release();
    g_pDevice.Release();
    EngineLog("[Graphics] DiligentCore shutdown complete");
}

void BeginFrame() {
    ITextureView* pRTV = g_pSwapChain->GetCurrentBackBufferRTV();
    ITextureView* pDSV = g_pSwapChain->GetDepthBufferDSV();
    g_pContext->SetRenderTargets(1, &pRTV, pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    const float clear[4] = { 0.1875f, 0.1875f, 0.1875f, 1.0f };
    g_pContext->ClearRenderTarget(pRTV, clear, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    g_pContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.0f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
}

void Present() {
    g_pSwapChain->Present(1);
}

// ── Mesh loading — uploads 3 LODs into global megabuffers via staging ─────────

LODGroup LoadMesh(const char* filepath) {
    assert(filepath);
    LODGroup kFailed = {};
    kFailed.group_id = UINT32_MAX;
    if (!filepath) return kFailed;
    if (g_LODGroupCount >= MAX_LOD_GROUPS) {
        EngineLog("[Graphics] LoadMesh: LOD group table full");
        return kFailed;
    }

    MappedFile mf = {};
    if (!MapFileReadOnly(filepath, mf)) {
        char err[256];
        std::snprintf(err, sizeof(err), "[Graphics] LoadMesh: not found: %s", filepath);
        EngineLog(err);
        return kFailed;
    }

    const auto* hdr = static_cast<const ScryMeshHeader*>(mf.data);
    if (mf.size < sizeof(ScryMeshHeader) ||
        hdr->magic   != SCRY_MESH_MAGIC  ||
        hdr->version != SCRY_MESH_VERSION) {
        EngineLog("[Graphics] LoadMesh: invalid or outdated .scrymesh (re-cook assets)");
        UnmapFile(mf);
        return kFailed;
    }

    const auto*    verts    = reinterpret_cast<const ScryVertex*>(hdr + 1);
    const auto*    lod0_idx = reinterpret_cast<const uint32_t*>(verts + hdr->vertex_count);
    const auto*    lod1_idx = lod0_idx + hdr->lod0_index_count;
    const auto*    lod2_idx = lod1_idx + hdr->lod1_index_count;

    const uint32_t vb_bytes   = hdr->vertex_count      * static_cast<uint32_t>(sizeof(ScryVertex));
    const uint32_t ib0_bytes  = hdr->lod0_index_count  * static_cast<uint32_t>(sizeof(uint32_t));
    const uint32_t ib1_bytes  = hdr->lod1_index_count  * static_cast<uint32_t>(sizeof(uint32_t));
    const uint32_t ib2_bytes  = hdr->lod2_index_count  * static_cast<uint32_t>(sizeof(uint32_t));
    const uint32_t ib_total   = ib0_bytes + ib1_bytes + ib2_bytes;

    if (g_VertexOffset * static_cast<uint32_t>(sizeof(ScryVertex)) + vb_bytes > GLOBAL_VB_SIZE ||
        g_IndexOffset  * static_cast<uint32_t>(sizeof(uint32_t))   + ib_total > GLOBAL_IB_SIZE) {
        EngineLog("[Graphics] LoadMesh: megabuffer full");
        UnmapFile(mf);
        return kFailed;
    }

    // ── Upload vertices via staging buffer ────────────────────────────────────
    {
        RefCntAutoPtr<IBuffer> pStaging;
        BufferDesc bd;
        bd.Name           = "StagingVB";
        bd.Usage          = USAGE_STAGING;
        bd.BindFlags      = BIND_NONE;
        bd.CPUAccessFlags = CPU_ACCESS_WRITE;
        bd.Size           = vb_bytes;
        g_pDevice->CreateBuffer(bd, nullptr, &pStaging);
        assert(pStaging);

        void* pMapped = nullptr;
        g_pContext->MapBuffer(pStaging, MAP_WRITE, MAP_FLAG_DO_NOT_WAIT, pMapped);
        assert(pMapped);
        std::memcpy(pMapped, verts, vb_bytes);
        g_pContext->UnmapBuffer(pStaging, MAP_WRITE);

        g_pContext->CopyBuffer(
            pStaging, 0,
            RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
            g_GlobalVertexBuffer,
            g_VertexOffset * static_cast<uint32_t>(sizeof(ScryVertex)),
            vb_bytes,
            RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }

    // ── Upload all 3 LOD index ranges via a single staging buffer ─────────────
    {
        RefCntAutoPtr<IBuffer> pStaging;
        BufferDesc bd;
        bd.Name           = "StagingIB";
        bd.Usage          = USAGE_STAGING;
        bd.BindFlags      = BIND_NONE;
        bd.CPUAccessFlags = CPU_ACCESS_WRITE;
        bd.Size           = ib_total;
        g_pDevice->CreateBuffer(bd, nullptr, &pStaging);
        assert(pStaging);

        void* pMapped = nullptr;
        g_pContext->MapBuffer(pStaging, MAP_WRITE, MAP_FLAG_DO_NOT_WAIT, pMapped);
        assert(pMapped);
        auto* dst = static_cast<uint8_t*>(pMapped);
        std::memcpy(dst,                        lod0_idx, ib0_bytes);
        std::memcpy(dst + ib0_bytes,            lod1_idx, ib1_bytes);
        std::memcpy(dst + ib0_bytes + ib1_bytes,lod2_idx, ib2_bytes);
        g_pContext->UnmapBuffer(pStaging, MAP_WRITE);

        g_pContext->CopyBuffer(
            pStaging, 0,
            RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
            g_GlobalIndexBuffer,
            g_IndexOffset * static_cast<uint32_t>(sizeof(uint32_t)),
            ib_total,
            RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }

    // ── Build LODGroup record ─────────────────────────────────────────────────
    const uint32_t group_id    = g_LODGroupCount;
    const uint32_t baseVertex  = g_VertexOffset;
    const uint32_t fi0         = g_IndexOffset;
    const uint32_t fi1         = fi0 + hdr->lod0_index_count;
    const uint32_t fi2         = fi1 + hdr->lod1_index_count;

    LODGroup lg;
    lg.lods[0] = { hdr->lod0_index_count, fi0, baseVertex, 150.0f };
    lg.lods[1] = { hdr->lod1_index_count, fi1, baseVertex, 300.0f };
    lg.lods[2] = { hdr->lod2_index_count, fi2, baseVertex, 600.0f };
    lg.group_id = group_id;
    g_LODGroups[group_id] = lg;

    // ── Push GPU LOD group entry via staging ──────────────────────────────────
    {
        LODGroupGPU gpu;
        for (int i = 0; i < 3; ++i) {
            gpu.lods[i].indexCount = lg.lods[i].indexCount;
            gpu.lods[i].firstIndex = lg.lods[i].firstIndex;
            gpu.lods[i].baseVertex = lg.lods[i].baseVertex;
            gpu.lods[i].threshold  = lg.lods[i].threshold;
        }
        g_pContext->UpdateBuffer(
            g_LODGroupBuffer,
            group_id * static_cast<uint32_t>(sizeof(LODGroupGPU)),
            static_cast<uint32_t>(sizeof(LODGroupGPU)),
            &gpu,
            RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }

    g_VertexOffset += hdr->vertex_count;
    g_IndexOffset  += hdr->lod0_index_count + hdr->lod1_index_count + hdr->lod2_index_count;
    ++g_LODGroupCount;

    {
        char buf[512];
        std::snprintf(buf, sizeof(buf),
            "[Graphics] Mesh loaded: %s (v=%u LOD0=%u LOD1=%u LOD2=%u idx, group_id=%u)",
            filepath,
            hdr->vertex_count,
            hdr->lod0_index_count, hdr->lod1_index_count, hdr->lod2_index_count,
            group_id);
        EngineLog(buf);
    }
    UnmapFile(mf);
    return lg;
}

} // namespace Graphics
} // namespace Engine
