#ifndef ENGINE_RENDERER_DILIGENT_MODULE_HPP
#define ENGINE_RENDERER_DILIGENT_MODULE_HPP

#include <module.hpp>
#include <memory/chained_arena.hpp>
#include "render_queue.hpp"
#include <thread>
#include <mutex>
#include <atomic>
#include <unordered_map>

#include "Graphics/GraphicsEngine/interface/RenderDevice.h"
#include "Graphics/GraphicsEngine/interface/DeviceContext.h"
#include "Graphics/GraphicsEngine/interface/SwapChain.h"
#include "Common/interface/RefCntAutoPtr.hpp"

namespace engine {
namespace renderer {

    class DiligentModule : public engine::IModule {
    public:
        DiligentModule()
            : m_renderArenas{engine::ChainedArena(64 * 1024),
                             engine::ChainedArena(64 * 1024)} {}
        ~DiligentModule() override = default;

        // Disable copy and move semantics
        DiligentModule(const DiligentModule&) = delete;
        DiligentModule& operator=(const DiligentModule&) = delete;
        DiligentModule(DiligentModule&&) = delete;
        DiligentModule& operator=(DiligentModule&&) = delete;

        // IModule interface implementation
        const char* GetName() const override {
            return "DiligentRenderModule";
        }

        bool Initialize(engine::Engine& engine) override;
        void BuildGraph(tf::Taskflow& taskflow) override;
        void CompileFrameGraph(FrameDAG& dag) override;
        void Shutdown() override;

    private:
        void RenderThreadLoop();

        // Diligent Engine pointers
        Diligent::RefCntAutoPtr<Diligent::IRenderDevice> m_pDevice;
        Diligent::RefCntAutoPtr<Diligent::IDeviceContext> m_pImmediateContext;
        Diligent::RefCntAutoPtr<Diligent::ISwapChain> m_pSwapChain;

        void* m_allocatorInstance = nullptr;

        // Module-owned render resources (decoupled from the core engine)
        engine::ChainedArena m_renderArenas[2];
        engine::renderer::RenderQueue m_renderQueues[2];

        // Threading and engine context
        std::thread m_renderThread;
        std::atomic<bool> m_running{false};
        engine::Engine* m_engine = nullptr;
    };

} // namespace renderer
} // namespace engine

#endif // ENGINE_RENDERER_DILIGENT_MODULE_HPP
