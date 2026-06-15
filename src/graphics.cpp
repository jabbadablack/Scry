#include <engine/graphics.hpp>
#include <engine/graphics_backend.hpp>
#include <engine/CookedAsset.h>
#include <engine/renderer.hpp>

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

IRenderDevice*  GetDevice()   { return g_pDevice;   }
IDeviceContext* GetContext()  { return g_pContext;   }
ISwapChain*     GetSwapChain(){ return g_pSwapChain; }

// ── Mesh table ────────────────────────────────────────────────────────────────
struct MeshBuffers {
    RefCntAutoPtr<IBuffer> vb;           // vertex SSBO (BIND_SHADER_RESOURCE)
    RefCntAutoPtr<IBuffer> ib;           // index buffer
    uint32_t               index_count = 0;
    bool                   in_use      = false;
};

static MeshBuffers g_meshes[MAX_MESHES];
static uint32_t    g_mesh_count = 0;

IBuffer* GetVertexBuffer(uint32_t handle) {
    if (handle < MAX_MESHES && g_meshes[handle].in_use) return g_meshes[handle].vb;
    return nullptr;
}
IBuffer* GetIndexBuffer(uint32_t handle) {
    if (handle < MAX_MESHES && g_meshes[handle].in_use) return g_meshes[handle].ib;
    return nullptr;
}

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
    out.hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
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

    {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "[Graphics] DiligentCore Vulkan initialized (%dx%d)", w, h);
        EngineLog(buf);
    }
    return true;
}

void Shutdown() {
    EngineLog("[Graphics] Shutting down DiligentCore...");
    for (uint32_t i = 0; i < MAX_MESHES; ++i) {
        if (g_meshes[i].in_use) FreeMesh(i);
    }
    g_pSwapChain.Release();
    g_pContext.Release();
    g_pDevice.Release();
    EngineLog("[Graphics] DiligentCore shutdown complete");
}

void BeginFrame() {
    ITextureView* pRTV = g_pSwapChain->GetCurrentBackBufferRTV();
    ITextureView* pDSV = g_pSwapChain->GetDepthBufferDSV();
    g_pContext->SetRenderTargets(1, &pRTV, pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    const float clear[4] = { 0.1875f, 0.1875f, 0.1875f, 1.0f };  // 0x303030FF
    g_pContext->ClearRenderTarget(pRTV, clear, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    g_pContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.0f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
}

void Present() {
    g_pSwapChain->Present();
}

// ── Mesh loading ──────────────────────────────────────────────────────────────

uint32_t LoadMesh(const char* filepath) {
    assert(filepath && g_mesh_count < MAX_MESHES);
    if (!filepath || g_mesh_count >= MAX_MESHES) return INVALID_MESH;

    MappedFile mf = {};
    if (!MapFileReadOnly(filepath, mf)) {
        char err[256];
        std::snprintf(err, sizeof(err), "[Graphics] LoadMesh: not found: %s", filepath);
        EngineLog(err);
        return INVALID_MESH;
    }

    const auto* hdr = static_cast<const ScryMeshHeader*>(mf.data);
    if (mf.size < sizeof(ScryMeshHeader) || hdr->magic != SCRY_MESH_MAGIC || hdr->version != SCRY_MESH_VERSION) {
        EngineLog("[Graphics] LoadMesh: invalid .scrymesh");
        UnmapFile(mf);
        return INVALID_MESH;
    }

    const auto* verts   = reinterpret_cast<const ScryVertex*>(hdr + 1);
    const auto* indices = reinterpret_cast<const uint32_t*>(verts + hdr->vertex_count);
    const uint32_t slot = g_mesh_count++;

    // Vertex buffer — BIND_SHADER_RESOURCE for SSBO vertex pulling
    {
        BufferDesc bd;
        bd.Name              = "MeshVB";
        bd.Usage             = USAGE_IMMUTABLE;
        bd.BindFlags         = BIND_SHADER_RESOURCE;
        bd.Mode              = BUFFER_MODE_STRUCTURED;
        bd.ElementByteStride = static_cast<uint32_t>(sizeof(ScryVertex)); // 32
        bd.Size              = hdr->vertex_count * sizeof(ScryVertex);

        BufferData data;
        data.pData    = verts;
        data.DataSize = bd.Size;
        g_pDevice->CreateBuffer(bd, &data, &g_meshes[slot].vb);
    }

    // Index buffer
    {
        BufferDesc bd;
        bd.Name      = "MeshIB";
        bd.Usage     = USAGE_IMMUTABLE;
        bd.BindFlags = BIND_INDEX_BUFFER;
        bd.Size      = hdr->index_count * sizeof(uint32_t);

        BufferData data;
        data.pData    = indices;
        data.DataSize = bd.Size;
        g_pDevice->CreateBuffer(bd, &data, &g_meshes[slot].ib);
    }

    assert(g_meshes[slot].vb && g_meshes[slot].ib);
    g_meshes[slot].index_count = hdr->index_count;
    g_meshes[slot].in_use      = true;

    {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "[Graphics] Mesh: %s (v=%u i=%u slot=%u)",
            filepath, hdr->vertex_count, hdr->index_count, slot);
        EngineLog(buf);
    }
    UnmapFile(mf);
    return slot;
}

void FreeMesh(uint32_t handle) {
    if (handle >= MAX_MESHES || !g_meshes[handle].in_use) return;
    g_meshes[handle].vb.Release();
    g_meshes[handle].ib.Release();
    g_meshes[handle].index_count = 0;
    g_meshes[handle].in_use      = false;
}

uint32_t GetIndexCount(uint32_t handle) {
    if (handle < MAX_MESHES && g_meshes[handle].in_use) return g_meshes[handle].index_count;
    return 0;
}

} // namespace Graphics
} // namespace Engine
