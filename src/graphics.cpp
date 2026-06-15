#include <engine/graphics.hpp>
#include <engine/CookedAsset.h>
#include <engine/memory.hpp>
#include "graphics_internal.hpp"

#include <libassert/assert.hpp>
#include <mimalloc.h>
#include <cstdio>
#include <cstring>

#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>

/* ── Platform-specific Diligent backend ──────────────────────────────────────
 * MSVC on Windows → D3D12.  GCC/MinGW on Windows or Linux → Vulkan.
 * Controlled by compile-time definitions set in CMakeLists.txt.
 */
#if defined(SCRY_BACKEND_DX12)
#  include <Graphics/GraphicsEngineDirect3D12/interface/EngineFactoryD3D12.h>
#  pragma comment(lib, "d3d12.lib")
#  pragma comment(lib, "dxgi.lib")
#elif defined(SCRY_BACKEND_VULKAN)
#  include <Graphics/GraphicsEngineVulkan/interface/EngineFactoryVk.h>
#else
#  error "Define SCRY_BACKEND_DX12 or SCRY_BACKEND_VULKAN via CMake."
#endif

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#endif

using namespace Diligent;

namespace Engine {
namespace Graphics {

// ── Module-level state ────────────────────────────────────────────────────────

static RefCntAutoPtr<IRenderDevice>  g_device;
static RefCntAutoPtr<IDeviceContext> g_ctx;
static RefCntAutoPtr<ISwapChain>     g_swap_chain;

static MeshBuffers g_meshes[MAX_MESHES];
static uint32_t    g_mesh_count = 0;

// ── Internal accessors ────────────────────────────────────────────────────────

IRenderDevice*  GetDevice()              { return g_device;     }
IDeviceContext* GetContext()             { return g_ctx;        }
ISwapChain*     GetSwapChain()           { return g_swap_chain; }
MeshBuffers*    GetMesh(uint32_t handle) {
    if (handle >= MAX_MESHES || !g_meshes[handle].in_use) return nullptr;
    return &g_meshes[handle];
}

// ── Init ──────────────────────────────────────────────────────────────────────

bool Init(void* sdl_window_handle) {
    DEBUG_ASSERT(sdl_window_handle != nullptr);
    if (!sdl_window_handle) {
        EngineLog("[Graphics] FATAL: null SDL window handle");
        return false;
    }

#if defined(SCRY_BACKEND_VULKAN)
    IEngineFactoryVk* factory = GetEngineFactoryVk();
    DEBUG_ASSERT(factory != nullptr);

    EngineVkCreateInfo ci;

    IRenderDevice*  raw_device = nullptr;
    IDeviceContext* raw_ctx    = nullptr;
    factory->CreateDeviceAndContextsVk(ci, &raw_device, &raw_ctx);
    if (!raw_device || !raw_ctx) {
        EngineLog("[Graphics] FATAL: Vulkan device creation failed");
        return false;
    }
    g_device.Attach(raw_device);
    g_ctx.Attach(raw_ctx);

    SwapChainDesc sc_desc;
    sc_desc.ColorBufferFormat = TEX_FORMAT_BGRA8_UNORM_SRGB;
    sc_desc.DepthBufferFormat = TEX_FORMAT_D32_FLOAT;
    sc_desc.BufferCount       = 2;

#  ifdef _WIN32
    SDL_PropertiesID props = SDL_GetWindowProperties(static_cast<SDL_Window*>(sdl_window_handle));
    HWND hwnd = static_cast<HWND>(
        SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
    DEBUG_ASSERT(hwnd != nullptr);
    Win32NativeWindow native_window{hwnd};
#  else
    // Linux: obtain X11 window + Display from SDL3 properties
    Window  x11_win  = static_cast<Window>(SDL_GetNumberProperty(
        SDL_GetWindowProperties(static_cast<SDL_Window*>(sdl_window_handle)),
        SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0));
    Display* x11_dpy = static_cast<Display*>(SDL_GetPointerProperty(
        SDL_GetWindowProperties(static_cast<SDL_Window*>(sdl_window_handle)),
        SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr));
    DEBUG_ASSERT(x11_win != 0 && x11_dpy != nullptr);
    LinuxNativeWindow native_window;
    native_window.WindowId = x11_win;
    native_window.pDisplay = x11_dpy;
#  endif

    factory->CreateSwapChainVk(g_device, g_ctx, sc_desc, native_window, &g_swap_chain);

#elif defined(SCRY_BACKEND_DX12)
    IEngineFactoryD3D12* factory = GetEngineFactoryD3D12();
    DEBUG_ASSERT(factory != nullptr);

    EngineD3D12CreateInfo ci;

    IRenderDevice*  raw_device = nullptr;
    IDeviceContext* raw_ctx    = nullptr;
    factory->CreateDeviceAndContextsD3D12(ci, &raw_device, &raw_ctx);
    if (!raw_device || !raw_ctx) {
        EngineLog("[Graphics] FATAL: D3D12 device creation failed");
        return false;
    }
    g_device.Attach(raw_device);
    g_ctx.Attach(raw_ctx);

    SDL_PropertiesID props = SDL_GetWindowProperties(static_cast<SDL_Window*>(sdl_window_handle));
    HWND hwnd = static_cast<HWND>(
        SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
    DEBUG_ASSERT(hwnd != nullptr);

    SwapChainDesc sc_desc;
    sc_desc.ColorBufferFormat = TEX_FORMAT_BGRA8_UNORM_SRGB;
    sc_desc.DepthBufferFormat = TEX_FORMAT_D32_FLOAT;
    sc_desc.BufferCount       = 2;
    Win32NativeWindow native_window{hwnd};
    factory->CreateSwapChainD3D12(g_device, g_ctx, sc_desc, FullScreenModeDesc{}, native_window, &g_swap_chain);
#endif

    if (!g_swap_chain) {
        EngineLog("[Graphics] FATAL: swap chain creation failed");
        return false;
    }

#ifndef NDEBUG
    EngineLog("[Graphics] Diligent device + swap chain ready");
#endif
    return true;
}

// ── Shutdown ──────────────────────────────────────────────────────────────────

void Shutdown() {
    for (uint32_t i = 0; i < MAX_MESHES; ++i) {
        if (g_meshes[i].in_use) FreeMesh(i);
    }
    g_swap_chain.Release();
    g_ctx.Release();
    g_device.Release();
#ifndef NDEBUG
    EngineLog("[Graphics] Shutdown complete");
#endif
}

// ── BeginFrame ────────────────────────────────────────────────────────────────
// Called once per frame from platform.cpp before ecs_progress.
// Acquires the back buffer and clears colour + depth so the swap chain image
// is always in a valid state before Present(), even when no mesh entities exist.

void BeginFrame() {
    if (!g_swap_chain || !g_ctx) return;

    auto* rtv = g_swap_chain->GetCurrentBackBufferRTV();
    auto* dsv = g_swap_chain->GetDepthBufferDSV();
    const float clear[4] = {0.05f, 0.05f, 0.05f, 1.0f};
    g_ctx->SetRenderTargets(1, &rtv, dsv, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    g_ctx->ClearRenderTarget(rtv, clear, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    g_ctx->ClearDepthStencil(dsv, CLEAR_DEPTH_FLAG, 1.0f, 0,
                             RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
}

// ── Present ───────────────────────────────────────────────────────────────────

void Present() {
    DEBUG_ASSERT(g_swap_chain != nullptr);
    if (g_swap_chain) {
        g_swap_chain->Present();
    }
}

// ── LoadMesh ──────────────────────────────────────────────────────────────────

uint32_t LoadMesh(const char* filepath) {
    DEBUG_ASSERT(filepath != nullptr);
    DEBUG_ASSERT(g_device != nullptr);

    if (!filepath || !g_device) return INVALID_MESH;
    if (g_mesh_count >= MAX_MESHES) {
        EngineLog("[Graphics] LoadMesh: mesh table full");
        return INVALID_MESH;
    }

    // ── Single fread into a temporary arena ──────────────────────────────────
    FILE* f = std::fopen(filepath, "rb");
    if (!f) {
        char err_buf[256];
        std::snprintf(err_buf, sizeof(err_buf), "[Graphics] LoadMesh: file not found: %s", filepath);
        EngineLog(err_buf);
        return INVALID_MESH;
    }

    std::fseek(f, 0, SEEK_END);
    const size_t file_size = static_cast<size_t>(std::ftell(f));
    std::rewind(f);

    void* blob = mi_malloc(file_size);
    DEBUG_ASSERT(blob != nullptr);
    if (!blob) { std::fclose(f); return INVALID_MESH; }

    const size_t read = std::fread(blob, 1, file_size, f);
    std::fclose(f);
    DEBUG_ASSERT(read == file_size);

    if (read != file_size) {
        mi_free(blob);
        EngineLog("[Graphics] LoadMesh: read error");
        return INVALID_MESH;
    }

    // ── Validate header ───────────────────────────────────────────────────────
    const auto* hdr = static_cast<const ScryMeshHeader*>(blob);
    DEBUG_ASSERT(hdr->magic   == SCRY_MESH_MAGIC);
    DEBUG_ASSERT(hdr->version == SCRY_MESH_VERSION);
    DEBUG_ASSERT(hdr->vertex_count > 0);
    DEBUG_ASSERT(hdr->index_count  > 0);

    if (hdr->magic != SCRY_MESH_MAGIC || hdr->version != SCRY_MESH_VERSION) {
        EngineLog("[Graphics] LoadMesh: invalid .scrymesh header");
        mi_free(blob);
        return INVALID_MESH;
    }

    const auto* vertices = reinterpret_cast<const ScryVertex*>(hdr + 1);
    const auto* indices  = reinterpret_cast<const uint32_t*>(vertices + hdr->vertex_count);

    // ── Create Diligent static GPU buffers ───────────────────────────────────
    const uint32_t slot = g_mesh_count++;

    // Vertex buffer
    {
        BufferDesc bd;
        bd.Name      = "VertexBuffer";
        bd.Usage     = USAGE_IMMUTABLE;
        bd.BindFlags = BIND_VERTEX_BUFFER;
        bd.Size      = hdr->vertex_count * sizeof(ScryVertex);

        BufferData init_data;
        init_data.pData    = vertices;
        init_data.DataSize = bd.Size;

        g_device->CreateBuffer(bd, &init_data, &g_meshes[slot].vertices);
        DEBUG_ASSERT(g_meshes[slot].vertices != nullptr);
    }

    // Index buffer
    {
        BufferDesc bd;
        bd.Name      = "IndexBuffer";
        bd.Usage     = USAGE_IMMUTABLE;
        bd.BindFlags = BIND_INDEX_BUFFER;
        bd.Size      = hdr->index_count * sizeof(uint32_t);

        BufferData init_data;
        init_data.pData    = indices;
        init_data.DataSize = bd.Size;

        g_device->CreateBuffer(bd, &init_data, &g_meshes[slot].indices);
        DEBUG_ASSERT(g_meshes[slot].indices != nullptr);
    }

    g_meshes[slot].index_count = hdr->index_count;
    g_meshes[slot].in_use      = true;

#ifndef NDEBUG
    char buf[128];
    std::snprintf(buf, sizeof(buf), "[Graphics] Mesh loaded: %s (v=%u i=%u handle=%u)",
        filepath, hdr->vertex_count, hdr->index_count, slot);
    EngineLog(buf);
#endif

    mi_free(blob);

    return slot;
}

// ── FreeMesh ──────────────────────────────────────────────────────────────────

void FreeMesh(uint32_t handle) {
    DEBUG_ASSERT(handle < MAX_MESHES);
    if (handle >= MAX_MESHES) return;
    g_meshes[handle].vertices.Release();
    g_meshes[handle].indices.Release();
    g_meshes[handle].index_count = 0;
    g_meshes[handle].in_use      = false;
}

} // namespace Graphics
} // namespace Engine
