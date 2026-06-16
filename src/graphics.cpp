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
static constexpr uint32_t GLOBAL_VB_SIZE = 128u * 1024u * 1024u; // 128 MB
static constexpr uint32_t GLOBAL_IB_SIZE =  64u * 1024u * 1024u; //  64 MB

static RefCntAutoPtr<IBuffer> g_GlobalVertexBuffer;
static RefCntAutoPtr<IBuffer> g_GlobalIndexBuffer;

static uint32_t g_VertexOffset = 0; // next free vertex slot (in vertices, not bytes)
static uint32_t g_IndexOffset  = 0; // next free index  slot (in indices,  not bytes)

IBuffer* GetGlobalVertexBuffer() { return g_GlobalVertexBuffer; }
IBuffer* GetGlobalIndexBuffer()  { return g_GlobalIndexBuffer;  }

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

    {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "[Graphics] DiligentCore Vulkan initialized (%dx%d)", w, h);
        EngineLog(buf);
    }
    return true;
}

void Shutdown() {
    EngineLog("[Graphics] Shutting down DiligentCore...");
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

// ── Mesh loading — uploads into global megabuffers via staging ────────────────

MeshAllocation LoadMesh(const char* filepath) {
    assert(filepath);
    const MeshAllocation kFailed = {0, 0, 0};
    if (!filepath) return kFailed;

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
        EngineLog("[Graphics] LoadMesh: invalid .scrymesh");
        UnmapFile(mf);
        return kFailed;
    }

    const auto*    verts    = reinterpret_cast<const ScryVertex*>(hdr + 1);
    const auto*    indices  = reinterpret_cast<const uint32_t*>(verts + hdr->vertex_count);
    const uint32_t vb_bytes = hdr->vertex_count * static_cast<uint32_t>(sizeof(ScryVertex));
    const uint32_t ib_bytes = hdr->index_count  * static_cast<uint32_t>(sizeof(uint32_t));

    if (g_VertexOffset * sizeof(ScryVertex) + vb_bytes > GLOBAL_VB_SIZE ||
        g_IndexOffset  * sizeof(uint32_t)   + ib_bytes > GLOBAL_IB_SIZE) {
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

    // ── Upload indices via staging buffer ─────────────────────────────────────
    {
        RefCntAutoPtr<IBuffer> pStaging;
        BufferDesc bd;
        bd.Name           = "StagingIB";
        bd.Usage          = USAGE_STAGING;
        bd.BindFlags      = BIND_NONE;
        bd.CPUAccessFlags = CPU_ACCESS_WRITE;
        bd.Size           = ib_bytes;
        g_pDevice->CreateBuffer(bd, nullptr, &pStaging);
        assert(pStaging);

        void* pMapped = nullptr;
        g_pContext->MapBuffer(pStaging, MAP_WRITE, MAP_FLAG_DO_NOT_WAIT, pMapped);
        assert(pMapped);
        std::memcpy(pMapped, indices, ib_bytes);
        g_pContext->UnmapBuffer(pStaging, MAP_WRITE);

        g_pContext->CopyBuffer(
            pStaging, 0,
            RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
            g_GlobalIndexBuffer,
            g_IndexOffset * static_cast<uint32_t>(sizeof(uint32_t)),
            ib_bytes,
            RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }

    MeshAllocation alloc;
    alloc.indexCount  = hdr->index_count;
    alloc.firstIndex  = g_IndexOffset;
    alloc.baseVertex  = g_VertexOffset;

    g_VertexOffset += hdr->vertex_count;
    g_IndexOffset  += hdr->index_count;

    {
        char buf[256];
        std::snprintf(buf, sizeof(buf),
            "[Graphics] Mesh loaded: %s (v=%u i=%u baseVertex=%u firstIndex=%u)",
            filepath, hdr->vertex_count, hdr->index_count,
            alloc.baseVertex, alloc.firstIndex);
        EngineLog(buf);
    }
    UnmapFile(mf);
    return alloc;
}

} // namespace Graphics
} // namespace Engine
