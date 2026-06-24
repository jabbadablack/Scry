#include "renderer/diligent_module.hpp"
#include "diligent_allocator.hpp"
#include <engine.hpp>
#include <OS/IWindow.hpp>
#include <ecs/components.hpp>
#include <math/interpolation.hpp>
#include <debug/logger.hpp>
#include <debug/assert.h>

extern "C" {
    extern void* vkGetPhysicalDeviceFeatures2;
    extern void* vkGetPhysicalDeviceFeatures2KHR;
}

#include "Graphics/GraphicsEngineVulkan/interface/EngineFactoryVk.h"


namespace engine::renderer {

    bool DiligentModule::Initialize(engine::Engine& engine) {
        ENGINE_ASSERT(m_engine == nullptr, "DiligentModule::Initialize called more than once");
        ENGINE_ASSERT(engine.GetWindowManager().GetMainWindow() != nullptr, "DiligentModule requires a main window before initialization");

        m_engine = &engine;
        ENGINE_LOG_INFO("DiligentModule: initializing Vulkan renderer");

        auto* custom_allocator = new EngineDiligentAllocator();
        m_allocatorInstance = custom_allocator;

        engine::NativeHandles handles = engine.GetWindowManager().GetMainWindow()->GetNativeHandles();

        Diligent::NativeWindow Window;
#if defined(ENGINE_PLATFORM_WINDOWS)
        Window.hWnd = handles.window;
#elif defined(ENGINE_PLATFORM_LINUX)
        Window.WindowId = reinterpret_cast<uint32_t>(handles.window);
        Window.pDisplay = handles.display;
#endif

        auto* pFactoryVk = Diligent::GetEngineFactoryVk();
        if (!pFactoryVk) {
            ENGINE_LOG_ERROR("DiligentModule: failed to retrieve EngineFactoryVk");
            return false;
        }

        Diligent::EngineVkCreateInfo EngineCI{};
        pFactoryVk->CreateDeviceAndContextsVk(EngineCI, &m_pDevice, &m_pImmediateContext);
        if (!m_pDevice) {
            ENGINE_LOG_ERROR("DiligentModule: failed to create Vulkan device and context");
            return false;
        }

        Diligent::SwapChainDesc SCDesc{};
        pFactoryVk->CreateSwapChainVk(m_pDevice, m_pImmediateContext, SCDesc, Window, &m_pSwapChain);
        if (!m_pSwapChain) {
            ENGINE_LOG_ERROR("DiligentModule: failed to create swap chain");
            return false;
        }

        m_running.store(true, std::memory_order_release);
        m_renderThread = std::thread(&DiligentModule::RenderThreadLoop, this);

        ENGINE_LOG_INFO("DiligentModule: Vulkan renderer initialized");
        return true;
    }

    void DiligentModule::BuildGraph(tf::Taskflow& taskflow) {
        // Decoupled execution graph
    }

    void DiligentModule::CompileFrameGraph(FrameDAG& dag) {
        ENGINE_ASSERT(m_engine != nullptr, "DiligentModule::CompileFrameGraph called before Initialize");
        ENGINE_ASSERT(m_pDevice != nullptr, "DiligentModule::CompileFrameGraph called without a valid Vulkan device");

        int write_state = dag.write_state;

        tf::Task init_task = dag.taskflow.emplace([this, write_state]() {
            m_renderArenas[write_state].Clear();
            m_renderQueues[write_state].Initialize(m_renderArenas[write_state], 10000);
        }).name("DiligentInit");

        tf::Task extract_task = dag.taskflow.emplace([this, write_state]() {
            auto& registry = m_engine->GetRegistry();
            auto view = registry.View<engine::ecs::TransformComponent, engine::ecs::RenderComponent>();
            for (auto ent : view) {
                const auto& trans = registry.GetComponent<engine::ecs::TransformComponent>(ent);
                const auto& rnd = registry.GetComponent<engine::ecs::RenderComponent>(ent);

                engine::renderer::RenderCommand cmd;
                cmd.transform = trans.matrix;
                cmd.previous_transform = trans.previous_matrix;
                cmd.mesh_id = rnd.mesh_id;
                cmd.texture_id = rnd.texture_id;

                m_renderQueues[write_state].Push(cmd);
            }
        }).name("DiligentExtract");

        dag.phase_reactor.precede(init_task);
        init_task.precede(extract_task);
        extract_task.precede(dag.phase_extract);
    }

    void DiligentModule::Shutdown() {
        ENGINE_ASSERT(m_engine != nullptr, "DiligentModule::Shutdown called before Initialize");

        ENGINE_LOG_INFO("DiligentModule: shutting down");

        m_running.store(false, std::memory_order_release);
        if (m_renderThread.joinable()) {
            m_renderThread.join();
        }

        ENGINE_ASSERT(!m_renderThread.joinable(), "DiligentModule: render thread failed to join at shutdown");

        m_pSwapChain.Release();
        m_pImmediateContext.Release();
        m_pDevice.Release();

        ENGINE_ASSERT(!m_pDevice, "DiligentModule: Vulkan device was not released at shutdown");

        if (m_allocatorInstance) {
            delete static_cast<EngineDiligentAllocator*>(m_allocatorInstance);
            m_allocatorInstance = nullptr;
        }

        ENGINE_LOG_INFO("DiligentModule: Vulkan renderer shut down");
    }

    void DiligentModule::RenderThreadLoop() {
        while (m_running.load(std::memory_order_relaxed)) {
            int read_state;
            float alpha;
            {
                std::lock_guard<std::mutex> lock(m_engine->GetRenderSwapMutex());
                read_state = m_engine->GetRenderReadState();
                alpha = m_engine->GetInterpolationAlpha();
            }

            const auto& queue = m_renderQueues[read_state];

            // Interpolation loop to prepare commands
            for (size_t i = 0; i < queue.GetCount(); ++i) {
                const auto& cmd = queue.GetCommands()[i];

                engine::math::Vector3 pos_prev = cmd.previous_transform.block<3, 1>(0, 3);
                engine::math::Vector3 pos_curr = cmd.transform.block<3, 1>(0, 3);
                engine::math::Vector3 pos_interp = engine::math::Lerp(pos_prev, pos_curr, alpha);

                engine::math::Quaternion rot_prev(cmd.previous_transform.block<3, 3>(0, 0));
                engine::math::Quaternion rot_curr(cmd.transform.block<3, 3>(0, 0));
                engine::math::Quaternion rot_interp = engine::math::Slerp(rot_prev, rot_curr, alpha);

                engine::math::Matrix4 interpolated_matrix = engine::math::Matrix4::Identity();
                interpolated_matrix.block<3, 3>(0, 0) = rot_interp.toRotationMatrix();
                interpolated_matrix.block<3, 1>(0, 3) = pos_interp;

                (void)interpolated_matrix; // stability mapping loop
            }

            // Set Render Target and Clear Screen (Vulkan swap chain operations)
            auto* pRTV = m_pSwapChain->GetCurrentBackBufferRTV();
            auto* pDSV = m_pSwapChain->GetDepthBufferDSV();
            if (pRTV && pDSV) {
                m_pImmediateContext->SetRenderTargets(1, &pRTV, pDSV, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
                const float ClearColor[] = { 0.15f, 0.15f, 0.15f, 1.0f };
                m_pImmediateContext->ClearRenderTarget(pRTV, ClearColor, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
                m_pImmediateContext->ClearDepthStencil(pDSV, Diligent::CLEAR_DEPTH_FLAG, 1.0f, 0, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            }

            m_pSwapChain->Present();
            std::this_thread::yield();
        }
    }

} // namespace engine::renderer

