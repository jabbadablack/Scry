#ifdef SCRY_DEBUG

#include <engine/debug/debug_ui.h>
#include <engine/renderer/core.h>

#include <ImGuiImplDiligent.hpp>

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>

#include "Graphics/GraphicsEngine/interface/DeviceContext.h"
#include "Graphics/GraphicsEngine/interface/SwapChain.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

using namespace Diligent;

static bool               g_ShowUI     = false;
static ImGuiImplDiligent* g_pImGuiImpl = nullptr;

extern "C" {

void DebugUI_Init(void* window, void* device, void* /*context*/) {
    IRenderDevice* dev = static_cast<IRenderDevice*>(device);
    ISwapChain*    sc  = static_cast<ISwapChain*>(ScryGraphics_GetSwapChain());

    // ImGuiImplDiligent ctor calls ImGui::CreateContext() internally.
    // Calling CreateContext() ourselves first would leave a context with the
    // GLFW backend already installed when the ctor's CreateContext() fires —
    // imgui asserts BackendPlatformUserData == NULL at that point.
    const SwapChainDesc& scDesc = sc->GetDesc();
    g_pImGuiImpl = new ImGuiImplDiligent(ImGuiDiligentCreateInfo{dev, scDesc.ColorBufferFormat, TEX_FORMAT_UNKNOWN});

    // Context is now live — configure it, then install the GLFW platform backend.
    IMGUI_CHECKVERSION();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForVulkan(static_cast<GLFWwindow*>(window), true);
}

void DebugUI_Toggle(void) {
    g_ShowUI = !g_ShowUI;
}

void DebugUI_NewFrame(void) {
    ISwapChain* sc = static_cast<ISwapChain*>(ScryGraphics_GetSwapChain());
    const SwapChainDesc& scDesc = sc->GetDesc();
    // GLFW must run first — it sets io.DisplaySize which ImGui::NewFrame() asserts on.
    // ImGuiImplDiligent::NewFrame() calls ImGui::NewFrame() internally, so we must not.
    ImGui_ImplGlfw_NewFrame();
    g_pImGuiImpl->NewFrame(scDesc.Width, scDesc.Height, SURFACE_TRANSFORM_IDENTITY);
}

void DebugUI_Render(void) {
    if (g_ShowUI) {
        ImGui::ShowMetricsWindow();
        if (ImGui::Begin("Scry Engine")) {
            ImGui::Text("F1 -- toggle this panel");
        }
        ImGui::End();
    }

    // ImGuiImplDiligent::Render() calls ImGui::Render() (which auto-calls EndFrame())
    // internally — do not call them separately.
    IDeviceContext* ctx = static_cast<IDeviceContext*>(ScryGraphics_GetContext());
    ISwapChain*     sc  = static_cast<ISwapChain*>(ScryGraphics_GetSwapChain());
    ITextureView*   pRTV = sc->GetCurrentBackBufferRTV();
    ctx->SetRenderTargets(1, &pRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    g_pImGuiImpl->Render(ctx);
}

void DebugUI_Shutdown(void) {
    // GLFW backend must be shut down BEFORE the dtor calls ImGui::DestroyContext(),
    // which asserts BackendPlatformUserData == NULL.
    ImGui_ImplGlfw_Shutdown();
    delete g_pImGuiImpl;   // dtor: InvalidateDeviceObjects() + ImGui::DestroyContext()
    g_pImGuiImpl = nullptr;
}

} // extern "C"

#endif // SCRY_DEBUG
