#include <OS/glfw/glfw_window.hpp>
#include <OS/glfw/glfw_input.hpp>

#include <GLFW/glfw3.h>

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>

#include "Imgui/interface/ImGuiImplDiligent.hpp"
#include "Common/interface/RefCntAutoPtr.hpp"
#include "Graphics/GraphicsEngine/interface/RenderDevice.h"
#include "Graphics/GraphicsEngine/interface/DeviceContext.h"
#include "Graphics/GraphicsEngine/interface/SwapChain.h"
#include "Graphics/GraphicsEngineVulkan/interface/EngineFactoryVk.h"

int main() {
    engine::GlfwWindow window;
    engine::GlfwInput input;

    if (!window.Initialize()) return -1;
    if (!window.CreateWindow(1920, 1080, "Scry Editor")) return -1;
    glfwMaximizeWindow(window.GetRawWindow());

    input.Initialize(window.GetRawWindow());

    engine::NativeHandles handles = window.GetNativeHandles();
    Diligent::NativeWindow nativeWindow{};
#if defined(ENGINE_PLATFORM_WINDOWS)
    nativeWindow.hWnd = handles.window;
#elif defined(ENGINE_PLATFORM_LINUX)
    nativeWindow.WindowId = reinterpret_cast<uint32_t>(handles.window);
    nativeWindow.pDisplay = handles.display;
#endif

    auto* pFactoryVk = Diligent::GetEngineFactoryVk();
    if (pFactoryVk == nullptr) return -1;

    Diligent::EngineVkCreateInfo engineCI{};
    engineCI.DynamicHeapSize = 2 * 1024 * 1024;
    engineCI.DynamicHeapPageSize = 256 * 1024;

    Diligent::RefCntAutoPtr<Diligent::IRenderDevice> pDevice;
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext> pContext;
    pFactoryVk->CreateDeviceAndContextsVk(engineCI, &pDevice, &pContext);
    if (!pDevice) return -1;

    Diligent::SwapChainDesc scDesc{};
    Diligent::RefCntAutoPtr<Diligent::ISwapChain> pSwapChain;
    pFactoryVk->CreateSwapChainVk(pDevice, pContext, scDesc, nativeWindow, &pSwapChain);
    if (!pSwapChain) return -1;

    ImGui::CreateContext();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    Diligent::ImGuiDiligentCreateInfo imguiCI{};
    imguiCI.pDevice = pDevice;
    imguiCI.BackBufferFmt = pSwapChain->GetDesc().ColorBufferFormat;
    imguiCI.DepthBufferFmt = pSwapChain->GetDesc().DepthBufferFormat;
    auto* pImGui = new Diligent::ImGuiImplDiligent(imguiCI);
    pImGui->CreateDeviceObjects();

    ImGui_ImplGlfw_InitForOther(window.GetRawWindow(), true);

    while (!window.ShouldClose()) {
        window.PollEvents();
        input.Update();

        ImGui_ImplGlfw_NewFrame();
        pImGui->NewFrame(pSwapChain->GetDesc().Width, pSwapChain->GetDesc().Height,
                         pSwapChain->GetDesc().PreTransform);

        if (ImGui::Begin("Viewport")) {
            ImGui::Text("Game viewport will be drawn here.");
        }
        ImGui::End();

        ImGui::Render();

        auto* pRTV = pSwapChain->GetCurrentBackBufferRTV();
        auto* pDSV = pSwapChain->GetDepthBufferDSV();
        const float clearColor[] = {0.1F, 0.1F, 0.1F, 1.0F};
        pContext->SetRenderTargets(1, &pRTV, pDSV, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        pContext->ClearRenderTarget(pRTV, clearColor, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        pContext->ClearDepthStencil(pDSV, Diligent::CLEAR_DEPTH_FLAG, 1.0F, 0,
                                    Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        pImGui->Render(pContext);

        pSwapChain->Present();
    }

    ImGui_ImplGlfw_Shutdown();
    delete pImGui;
    ImGui::DestroyContext();

    pSwapChain.Release();
    pContext.Release();
    pDevice.Release();

    return 0;
}
