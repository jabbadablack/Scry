#include "Imgui/interface/ImGuiImplDiligent.hpp"
#include "renderer/diligent_module.hpp"
#include <GLFW/glfw3.h>
#include <backends/imgui_impl_glfw.h>
#include <debug/assert.h>
#include <debug/logger.hpp>
#include <engine.hpp>
#include <thread>
#include <tracy/Tracy.hpp>

namespace engine::renderer {

void DiligentModule::DestroyImGui() {
    ImGui_ImplGlfw_Shutdown();
    delete static_cast<Diligent::ImGuiImplDiligent*>(m_pImGui);
    m_pImGui = nullptr;
}

void DiligentModule::RenderThreadLoop() {
    if (m_imguiContext != nullptr) {
        ImGui::SetCurrentContext(static_cast<ImGuiContext*>(m_imguiContext));
    }

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
            if (intent.desc.bind == engine::graphics::BufferBind::Vertex ||
                intent.desc.bind == engine::graphics::BufferBind::Instance) {
                BuffDesc.BindFlags = Diligent::BIND_VERTEX_BUFFER;
            } else if (intent.desc.bind == engine::graphics::BufferBind::Index) {
                BuffDesc.BindFlags = Diligent::BIND_INDEX_BUFFER;
            } else if (intent.desc.bind == engine::graphics::BufferBind::Uniform) {
                BuffDesc.BindFlags = Diligent::BIND_UNIFORM_BUFFER;
            } else if (intent.desc.bind == engine::graphics::BufferBind::Indirect) {
                BuffDesc.BindFlags = Diligent::BIND_INDIRECT_DRAW_ARGS;
            }
            BuffDesc.Usage = (intent.desc.usage == engine::graphics::BufferUsage::Static) ? Diligent::USAGE_IMMUTABLE
                                                                                          : Diligent::USAGE_DYNAMIC;
            BuffDesc.CPUAccessFlags =
                (BuffDesc.Usage == Diligent::USAGE_DYNAMIC) ? Diligent::CPU_ACCESS_WRITE : Diligent::CPU_ACCESS_NONE;

            Diligent::BufferData BuffData;
            BuffData.pData = intent.data;
            BuffData.DataSize = intent.desc.size;

            Diligent::RefCntAutoPtr<Diligent::IBuffer> pBuffer;
            m_pDevice->CreateBuffer(BuffDesc, (intent.data != nullptr) ? &BuffData : nullptr, &pBuffer);
            m_buffers.Insert(intent.handle.GetIndex(), intent.handle.GetGeneration(), std::move(pBuffer));
        }

        for (const auto& intent : new_textures) {
            Diligent::TextureDesc TexDesc;
            TexDesc.Name = "DoDTexture";
            TexDesc.Type = Diligent::RESOURCE_DIM_TEX_2D;
            TexDesc.Width = intent.desc.width;
            TexDesc.Height = intent.desc.height;
            TexDesc.Usage = Diligent::USAGE_IMMUTABLE;
            TexDesc.CPUAccessFlags = Diligent::CPU_ACCESS_NONE;
            switch (intent.desc.format) {
                case engine::graphics::Format::RGBA8_UNORM:
                    TexDesc.Format = Diligent::TEX_FORMAT_RGBA8_UNORM;
                    break;
                case engine::graphics::Format::RGBA16_FLOAT:
                    TexDesc.Format = Diligent::TEX_FORMAT_RGBA16_FLOAT;
                    break;
                case engine::graphics::Format::D32_FLOAT:
                    TexDesc.Format = Diligent::TEX_FORMAT_D32_FLOAT;
                    break;
            }

            if (intent.desc.is_render_target) {
                TexDesc.BindFlags = Diligent::BIND_RENDER_TARGET | Diligent::BIND_SHADER_RESOURCE;
            } else if (intent.desc.is_depth_stencil) {
                TexDesc.BindFlags = Diligent::BIND_DEPTH_STENCIL | Diligent::BIND_SHADER_RESOURCE;
            } else {
                TexDesc.BindFlags = Diligent::BIND_SHADER_RESOURCE;
            }

            Diligent::TextureData SubResources;
            Diligent::TextureSubResData Mips[1];
            Mips[0].pData = intent.data;
            engine::u32 bytes_per_pixel = (intent.desc.format == engine::graphics::Format::RGBA16_FLOAT) ? 8 : 4;
            Mips[0].Stride = static_cast<Diligent::Uint64>(intent.desc.width) * bytes_per_pixel;
            SubResources.pSubResources = Mips;
            SubResources.NumSubresources = 1;

            Diligent::RefCntAutoPtr<Diligent::ITexture> pTexture;
            m_pDevice->CreateTexture(TexDesc, (intent.data != nullptr) ? &SubResources : nullptr, &pTexture);
            if (pTexture) {
                auto* pTexSRV = pTexture->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
                Diligent::IDeviceObject* pTexObject = pTexSRV;
                m_globalSRB->GetVariableByName(Diligent::SHADER_TYPE_PIXEL, "g_Textures")
                    ->SetArray(&pTexObject, intent.handle.GetIndex(), 1);
            }
            m_textures.Insert(intent.handle.GetIndex(), intent.handle.GetGeneration(), std::move(pTexture));
        }

        for (const auto& intent : new_pipelines) {
            Diligent::GraphicsPipelineStateCreateInfo PSOCreateInfo;

            Diligent::ShaderCreateInfo ShaderCI;
            ShaderCI.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
            ShaderCI.Desc.UseCombinedTextureSamplers = true;

            Diligent::RefCntAutoPtr<Diligent::IShader> pVS;
            if (intent.desc.vs_source != nullptr) {
                ShaderCI.Desc.ShaderType = Diligent::SHADER_TYPE_VERTEX;
                ShaderCI.Desc.Name = "DoD VS";
                ShaderCI.Source = intent.desc.vs_source;
                m_pDevice->CreateShader(ShaderCI, &pVS);
            }

            Diligent::RefCntAutoPtr<Diligent::IShader> pPS;
            if (intent.desc.ps_source != nullptr) {
                ShaderCI.Desc.ShaderType = Diligent::SHADER_TYPE_PIXEL;
                ShaderCI.Desc.Name = "DoD PS";
                ShaderCI.Source = intent.desc.ps_source;
                m_pDevice->CreateShader(ShaderCI, &pPS);
            }

            PSOCreateInfo.pVS = pVS;
            PSOCreateInfo.pPS = pPS;
            PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode = Diligent::CULL_MODE_NONE;

            switch (intent.desc.topology) {
                case engine::graphics::PrimitiveTopology::TriangleList:
                    PSOCreateInfo.GraphicsPipeline.PrimitiveTopology = Diligent::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
                    break;
                case engine::graphics::PrimitiveTopology::LineList:
                    PSOCreateInfo.GraphicsPipeline.PrimitiveTopology = Diligent::PRIMITIVE_TOPOLOGY_LINE_LIST;
                    break;
            }

            // Define standard Vertex and Instance input layout to match the shader attributes
            static const Diligent::LayoutElement LayoutElems[] = {
                // Vertex layout (slot 0)
                Diligent::LayoutElement{0, 0, 3, Diligent::VT_FLOAT32, false, 0, sizeof(engine::io::Vertex),
                                        Diligent::INPUT_ELEMENT_FREQUENCY_PER_VERTEX},
                Diligent::LayoutElement{1, 0, 3, Diligent::VT_FLOAT32, false, 12, sizeof(engine::io::Vertex),
                                        Diligent::INPUT_ELEMENT_FREQUENCY_PER_VERTEX},
                Diligent::LayoutElement{2, 0, 2, Diligent::VT_FLOAT32, false, 24, sizeof(engine::io::Vertex),
                                        Diligent::INPUT_ELEMENT_FREQUENCY_PER_VERTEX},

                // Hardware Instancing layout (slot 1)
                Diligent::LayoutElement{3, 1, 4, Diligent::VT_FLOAT32, false, 0, 68,
                                        Diligent::INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},
                Diligent::LayoutElement{4, 1, 4, Diligent::VT_FLOAT32, false, 16, 68,
                                        Diligent::INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},
                Diligent::LayoutElement{5, 1, 4, Diligent::VT_FLOAT32, false, 32, 68,
                                        Diligent::INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},
                Diligent::LayoutElement{6, 1, 4, Diligent::VT_FLOAT32, false, 48, 68,
                                        Diligent::INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},
                Diligent::LayoutElement{7, 1, 1, Diligent::VT_UINT32, false, 64, 68,
                                        Diligent::INPUT_ELEMENT_FREQUENCY_PER_INSTANCE}};
            PSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems;
            PSOCreateInfo.GraphicsPipeline.InputLayout.NumElements = 8;

            // Enable Depth Buffer and Depth Testing
            PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = true;
            PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = true;
            PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthFunc = Diligent::COMPARISON_FUNC_LESS_EQUAL;

            PSOCreateInfo.GraphicsPipeline.NumRenderTargets = 1;
            PSOCreateInfo.GraphicsPipeline.RTVFormats[0] = m_pSwapChain->GetDesc().ColorBufferFormat;
            PSOCreateInfo.GraphicsPipeline.DSVFormat = m_pSwapChain->GetDesc().DepthBufferFormat;

            PSOCreateInfo.PSODesc.Name = "DoD PSO";
            PSOCreateInfo.PSODesc.PipelineType = Diligent::PIPELINE_TYPE_GRAPHICS;
            Diligent::IPipelineResourceSignature* ppSignatures[] = {m_globalSignature.RawPtr()};
            PSOCreateInfo.ppResourceSignatures = ppSignatures;
            PSOCreateInfo.ResourceSignaturesCount = 1;

            Diligent::RefCntAutoPtr<Diligent::IPipelineState> pPSO;
            m_pDevice->CreatePipelineState(PSOCreateInfo, &pPSO);
            if (pPSO) {
                m_pipelines.Insert(intent.handle.GetIndex(), intent.handle.GetGeneration(), std::move(pPSO));
            }
        }

        for (const auto& intent : ready_deletions) {
            m_buffers.Remove(intent.handle_id & 0xFFFFF, (intent.handle_id >> 20) & 0xFFF);
            auto* pDefaultSRV = m_defaultTexture->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
            Diligent::IDeviceObject* pDefaultObject = pDefaultSRV;
            m_globalSRB->GetVariableByName(Diligent::SHADER_TYPE_PIXEL, "g_Textures")
                ->SetArray(&pDefaultObject, intent.handle_id & 0xFFFFF, 1);
            m_textures.Remove(intent.handle_id & 0xFFFFF, (intent.handle_id >> 20) & 0xFFF);
            m_pipelines.Remove(intent.handle_id & 0xFFFFF, (intent.handle_id >> 20) & 0xFFF);
        }

        int read_state;
        {
            std::lock_guard<std::mutex> lock(m_engine->GetRenderSwapMutex());
            read_state = m_engine->GetRenderReadState();
        }

        const auto& queue = m_renderQueues[read_state];

        // We must cast away constness locally to sort the lockless double-buffer queue before reading
        const_cast<engine::renderer::RenderQueue&>(queue).Sort();

        // Map camera constants
        void* cameraData = nullptr;
        m_pImmediateContext->MapBuffer(m_cameraConstants, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD, cameraData);
        if (cameraData != nullptr) {
            *static_cast<engine::math::Matrix4*>(cameraData) = queue.GetCameraViewProj();
            m_pImmediateContext->UnmapBuffer(m_cameraConstants, Diligent::MAP_WRITE);
        }

        auto* pMainRTV = m_pSwapChain->GetCurrentBackBufferRTV();
        auto* pMainDSV = m_pSwapChain->GetDepthBufferDSV();
        const float ClearColor[] = {0.15F, 0.15F, 0.15F, 1.0F};

        if (pMainRTV && pMainDSV) {
            // 1. Z-PREPASS
            // Render ONLY to the depth buffer to prime early-Z culling
            m_pImmediateContext->SetRenderTargets(0, nullptr, pMainDSV,
                                                  Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            m_pImmediateContext->ClearDepthStencil(pMainDSV, Diligent::CLEAR_DEPTH_FLAG, 1.0F, 0,
                                                   Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

            m_pImmediateContext->CommitShaderResources(m_globalSRB,
                                                       Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            DispatchPackets(queue, engine::graphics::RenderPass::ZPrePass);

            // 2. OPAQUE (G-BUFFER / FORWARD)
            // Re-bind the color target, keeping the primed depth buffer
            m_pImmediateContext->SetRenderTargets(1, &pMainRTV, pMainDSV,
                                                  Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            m_pImmediateContext->ClearRenderTarget(pMainRTV, ClearColor,
                                                   Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            DispatchPackets(queue, engine::graphics::RenderPass::Opaque);

            // 3. TRANSPARENT
            DispatchPackets(queue, engine::graphics::RenderPass::Transparent);

            // 4. POST-PROCESS
            // (When you implement offscreen HDR buffers, you will bind the Swapchain RTV here
            // and draw a fullscreen quad reading from your offscreen HDR texture).
            DispatchPackets(queue, engine::graphics::RenderPass::PostProcess);

            if (m_engine->IsProfilerActive() && m_pImGui != nullptr) {
                std::lock_guard<std::mutex> lock(m_imguiMutex);

                auto* pImGui = static_cast<Diligent::ImGuiImplDiligent*>(m_pImGui);
                ImGui_ImplGlfw_NewFrame();
                pImGui->NewFrame(m_pSwapChain->GetDesc().Width, m_pSwapChain->GetDesc().Height,
                                 m_pSwapChain->GetDesc().PreTransform);

                if (ImGui::Begin("Engine Profiler")) {
                    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
                    ImGui::Text("Delta Time: %.4f ms", m_engine->GetTime().GetDeltaTime() * 1000.0);
                    ImGui::Separator();
                    ImGui::Text("Memory: %zu MB", engine::TrackedHeap::GetCurrentUsage() / (1024 * 1024));
                    ImGui::Text("Peak Memory: %zu MB", engine::TrackedHeap::GetPeakUsage() / (1024 * 1024));
                    ImGui::Separator();
                    ImGui::Text("DAG Nodes: %zu", m_engine->GetTaskflow().num_tasks());
                }
                ImGui::End();

                // Explicitly bind the backbuffer RTV and DSV for ImGui rendering
                m_pImmediateContext->SetRenderTargets(1, &pMainRTV, pMainDSV,
                                                       Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
                pImGui->Render(m_pImmediateContext);
            }
        }

        m_pSwapChain->Present();
        m_frameCount++;
        std::this_thread::yield();
        FrameMark;
    }
}

void DiligentModule::DispatchPackets(const engine::renderer::RenderQueue& queue,
                                     engine::graphics::RenderPass targetPass) {
    ZoneScopedN("DispatchPackets");
    for (size_t i = 0; i < queue.GetCount(); ++i) {
        const auto& packet = queue.GetCommands()[i];
        if (packet.pass != targetPass)
            continue; // Queue is sorted, but we loop for safety

        auto* pso = m_pipelines.Get(packet.pipeline.GetIndex(), packet.pipeline.GetGeneration());
        if (pso == nullptr)
            continue;

        m_pImmediateContext->SetPipelineState(pso);

        void* mappedData = nullptr;
        m_pImmediateContext->MapBuffer(m_pushConstants, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD, mappedData);
        if (mappedData != nullptr) {
            auto* pc = static_cast<PushConstants*>(mappedData);
            pc->transform = packet.transform;
            pc->texture_index = packet.texture.IsValid() ? packet.texture.GetIndex() : 0;
            m_pImmediateContext->UnmapBuffer(m_pushConstants, Diligent::MAP_WRITE);
        }

        Diligent::IBuffer* pBuffs[2] = {nullptr, nullptr};
        Diligent::Uint64 offsets[2] = {0, 0};
        engine::u32 num_buffers = 0;

        auto* vBuffer = m_buffers.Get(packet.vertex_buffer.GetIndex(), packet.vertex_buffer.GetGeneration());
        if (vBuffer != nullptr) {
            pBuffs[0] = vBuffer;
            num_buffers = 1;

            auto* instBuffer = m_buffers.Get(packet.instance_buffer.GetIndex(), packet.instance_buffer.GetGeneration());
            if (instBuffer != nullptr) {
                pBuffs[1] = instBuffer;
                num_buffers = 2;
            } else {
                pBuffs[1] = m_defaultInstanceBuffer;
                num_buffers = 2;
            }
            m_pImmediateContext->SetVertexBuffers(0, num_buffers, pBuffs, offsets,
                                                  Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                                                  Diligent::SET_VERTEX_BUFFERS_FLAG_RESET);
        }

        auto* iBuffer = m_buffers.Get(packet.index_buffer.GetIndex(), packet.index_buffer.GetGeneration());
        if (iBuffer != nullptr) {
            m_pImmediateContext->SetIndexBuffer(iBuffer, 0, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

            auto* indirectBuffer =
                m_buffers.Get(packet.indirect_buffer.GetIndex(), packet.indirect_buffer.GetGeneration());
            if (indirectBuffer != nullptr) {
                Diligent::DrawIndexedIndirectAttribs DrawAttrs;
                DrawAttrs.pAttribsBuffer = indirectBuffer;
                DrawAttrs.DrawArgsOffset = packet.indirect_offset;
                DrawAttrs.IndexType = Diligent::VT_UINT32;
                DrawAttrs.Flags = Diligent::DRAW_FLAG_VERIFY_ALL;
                m_pImmediateContext->DrawIndexedIndirect(DrawAttrs);
            } else {
                Diligent::DrawIndexedAttribs DrawAttrs;
                DrawAttrs.NumIndices = packet.index_count;
                DrawAttrs.IndexType = Diligent::VT_UINT32;
                DrawAttrs.Flags = Diligent::DRAW_FLAG_VERIFY_ALL;
                m_pImmediateContext->DrawIndexed(DrawAttrs);
            }
        }
    }
}

} // namespace engine::renderer
