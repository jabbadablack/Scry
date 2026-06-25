#include "renderer/diligent_module.hpp"
#include "diligent_allocator.hpp"
#include <cstddef>
#include <engine.hpp>
#include <OS/IWindow.hpp>
#include <ecs/components.hpp>
#include <math/interpolation.hpp>
#include <debug/logger.hpp>
#include <debug/assert.h>
#include <graphics/standard_shaders.hpp>

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
        pFactoryVk->SetMemoryAllocator(custom_allocator);

        Diligent::EngineVkCreateInfo EngineCI{};

        EngineCI.DynamicHeapSize     = 2 * 1024 * 1024; // 2MB total dynamic heap
        EngineCI.DynamicHeapPageSize = 256 * 1024;      // 256KB page sizes

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

        // Configure Global Bindless Setup
        Diligent::PipelineResourceDesc Resources[] = {
            { Diligent::SHADER_TYPE_VERTEX | Diligent::SHADER_TYPE_PIXEL, "PushConstants", 1, Diligent::SHADER_RESOURCE_TYPE_CONSTANT_BUFFER, Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC },
            { Diligent::SHADER_TYPE_VERTEX | Diligent::SHADER_TYPE_PIXEL, "CameraConstants", 1, Diligent::SHADER_RESOURCE_TYPE_CONSTANT_BUFFER, Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC },
            { Diligent::SHADER_TYPE_PIXEL, "g_Textures", 1024, Diligent::SHADER_RESOURCE_TYPE_TEXTURE_SRV, Diligent::SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE },
            { Diligent::SHADER_TYPE_PIXEL, "g_Textures_sampler", 1, Diligent::SHADER_RESOURCE_TYPE_SAMPLER, Diligent::SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE }
        };

        Diligent::PipelineResourceSignatureDesc PRSDesc;
        PRSDesc.Name         = "Global Bindless Signature";
        PRSDesc.Resources    = Resources;
        PRSDesc.NumResources = 4;
        PRSDesc.BindingIndex = 0;

        m_pDevice->CreatePipelineResourceSignature(PRSDesc, &m_globalSignature);
        ENGINE_ASSERT(m_globalSignature != nullptr, "Failed to create Global Bindless Signature");

        m_globalSignature->CreateShaderResourceBinding(&m_globalSRB, true);
        ENGINE_ASSERT(m_globalSRB != nullptr, "Failed to create Global SRB");

        Diligent::BufferDesc CBDesc;
        CBDesc.Name           = "PushConstants CB";
        CBDesc.Size           = sizeof(PushConstants);
        CBDesc.Usage          = Diligent::USAGE_DYNAMIC;
        CBDesc.BindFlags      = Diligent::BIND_UNIFORM_BUFFER;
        CBDesc.CPUAccessFlags = Diligent::CPU_ACCESS_WRITE;
        m_pDevice->CreateBuffer(CBDesc, nullptr, &m_pushConstants);
        ENGINE_ASSERT(m_pushConstants != nullptr, "Failed to create PushConstants buffer");

        m_globalSRB->GetVariableByName(Diligent::SHADER_TYPE_VERTEX, "PushConstants")->Set(m_pushConstants);

        Diligent::BufferDesc CamDesc;
        CamDesc.Name           = "CameraConstants CB";
        CamDesc.Size           = sizeof(engine::math::Matrix4);
        CamDesc.Usage          = Diligent::USAGE_DYNAMIC;
        CamDesc.BindFlags      = Diligent::BIND_UNIFORM_BUFFER;
        CamDesc.CPUAccessFlags = Diligent::CPU_ACCESS_WRITE;
        m_pDevice->CreateBuffer(CamDesc, nullptr, &m_cameraConstants);
        ENGINE_ASSERT(m_cameraConstants != nullptr, "Failed to create CameraConstants buffer");

        m_globalSRB->GetVariableByName(Diligent::SHADER_TYPE_VERTEX, "CameraConstants")->Set(m_cameraConstants);

        // Bind default sampler to the shader resource binding
        Diligent::RefCntAutoPtr<Diligent::ISampler> pSampler;
        Diligent::SamplerDesc SamDesc;
        m_pDevice->CreateSampler(SamDesc, &pSampler);
        m_globalSRB->GetVariableByName(Diligent::SHADER_TYPE_PIXEL, "g_Textures_sampler")->Set(pSampler);

        // Initialize a 1x1 default texture to fill the array and prevent Vulkan partially-bound validation crashes
        Diligent::TextureDesc DefaultDesc;
        DefaultDesc.Name      = "Bindless Default Texture";
        DefaultDesc.Type      = Diligent::RESOURCE_DIM_TEX_2D;
        DefaultDesc.Width     = 1;
        DefaultDesc.Height    = 1;
        DefaultDesc.Format    = Diligent::TEX_FORMAT_RGBA8_UNORM;
        DefaultDesc.BindFlags = Diligent::BIND_SHADER_RESOURCE;

        engine::u32 DefaultPixel = 0xFFFFFFFF; // White pixel
        Diligent::TextureSubResData Mips[1] = { { &DefaultPixel, 4 } };
        Diligent::TextureData InitData(Mips, 1);
        m_pDevice->CreateTexture(DefaultDesc, &InitData, &m_defaultTexture);
        ENGINE_ASSERT(m_defaultTexture != nullptr, "Failed to create Default Texture");

        auto* pDefaultSRV = m_defaultTexture->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
        Diligent::IDeviceObject* pDefaultObject = pDefaultSRV;
        auto* pTexVar   = m_globalSRB->GetVariableByName(Diligent::SHADER_TYPE_PIXEL, "g_Textures");
        for (engine::u32 i = 0; i < 1024; ++i) {
            pTexVar->SetArray(&pDefaultObject, i, 1);
        }

        // Create Default Instance Buffer to satisfy Vulkan/Diligent binding when instancing is not used
        Diligent::BufferDesc InstDesc;
        InstDesc.Name           = "Default Instance Buffer";
        InstDesc.Size           = 256;
        InstDesc.Usage          = Diligent::USAGE_DEFAULT;
        InstDesc.BindFlags      = Diligent::BIND_VERTEX_BUFFER;
        m_pDevice->CreateBuffer(InstDesc, nullptr, &m_defaultInstanceBuffer);
        ENGINE_ASSERT(m_defaultInstanceBuffer != nullptr, "Failed to create Default Instance Buffer");

        // Initialize Default Pipeline PSO
        engine::graphics::PipelineDesc pipeDesc;
        pipeDesc.vs_source = engine::graphics::shaders::UberPBR_VS;
        pipeDesc.ps_source = engine::graphics::shaders::UberPBR_PS;
        pipeDesc.topology  = engine::graphics::PrimitiveTopology::TriangleList;
        m_defaultPipeline = engine.GetIGraphics().CreatePipeline(pipeDesc);

        // Initialize Line Pipeline PSO
        engine::graphics::PipelineDesc linePipeDesc;
        linePipeDesc.vs_source = engine::graphics::shaders::UberPBR_VS;
        linePipeDesc.ps_source = engine::graphics::shaders::UberPBR_PS;
        linePipeDesc.topology  = engine::graphics::PrimitiveTopology::LineList;
        m_linePipeline = engine.GetIGraphics().CreatePipeline(linePipeDesc);

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

            // 1. Extract Camera
            engine::math::Matrix4 viewProj = engine::math::Matrix4::Identity();
            auto camView = registry.View<engine::ecs::CameraComponent>();
            for (auto ent : camView) {
                const auto& cam = registry.GetComponent<engine::ecs::CameraComponent>(ent);
                if (cam.is_active) {
                    viewProj = cam.view_proj;
                    break;
                }
            }
            m_renderQueues[write_state].SetCameraViewProj(viewProj);

            // 2. DAG Hierarchy Traversal (Only extract if inside a Level)
            std::vector<engine::ecs::Entity> stack;
            auto tagView = registry.View<engine::ecs::TagComponent>();
            for (auto ent : tagView) {
                if (registry.GetComponent<engine::ecs::TagComponent>(ent).tag == engine::ecs::Hash("Level")) {
                    stack.push_back(ent);
                }
            }

            auto& resourceMgr = m_engine->GetResourceManager();

            while (!stack.empty()) {
                auto curr = stack.back();
                stack.pop_back();

                if (registry.HasComponent<engine::ecs::RenderComponent>(curr) && registry.HasComponent<engine::ecs::TransformComponent>(curr)) {
                    const auto& trans = registry.GetComponent<engine::ecs::TransformComponent>(curr);
                    const auto& render = registry.GetComponent<engine::ecs::RenderComponent>(curr);

                    engine::graphics::RenderPacket packet;
                    packet.transform = trans.matrix;
                    packet.pipeline = (render.topology == engine::graphics::PrimitiveTopology::LineList) ? m_linePipeline : m_defaultPipeline;

                    // Mesh Buffers
                    engine::graphics::BufferHandle vertexBuffer{};
                    engine::graphics::BufferHandle indexBuffer{};
                    uint32_t indexCount = 0;

                    if (resourceMgr.IsMeshLoaded(render.mesh_id)) {
                        auto it = m_gpuMeshes.find(render.mesh_id.value());
                        if (it != m_gpuMeshes.end()) {
                            vertexBuffer = it->second.vertexBuffer;
                            indexBuffer = it->second.indexBuffer;
                            indexCount = it->second.indexCount;
                        } else {
                            auto mesh_res = resourceMgr.GetMesh(render.mesh_id);
                            if (mesh_res) {
                                engine::graphics::BufferDesc vertexDesc;
                                vertexDesc.size = static_cast<uint32_t>(mesh_res->vertices.size() * sizeof(engine::io::Vertex));
                                vertexDesc.bind = engine::graphics::BufferBind::Vertex;
                                vertexDesc.usage = engine::graphics::BufferUsage::Static;
                                vertexBuffer = m_engine->GetIGraphics().CreateBuffer(vertexDesc, mesh_res->vertices.data());

                                engine::graphics::BufferDesc indexDesc;
                                indexDesc.size = static_cast<uint32_t>(mesh_res->indices.size() * sizeof(uint32_t));
                                indexDesc.bind = engine::graphics::BufferBind::Index;
                                indexDesc.usage = engine::graphics::BufferUsage::Static;
                                indexBuffer = m_engine->GetIGraphics().CreateBuffer(indexDesc, mesh_res->indices.data());

                                indexCount = static_cast<uint32_t>(mesh_res->indices.size());

                                m_gpuMeshes[render.mesh_id.value()] = {vertexBuffer, indexBuffer, indexCount};
                            }
                        }
                    }

                    packet.vertex_buffer = vertexBuffer;
                    packet.index_buffer = indexBuffer;
                    packet.index_count = indexCount;

                    // Texture Handle
                    engine::graphics::TextureHandle textureHandle{};
                    if (render.texture_id.value() != 0 && resourceMgr.IsTextureLoaded(render.texture_id)) {
                        auto it = m_gpuTextures.find(render.texture_id.value());
                        if (it != m_gpuTextures.end()) {
                            textureHandle = it->second;
                        } else {
                            auto tex_res = resourceMgr.GetTexture(render.texture_id);
                            if (tex_res) {
                                engine::graphics::TextureDesc texDesc;
                                texDesc.width = tex_res->width;
                                texDesc.height = tex_res->height;
                                texDesc.format = (tex_res->channels == 8) ? engine::graphics::Format::RGBA16_FLOAT : engine::graphics::Format::RGBA8_UNORM;
                                texDesc.is_render_target = false;
                                texDesc.is_depth_stencil = false;
                                textureHandle = m_engine->GetIGraphics().CreateTexture(texDesc, tex_res->data.get());

                                m_gpuTextures[render.texture_id.value()] = textureHandle;
                            }
                        }
                    }
                    packet.texture = textureHandle;

                    m_renderQueues[write_state].Push(packet);
                }

                if (registry.HasComponent<engine::ecs::HierarchyComponent>(curr)) {
                    auto child = registry.GetComponent<engine::ecs::HierarchyComponent>(curr).first_child;
                    while (registry.GetRawRegistry().valid(child)) {
                        stack.push_back(child);
                        if (registry.HasComponent<engine::ecs::HierarchyComponent>(child)) {
                            child = registry.GetComponent<engine::ecs::HierarchyComponent>(child).next_sibling;
                        } else {
                            break;
                        }
                    }
                }
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

        m_defaultTexture.Release();
        m_defaultInstanceBuffer.Release();
        m_cameraConstants.Release();
        m_pushConstants.Release();
        m_globalSRB.Release();
        m_globalSignature.Release();

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
                if (intent.desc.bind == engine::graphics::BufferBind::Vertex || intent.desc.bind == engine::graphics::BufferBind::Instance) {
                    BuffDesc.BindFlags = Diligent::BIND_VERTEX_BUFFER;
                } else if (intent.desc.bind == engine::graphics::BufferBind::Index) {
                    BuffDesc.BindFlags = Diligent::BIND_INDEX_BUFFER;
                } else if (intent.desc.bind == engine::graphics::BufferBind::Uniform) {
                    BuffDesc.BindFlags = Diligent::BIND_UNIFORM_BUFFER;
                } else if (intent.desc.bind == engine::graphics::BufferBind::Indirect) {
                    BuffDesc.BindFlags = Diligent::BIND_INDIRECT_DRAW_ARGS;
                }
                BuffDesc.Usage          = (intent.desc.usage == engine::graphics::BufferUsage::Static) ? Diligent::USAGE_IMMUTABLE : Diligent::USAGE_DYNAMIC;
                BuffDesc.CPUAccessFlags = (BuffDesc.Usage == Diligent::USAGE_DYNAMIC) ? Diligent::CPU_ACCESS_WRITE : Diligent::CPU_ACCESS_NONE;

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
                TexDesc.Usage          = Diligent::USAGE_IMMUTABLE;
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
                    m_globalSRB->GetVariableByName(Diligent::SHADER_TYPE_PIXEL, "g_Textures")->SetArray(&pTexObject, intent.handle.GetIndex(), 1);
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
                    Diligent::LayoutElement{0, 0, 3, Diligent::VT_FLOAT32, false, 0, sizeof(engine::io::Vertex), Diligent::INPUT_ELEMENT_FREQUENCY_PER_VERTEX},
                    Diligent::LayoutElement{1, 0, 3, Diligent::VT_FLOAT32, false, 12, sizeof(engine::io::Vertex), Diligent::INPUT_ELEMENT_FREQUENCY_PER_VERTEX},
                    Diligent::LayoutElement{2, 0, 2, Diligent::VT_FLOAT32, false, 24, sizeof(engine::io::Vertex), Diligent::INPUT_ELEMENT_FREQUENCY_PER_VERTEX},

                    // Hardware Instancing layout (slot 1)
                    Diligent::LayoutElement{3, 1, 4, Diligent::VT_FLOAT32, false, 0, 68, Diligent::INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},
                    Diligent::LayoutElement{4, 1, 4, Diligent::VT_FLOAT32, false, 16, 68, Diligent::INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},
                    Diligent::LayoutElement{5, 1, 4, Diligent::VT_FLOAT32, false, 32, 68, Diligent::INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},
                    Diligent::LayoutElement{6, 1, 4, Diligent::VT_FLOAT32, false, 48, 68, Diligent::INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},
                    Diligent::LayoutElement{7, 1, 1, Diligent::VT_UINT32, false, 64, 68, Diligent::INPUT_ELEMENT_FREQUENCY_PER_INSTANCE}
                };
                PSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems;
                PSOCreateInfo.GraphicsPipeline.InputLayout.NumElements    = 8;

                // Enable Depth Buffer and Depth Testing
                PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = true;
                PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = true;
                PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthFunc = Diligent::COMPARISON_FUNC_LESS_EQUAL;

                PSOCreateInfo.GraphicsPipeline.NumRenderTargets = 1;
                PSOCreateInfo.GraphicsPipeline.RTVFormats[0] = m_pSwapChain->GetDesc().ColorBufferFormat;
                PSOCreateInfo.GraphicsPipeline.DSVFormat = m_pSwapChain->GetDesc().DepthBufferFormat;

                PSOCreateInfo.PSODesc.Name = "DoD PSO";
                PSOCreateInfo.PSODesc.PipelineType = Diligent::PIPELINE_TYPE_GRAPHICS;
                Diligent::IPipelineResourceSignature* ppSignatures[] = { m_globalSignature.RawPtr() };
                PSOCreateInfo.ppResourceSignatures                   = ppSignatures;
                PSOCreateInfo.ResourceSignaturesCount                = 1;

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
                m_globalSRB->GetVariableByName(Diligent::SHADER_TYPE_PIXEL, "g_Textures")->SetArray(&pDefaultObject, intent.handle_id & 0xFFFFF, 1);
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
            const float ClearColor[] = { 0.15F, 0.15F, 0.15F, 1.0F };

            if (pMainRTV && pMainDSV) {
                // 1. Z-PREPASS
                // Render ONLY to the depth buffer to prime early-Z culling
                m_pImmediateContext->SetRenderTargets(0, nullptr, pMainDSV, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
                m_pImmediateContext->ClearDepthStencil(pMainDSV, Diligent::CLEAR_DEPTH_FLAG, 1.0F, 0, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

                m_pImmediateContext->CommitShaderResources(m_globalSRB, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
                DispatchPackets(queue, engine::graphics::RenderPass::ZPrePass);

                // 2. OPAQUE (G-BUFFER / FORWARD)
                // Re-bind the color target, keeping the primed depth buffer
                m_pImmediateContext->SetRenderTargets(1, &pMainRTV, pMainDSV, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
                m_pImmediateContext->ClearRenderTarget(pMainRTV, ClearColor, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
                DispatchPackets(queue, engine::graphics::RenderPass::Opaque);

                // 3. TRANSPARENT
                DispatchPackets(queue, engine::graphics::RenderPass::Transparent);

                // 4. POST-PROCESS
                // (When you implement offscreen HDR buffers, you will bind the Swapchain RTV here
                // and draw a fullscreen quad reading from your offscreen HDR texture).
                DispatchPackets(queue, engine::graphics::RenderPass::PostProcess);
            }

            m_pSwapChain->Present();
            m_frameCount++;
            std::this_thread::yield();
        }
    }

    void DiligentModule::DispatchPackets(const engine::renderer::RenderQueue& queue, engine::graphics::RenderPass targetPass) {
        for (size_t i = 0; i < queue.GetCount(); ++i) {
            const auto& packet = queue.GetCommands()[i];
            if (packet.pass != targetPass) continue; // Queue is sorted, but we loop for safety

            auto* pso = m_pipelines.Get(packet.pipeline.GetIndex(), packet.pipeline.GetGeneration());
            if (pso == nullptr) continue;

            m_pImmediateContext->SetPipelineState(pso);

            void* mappedData = nullptr;
            m_pImmediateContext->MapBuffer(m_pushConstants, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD, mappedData);
            if (mappedData != nullptr) {
                auto* pc          = static_cast<PushConstants*>(mappedData);
                pc->transform     = packet.transform;
                pc->texture_index = packet.texture.IsValid() ? packet.texture.GetIndex() : 0;
                m_pImmediateContext->UnmapBuffer(m_pushConstants, Diligent::MAP_WRITE);
            }

            Diligent::IBuffer* pBuffs[2]  = { nullptr, nullptr };
            Diligent::Uint64   offsets[2] = { 0, 0 };
            engine::u32        num_buffers = 0;

            auto* vBuffer = m_buffers.Get(packet.vertex_buffer.GetIndex(), packet.vertex_buffer.GetGeneration());
            if (vBuffer != nullptr) {
                pBuffs[0]   = vBuffer;
                num_buffers = 1;

                auto* instBuffer = m_buffers.Get(packet.instance_buffer.GetIndex(), packet.instance_buffer.GetGeneration());
                if (instBuffer != nullptr) {
                    pBuffs[1]   = instBuffer;
                    num_buffers = 2;
                } else {
                    pBuffs[1]   = m_defaultInstanceBuffer;
                    num_buffers = 2;
                }
                m_pImmediateContext->SetVertexBuffers(0, num_buffers, pBuffs, offsets, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION, Diligent::SET_VERTEX_BUFFERS_FLAG_RESET);
            }

            auto* iBuffer = m_buffers.Get(packet.index_buffer.GetIndex(), packet.index_buffer.GetGeneration());
            if (iBuffer != nullptr) {
                m_pImmediateContext->SetIndexBuffer(iBuffer, 0, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

                auto* indirectBuffer = m_buffers.Get(packet.indirect_buffer.GetIndex(), packet.indirect_buffer.GetGeneration());
                if (indirectBuffer != nullptr) {
                    Diligent::DrawIndexedIndirectAttribs DrawAttrs;
                    DrawAttrs.pAttribsBuffer = indirectBuffer;
                    DrawAttrs.DrawArgsOffset = packet.indirect_offset;
                    DrawAttrs.IndexType      = Diligent::VT_UINT32;
                    DrawAttrs.Flags          = Diligent::DRAW_FLAG_VERIFY_ALL;
                    m_pImmediateContext->DrawIndexedIndirect(DrawAttrs);
                } else {
                    Diligent::DrawIndexedAttribs DrawAttrs;
                    DrawAttrs.NumIndices = packet.index_count;
                    DrawAttrs.IndexType  = Diligent::VT_UINT32;
                    DrawAttrs.Flags      = Diligent::DRAW_FLAG_VERIFY_ALL;
                    m_pImmediateContext->DrawIndexed(DrawAttrs);
                }
            }
        }
    }

} // namespace engine::renderer

