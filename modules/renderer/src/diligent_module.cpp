#include "renderer/diligent_module.hpp"
#include "Imgui/interface/ImGuiImplDiligent.hpp"
#include "diligent_allocator.hpp"
#include <GLFW/glfw3.h>
#include <OS/IWindow.hpp>
#include <OS/glfw/glfw_window.hpp>
#include <cstddef>
#include <debug/assert.h>
#include <debug/logger.hpp>
#include <ecs/components.hpp>
#include <engine.hpp>
#include <graphics/standard_shaders.hpp>
#include <math/interpolation.hpp>

extern "C" {
extern void* vkGetPhysicalDeviceFeatures2;
extern void* vkGetPhysicalDeviceFeatures2KHR;
}

#include "Graphics/GraphicsEngine/interface/Shader.h"
#include "Graphics/GraphicsEngineVulkan/interface/EngineFactoryVk.h"

namespace engine::renderer {

bool DiligentModule::Initialize(engine::Engine& engine) {
    ENGINE_ASSERT(m_engine == nullptr, "DiligentModule::Initialize called more than once");
    ENGINE_ASSERT(engine.GetWindowManager().GetMainWindow() != nullptr,
                  "DiligentModule requires a main window before initialization");

    m_engine = &engine;
    ENGINE_LOG_INFO("DiligentModule: initializing Vulkan renderer");

    auto* custom_allocator = new EngineDiligentAllocator();
    m_allocatorInstance = custom_allocator;

    auto* mainWindow = engine.GetWindowManager().GetMainWindow();
    engine::NativeHandles handles = mainWindow->GetNativeHandles();
    auto* glfwWindow = static_cast<GlfwWindow*>(mainWindow)->GetRawWindow();

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

    EngineCI.DynamicHeapSize = 2 * 1024 * 1024; // 2MB total dynamic heap
    EngineCI.DynamicHeapPageSize = 256 * 1024;  // 256KB page sizes

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

    // Initialize ImGui wrapper
    Diligent::ImGuiDiligentCreateInfo ImGuiCI;
    ImGuiCI.pDevice = m_pDevice;
    ImGuiCI.BackBufferFmt = m_pSwapChain->GetDesc().ColorBufferFormat;
    ImGuiCI.DepthBufferFmt = m_pSwapChain->GetDesc().DepthBufferFormat;

    auto* pImGui = new Diligent::ImGuiImplDiligent(ImGuiCI);
    pImGui->CreateDeviceObjects();
    m_pImGui = pImGui;
    m_imguiContext = ImGui::GetCurrentContext();

    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ImGui_ImplGlfw_InitForOther(glfwWindow, true);

    // Configure Global Bindless Setup
    Diligent::PipelineResourceDesc Resources[] = {
        {Diligent::SHADER_TYPE_VERTEX | Diligent::SHADER_TYPE_PIXEL, "PushConstants", 1,
         Diligent::SHADER_RESOURCE_TYPE_CONSTANT_BUFFER, Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {Diligent::SHADER_TYPE_VERTEX | Diligent::SHADER_TYPE_PIXEL, "CameraConstants", 1,
         Diligent::SHADER_RESOURCE_TYPE_CONSTANT_BUFFER, Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {Diligent::SHADER_TYPE_PIXEL, "g_Textures", 1024, Diligent::SHADER_RESOURCE_TYPE_TEXTURE_SRV,
         Diligent::SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {Diligent::SHADER_TYPE_PIXEL, "g_Textures_sampler", 1, Diligent::SHADER_RESOURCE_TYPE_SAMPLER,
         Diligent::SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}};

    Diligent::PipelineResourceSignatureDesc PRSDesc;
    PRSDesc.Name = "Global Bindless Signature";
    PRSDesc.Resources = Resources;
    PRSDesc.NumResources = 4;
    PRSDesc.BindingIndex = 0;

    m_pDevice->CreatePipelineResourceSignature(PRSDesc, &m_globalSignature);
    ENGINE_ASSERT(m_globalSignature != nullptr, "Failed to create Global Bindless Signature");

    m_globalSignature->CreateShaderResourceBinding(&m_globalSRB, true);
    ENGINE_ASSERT(m_globalSRB != nullptr, "Failed to create Global SRB");

    Diligent::BufferDesc CBDesc;
    CBDesc.Name = "PushConstants CB";
    CBDesc.Size = sizeof(PushConstants);
    CBDesc.Usage = Diligent::USAGE_DYNAMIC;
    CBDesc.BindFlags = Diligent::BIND_UNIFORM_BUFFER;
    CBDesc.CPUAccessFlags = Diligent::CPU_ACCESS_WRITE;
    m_pDevice->CreateBuffer(CBDesc, nullptr, &m_pushConstants);
    ENGINE_ASSERT(m_pushConstants != nullptr, "Failed to create PushConstants buffer");

    m_globalSRB->GetVariableByName(Diligent::SHADER_TYPE_VERTEX, "PushConstants")->Set(m_pushConstants);

    Diligent::BufferDesc CamDesc;
    CamDesc.Name = "CameraConstants CB";
    CamDesc.Size = sizeof(engine::math::Matrix4);
    CamDesc.Usage = Diligent::USAGE_DYNAMIC;
    CamDesc.BindFlags = Diligent::BIND_UNIFORM_BUFFER;
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
    DefaultDesc.Name = "Bindless Default Texture";
    DefaultDesc.Type = Diligent::RESOURCE_DIM_TEX_2D;
    DefaultDesc.Width = 1;
    DefaultDesc.Height = 1;
    DefaultDesc.Format = Diligent::TEX_FORMAT_RGBA8_UNORM;
    DefaultDesc.BindFlags = Diligent::BIND_SHADER_RESOURCE;

    engine::u32 DefaultPixel = 0xFFFFFFFF; // White pixel
    Diligent::TextureSubResData Mips[1] = {{&DefaultPixel, 4}};
    Diligent::TextureData InitData(Mips, 1);
    m_pDevice->CreateTexture(DefaultDesc, &InitData, &m_defaultTexture);
    ENGINE_ASSERT(m_defaultTexture != nullptr, "Failed to create Default Texture");

    auto* pDefaultSRV = m_defaultTexture->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
    Diligent::IDeviceObject* pDefaultObject = pDefaultSRV;
    auto* pTexVar = m_globalSRB->GetVariableByName(Diligent::SHADER_TYPE_PIXEL, "g_Textures");
    for (engine::u32 i = 0; i < 1024; ++i) {
        pTexVar->SetArray(&pDefaultObject, i, 1);
    }

    // Create Default Instance Buffer to satisfy Vulkan/Diligent binding when instancing is not used
    Diligent::BufferDesc InstDesc;
    InstDesc.Name = "Default Instance Buffer";
    InstDesc.Size = 256;
    InstDesc.Usage = Diligent::USAGE_DEFAULT;
    InstDesc.BindFlags = Diligent::BIND_VERTEX_BUFFER;
    m_pDevice->CreateBuffer(InstDesc, nullptr, &m_defaultInstanceBuffer);
    ENGINE_ASSERT(m_defaultInstanceBuffer != nullptr, "Failed to create Default Instance Buffer");

    // Initialize Default Pipeline PSO
    engine::graphics::PipelineDesc pipeDesc;
    pipeDesc.vs_source = engine::graphics::shaders::UberPBR_VS;
    pipeDesc.ps_source = engine::graphics::shaders::UberPBR_PS;
    pipeDesc.topology = engine::graphics::PrimitiveTopology::TriangleList;
    m_defaultPipeline = engine.GetIGraphics().CreatePipeline(pipeDesc);

    // Initialize Line Pipeline PSO
    engine::graphics::PipelineDesc linePipeDesc;
    linePipeDesc.vs_source = engine::graphics::shaders::UberPBR_VS;
    linePipeDesc.ps_source = engine::graphics::shaders::UberPBR_PS;
    linePipeDesc.topology = engine::graphics::PrimitiveTopology::LineList;
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

    const int* p_write = dag.p_write_state;

    tf::Task init_task = dag.taskflow
                             .emplace([this, p_write]() {
                                 int write_state = *p_write;
                                 m_renderArenas[write_state].Clear();
                                 m_renderQueues[write_state].Initialize(m_renderArenas[write_state], 10000);
                             })
                             .name("DiligentInit");

    tf::Task extract_task =
        dag.taskflow
            .emplace([this, p_write]() {
                int write_state = *p_write;
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

                    if (registry.HasComponent<engine::ecs::RenderComponent>(curr) &&
                        registry.HasComponent<engine::ecs::TransformComponent>(curr)) {
                        const auto& trans = registry.GetComponent<engine::ecs::TransformComponent>(curr);
                        const auto& render = registry.GetComponent<engine::ecs::RenderComponent>(curr);

                        engine::graphics::RenderPacket packet;
                        packet.transform = trans.matrix;
                        packet.pipeline = (render.topology == engine::graphics::PrimitiveTopology::LineList)
                                              ? m_linePipeline
                                              : m_defaultPipeline;

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
                                    vertexDesc.size =
                                        static_cast<uint32_t>(mesh_res->vertices.size() * sizeof(engine::io::Vertex));
                                    vertexDesc.bind = engine::graphics::BufferBind::Vertex;
                                    vertexDesc.usage = engine::graphics::BufferUsage::Static;
                                    vertexBuffer =
                                        m_engine->GetIGraphics().CreateBuffer(vertexDesc, mesh_res->vertices.data());

                                    engine::graphics::BufferDesc indexDesc;
                                    indexDesc.size = static_cast<uint32_t>(mesh_res->indices.size() * sizeof(uint32_t));
                                    indexDesc.bind = engine::graphics::BufferBind::Index;
                                    indexDesc.usage = engine::graphics::BufferUsage::Static;
                                    indexBuffer =
                                        m_engine->GetIGraphics().CreateBuffer(indexDesc, mesh_res->indices.data());

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
                                    texDesc.format = (tex_res->channels == 8) ? engine::graphics::Format::RGBA16_FLOAT
                                                                              : engine::graphics::Format::RGBA8_UNORM;
                                    texDesc.is_render_target = false;
                                    texDesc.is_depth_stencil = false;
                                    textureHandle =
                                        m_engine->GetIGraphics().CreateTexture(texDesc, tex_res->data.get());

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
            })
            .name("DiligentExtract");

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

    if (m_pImGui != nullptr) {
        DestroyImGui();
    }

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

// RenderThreadLoop and DispatchPackets implementations are in diligent_thread.cpp
} // namespace engine::renderer
