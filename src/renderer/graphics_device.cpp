#include "graphics_internal.h"
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

using namespace Diligent;

namespace Engine {
namespace Graphics {

// ── Diligent globals ──────────────────────────────────────────────────────────
RefCntAutoPtr<IRenderDevice>  g_pDevice;
RefCntAutoPtr<IDeviceContext> g_pContext;
RefCntAutoPtr<ISwapChain>     g_pSwapChain;

IRenderDevice*  GetDevice() {
    assert(g_pDevice != nullptr);
    return g_pDevice;
}

IDeviceContext* GetContext() {
    assert(g_pContext != nullptr);
    return g_pContext;
}

ISwapChain*     GetSwapChain() {
    assert(g_pSwapChain != nullptr);
    return g_pSwapChain;
}

bool Init(void* glfw_window_handle) {
    assert(glfw_window_handle != nullptr);
    std::printf("[Graphics] Initializing graphics system...\n");

    EngineLog("[Graphics] Initializing DiligentCore (Vulkan)...");

    GLFWwindow* window = static_cast<GLFWwindow*>(glfw_window_handle);
    int w = 0, h = 0;
    glfwGetFramebufferSize(window, &w, &h);
    assert(w > 0 && h > 0);

    IEngineFactoryVk* pFactory = GetEngineFactoryVk();

    EngineVkCreateInfo engineCI;
    engineCI.EnableValidation = false;
    engineCI.MainDescriptorPoolSize.MaxDescriptorSets    = 1024;
    engineCI.DynamicDescriptorPoolSize.MaxDescriptorSets = 1024;
    engineCI.DeviceLocalMemoryPageSize  = 2u * 1024u * 1024u; 
    engineCI.HostVisibleMemoryPageSize  = 1u * 1024u * 1024u; 
    engineCI.UploadHeapPageSize         = 1u * 1024u * 1024u; 

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

    // Initialize megabuffers and LOD buffer (calls from resources)
    if (!InitResources()) return false;

    {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "[Graphics] DiligentCore Vulkan initialized (%dx%d)", w, h);
        EngineLog(buf);
    }
    return true;
}

void Shutdown() {
    assert(g_pDevice != nullptr);
    std::printf("[Graphics] Shutting down graphics system...\n");

    EngineLog("[Graphics] Shutting down DiligentCore...");
    
    ShutdownResources();

    g_pSwapChain.Release();
    g_pContext.Release();
    g_pDevice.Release();
    EngineLog("[Graphics] DiligentCore shutdown complete");
}

void BeginFrame() {
    assert(g_pSwapChain != nullptr);
    assert(g_pContext != nullptr);

    ITextureView* pRTV = g_pSwapChain->GetCurrentBackBufferRTV();
    ITextureView* pDSV = g_pSwapChain->GetDepthBufferDSV();
    g_pContext->SetRenderTargets(1, &pRTV, pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    const float clear[4] = { 0.1875f, 0.1875f, 0.1875f, 1.0f };
    g_pContext->ClearRenderTarget(pRTV, clear, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    g_pContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.0f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
}

void Present() {
    assert(g_pSwapChain != nullptr);
    g_pSwapChain->Present(1);
}

} // namespace Graphics
} // namespace Engine
