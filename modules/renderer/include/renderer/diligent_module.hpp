#ifndef ENGINE_RENDERER_DILIGENT_MODULE_HPP
#define ENGINE_RENDERER_DILIGENT_MODULE_HPP

#include "../../src/diligent_resource_pool.hpp"
#include "render_queue.hpp"
#include <atomic>
#include <backends/imgui_impl_glfw.h>
#include <cstddef>
#include <imgui.h>
#include <memory/chained_arena.hpp>
#include <module/module.hpp>
#include <mutex>
#include <thread>
#include <unordered_map>

#include "Common/interface/RefCntAutoPtr.hpp"
#include "Graphics/GraphicsEngine/interface/DeviceContext.h"
#include "Graphics/GraphicsEngine/interface/PipelineResourceSignature.h"
#include "Graphics/GraphicsEngine/interface/RenderDevice.h"
#include "Graphics/GraphicsEngine/interface/SwapChain.h"

namespace engine::renderer {

struct PushConstants {
    engine::math::Matrix4 transform;
    engine::u32 texture_index;
    engine::u32 pad[3]; // Align to 16 bytes
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
    const char* GetName() const override { return "DiligentRenderModule"; }

    bool Initialize(engine::Engine& engine) override;
    void BuildGraph(tf::Taskflow& taskflow) override;
    void CompileFrameGraph(FrameDAG& dag) override;
    void Shutdown() override;

    [[nodiscard]] std::mutex& GetImGuiMutex() { return m_imguiMutex; }

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
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> m_globalSRB;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> m_pushConstants;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> m_cameraConstants;
    Diligent::RefCntAutoPtr<Diligent::ITexture> m_defaultTexture;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> m_defaultInstanceBuffer;

    struct GPUMesh {
        engine::graphics::BufferHandle vertexBuffer;
        engine::graphics::BufferHandle indexBuffer;
        u32 indexCount = 0;
    };
    engine::graphics::PipelineHandle m_defaultPipeline;
    engine::graphics::PipelineHandle m_linePipeline;
    std::unordered_map<uint32_t, GPUMesh> m_gpuMeshes;
    std::unordered_map<uint32_t, engine::graphics::TextureHandle> m_gpuTextures;

    std::mutex m_imguiMutex;
    void* m_pImGui = nullptr; // Abstracted to void* to keep headers clean
    void* m_imguiContext = nullptr;

    void DestroyImGui();
};

} // namespace engine::renderer

#endif // ENGINE_RENDERER_DILIGENT_MODULE_HPP
