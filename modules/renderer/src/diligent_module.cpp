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
#include "Graphics/GraphicsEngine/interface/Shader.h"


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
        if (pFactoryVk == nullptr) {
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

                engine::graphics::RenderPacket packet;
                packet.transform = trans.matrix;
                packet.pipeline = {};
                packet.vertex_buffer = {};
                packet.index_buffer = {};
                packet.texture = {};
                packet.index_count = 0;

                m_renderQueues[write_state].Push(packet);
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

        if (m_allocatorInstance != nullptr) {
            delete static_cast<EngineDiligentAllocator*>(m_allocatorInstance);
            m_allocatorInstance = nullptr;
        }

        ENGINE_LOG_INFO("DiligentModule: Vulkan renderer shut down");
    }

    void DiligentModule::RenderThreadLoop() {
        while (m_running.load(std::memory_order_relaxed)) {
            // Process resource intents before locking swap mutex
            std::vector<engine::graphics::BufferIntent> new_buffers;
            std::vector<engine::graphics::TextureIntent> new_textures;
            std::vector<engine::graphics::PipelineIntent> new_pipelines;
            std::vector<engine::graphics::DeletionIntent> ready_deletions;

            m_engine->GetIGraphics().FlushCreations(new_buffers, new_textures, new_pipelines);
            m_engine->GetIGraphics().FlushDeletions(m_frameCount, ready_deletions);

            for (const auto& intent : new_buffers) {
                Diligent::BufferDesc BuffDesc;
                BuffDesc.Name = "DoDBuffer";
                BuffDesc.Size = intent.desc.size;
                BuffDesc.BindFlags = (intent.desc.bind == engine::graphics::BufferBind::Vertex) ? Diligent::BIND_VERTEX_BUFFER : Diligent::BIND_INDEX_BUFFER;
                BuffDesc.Usage = (intent.desc.usage == engine::graphics::BufferUsage::Static) ? Diligent::USAGE_IMMUTABLE : Diligent::USAGE_DYNAMIC;

                Diligent::BufferData BuffData;
                BuffData.pData = intent.data;
                BuffData.DataSize = intent.desc.size;

                Diligent::RefCntAutoPtr<Diligent::IBuffer> pBuffer;
                m_pDevice->CreateBuffer(BuffDesc, intent.data ? &BuffData : nullptr, &pBuffer);
                m_buffers.Insert(intent.handle.GetIndex(), intent.handle.GetGeneration(), std::move(pBuffer));
            }

            for (const auto& intent : new_textures) {
                Diligent::TextureDesc TexDesc;
                TexDesc.Name = "DoDTexture";
                TexDesc.Type = Diligent::RESOURCE_DIM_TEX_2D;
                TexDesc.Width = intent.desc.width;
                TexDesc.Height = intent.desc.height;
                if (intent.desc.channels == 4) {
                    TexDesc.Format = Diligent::TEX_FORMAT_RGBA8_UNORM;
                } else if (intent.desc.channels == 1) {
                    TexDesc.Format = Diligent::TEX_FORMAT_R8_UNORM;
                } else {
                    TexDesc.Format = Diligent::TEX_FORMAT_RGBA8_UNORM;
                }
                TexDesc.BindFlags = intent.desc.is_render_target ? Diligent::BIND_RENDER_TARGET : Diligent::BIND_SHADER_RESOURCE;

                Diligent::TextureData SubResources;
                Diligent::TextureSubResData Mips[1];
                Mips[0].pData = intent.data;
                Mips[0].Stride = intent.desc.width * intent.desc.channels;
                SubResources.pSubResources = Mips;
                SubResources.NumSubresources = 1;

                Diligent::RefCntAutoPtr<Diligent::ITexture> pTexture;
                m_pDevice->CreateTexture(TexDesc, intent.data ? &SubResources : nullptr, &pTexture);
                m_textures.Insert(intent.handle.GetIndex(), intent.handle.GetGeneration(), std::move(pTexture));
            }

            for (const auto& intent : new_pipelines) {
                Diligent::GraphicsPipelineStateCreateInfo PSOCreateInfo;

                Diligent::ShaderCreateInfo ShaderCI;
                ShaderCI.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
                ShaderCI.Desc.UseCombinedTextureSamplers = true;

                Diligent::RefCntAutoPtr<Diligent::IShader> pVS;
                if (intent.desc.vs_source) {
                    ShaderCI.Desc.ShaderType = Diligent::SHADER_TYPE_VERTEX;
                    ShaderCI.Desc.Name = "DoD VS";
                    ShaderCI.Source = intent.desc.vs_source;
                    m_pDevice->CreateShader(ShaderCI, &pVS);
                }

                Diligent::RefCntAutoPtr<Diligent::IShader> pPS;
                if (intent.desc.ps_source) {
                    ShaderCI.Desc.ShaderType = Diligent::SHADER_TYPE_PIXEL;
                    ShaderCI.Desc.Name = "DoD PS";
                    ShaderCI.Source = intent.desc.ps_source;
                    m_pDevice->CreateShader(ShaderCI, &pPS);
                }

                PSOCreateInfo.pVS = pVS;
                PSOCreateInfo.pPS = pPS;
                PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode = Diligent::CULL_MODE_NONE;
                PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = false;

                PSOCreateInfo.GraphicsPipeline.NumRenderTargets = 1;
                PSOCreateInfo.GraphicsPipeline.RTVFormats[0] = m_pSwapChain->GetDesc().ColorBufferFormat;
                PSOCreateInfo.GraphicsPipeline.DSVFormat = m_pSwapChain->GetDesc().DepthBufferFormat;

                PSOCreateInfo.PSODesc.Name = "DoD PSO";
                PSOCreateInfo.PSODesc.PipelineType = Diligent::PIPELINE_TYPE_GRAPHICS;

                Diligent::RefCntAutoPtr<Diligent::IPipelineState> pPSO;
                m_pDevice->CreatePipelineState(PSOCreateInfo, &pPSO);
                if (pPSO) {
                    m_pipelines.Insert(intent.handle.GetIndex(), intent.handle.GetGeneration(), std::move(pPSO));
                }
            }

            for (const auto& intent : ready_deletions) {
                m_buffers.Remove(intent.handle_id & 0xFFFFF, (intent.handle_id >> 20) & 0xFFF);
                m_textures.Remove(intent.handle_id & 0xFFFFF, (intent.handle_id >> 20) & 0xFFF);
                m_pipelines.Remove(intent.handle_id & 0xFFFFF, (intent.handle_id >> 20) & 0xFFF);
            }

            int read_state;
            {
                std::lock_guard<std::mutex> lock(m_engine->GetRenderSwapMutex());
                read_state = m_engine->GetRenderReadState();
            }

            const auto& queue = m_renderQueues[read_state];

            // Set Render Target and Clear Screen (Vulkan swap chain operations)
            auto* pRTV = m_pSwapChain->GetCurrentBackBufferRTV();
            auto* pDSV = m_pSwapChain->GetDepthBufferDSV();
            if ((pRTV != nullptr) && (pDSV != nullptr)) {
                m_pImmediateContext->SetRenderTargets(1, &pRTV, pDSV, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
                const float ClearColor[] = { 0.15F, 0.15F, 0.15F, 1.0F };
                m_pImmediateContext->ClearRenderTarget(pRTV, ClearColor, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
                m_pImmediateContext->ClearDepthStencil(pDSV, Diligent::CLEAR_DEPTH_FLAG, 1.0F, 0, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            }

            // Draw loop processing Intent-Based Packets
            for (size_t i = 0; i < queue.GetCount(); ++i) {
                const auto& packet = queue.GetCommands()[i];

                auto* pso = m_pipelines.Get(packet.pipeline.GetIndex(), packet.pipeline.GetGeneration());
                if (!pso) continue;

                m_pImmediateContext->SetPipelineState(pso);

                // Bind SRB (Shader Resource Binding)
                auto* srb = m_srbs.Get(packet.pipeline.GetIndex(), packet.pipeline.GetGeneration());
                if (srb) m_pImmediateContext->CommitShaderResources(srb, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

                // Bind Vertex/Index Buffers
                auto* vBuffer = m_buffers.Get(packet.vertex_buffer.GetIndex(), packet.vertex_buffer.GetGeneration());
                if (vBuffer) {
                    Diligent::IBuffer* pBuffs[] = {vBuffer};
                    Diligent::Uint64 offsets[] = {0};
                    m_pImmediateContext->SetVertexBuffers(0, 1, pBuffs, offsets, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION, Diligent::SET_VERTEX_BUFFERS_FLAG_RESET);
                }

                auto* iBuffer = m_buffers.Get(packet.index_buffer.GetIndex(), packet.index_buffer.GetGeneration());
                if (iBuffer) {
                    m_pImmediateContext->SetIndexBuffer(iBuffer, 0, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
                    Diligent::DrawIndexedAttribs DrawAttrs;
                    DrawAttrs.NumIndices = packet.index_count;
                    DrawAttrs.IndexType = Diligent::VT_UINT32;
                    DrawAttrs.Flags = Diligent::DRAW_FLAG_VERIFY_ALL;
                    m_pImmediateContext->DrawIndexed(DrawAttrs);
                }
            }

            m_pSwapChain->Present();
            m_frameCount++;
            std::this_thread::yield();
        }
    }

} // namespace engine::renderer

