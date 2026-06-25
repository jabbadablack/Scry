#ifndef ENGINE_RENDERER_DILIGENT_MODULE_HPP
#define ENGINE_RENDERER_DILIGENT_MODULE_HPP

#include <cstddef>
#include <module/module.hpp>
#include <memory/chained_arena.hpp>
#include "render_queue.hpp"
#include "../../src/diligent_resource_pool.hpp"
#include <thread>
#include <mutex>
#include <atomic>
#include <unordered_map>

#include "Graphics/GraphicsEngine/interface/RenderDevice.h"
#include "Graphics/GraphicsEngine/interface/DeviceContext.h"
#include "Graphics/GraphicsEngine/interface/SwapChain.h"
#include "Graphics/GraphicsEngine/interface/PipelineResourceSignature.h"
#include "Common/interface/RefCntAutoPtr.hpp"


namespace engine::renderer {

    struct PushConstants {
        engine::math::Matrix4 transform;
        engine::u32           texture_index;
        engine::u32           pad[3]; // Align to 16 bytes
    };

    class DiligentModule : public engine::IModule {
    public:
        DiligentModule()
            : m_renderArenas{engine::ChainedArena(static_cast<long long>(64) * 1024),
                             engine::ChainedArena(static_cast<long long>(64) * 1024)} {}
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
        void DispatchPackets(const engine::renderer::RenderQueue& queue, engine::graphics::RenderPass targetPass);

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

        ResourcePool<Diligent::IBuffer> m_buffers;
        ResourcePool<Diligent::ITexture> m_textures;
        ResourcePool<Diligent::IPipelineState> m_pipelines;
        u64 m_frameCount = 0;

        Diligent::RefCntAutoPtr<Diligent::IPipelineResourceSignature> m_globalSignature;
        Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding>     m_globalSRB;
        Diligent::RefCntAutoPtr<Diligent::IBuffer>                    m_pushConstants;
        Diligent::RefCntAutoPtr<Diligent::ITexture>                   m_dummyTexture;
    };

} // namespace engine::renderer


#endif // ENGINE_RENDERER_DILIGENT_MODULE_HPP
