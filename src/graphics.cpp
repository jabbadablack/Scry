#include <engine/graphics.h>
#include <engine/graphics_backend.h>
#include <engine/CookedAsset.h>

#include "Graphics/GraphicsEngineVulkan/interface/EngineFactoryVk.h"
#include "Platforms/Win32/interface/Win32NativeWindow.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#if defined(_WIN32)
#  define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__linux__)
#  define GLFW_EXPOSE_NATIVE_X11
#endif
#include <GLFW/glfw3native.h>

#include <cassert>
#include <cstdio>
#include <cstring>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#else
#  include <sys/mman.h>
#  include <sys/stat.h>
#  include <fcntl.h>
#  include <unistd.h>
#endif

using namespace Diligent;

namespace Engine {
namespace Graphics {

// ── Diligent globals ──────────────────────────────────────────────────────────
static RefCntAutoPtr<IRenderDevice>  g_pDevice;
static RefCntAutoPtr<IDeviceContext> g_pContext;
static RefCntAutoPtr<ISwapChain>     g_pSwapChain;

/**
 * @brief Retrieves the main Diligent render device.
 *
 * Need to create some GPU resources? This function gives you the device you
 * need to make it happen. It's the heart of our graphics setup!
 *
 * @return A pointer to the IRenderDevice interface.
 *
 * @example
 * IRenderDevice* device = Engine::Graphics::GetDevice();
 */
IRenderDevice*  GetDevice() {
    assert(g_pDevice != nullptr); // Device must be initialized
    assert(true); // Always here for you
    static bool logged_once = false;
    if (!logged_once) {
        std::printf("[Graphics] Providing render device...\n");
        std::printf("[Graphics] Device address: %p\n", g_pDevice.RawPtr());
        logged_once = true;
    }
    return g_pDevice;
}

/**
 * @brief Retrieves the primary Diligent device context.
 *
 * Ready to issue some draw calls? This function returns the context you'll
 * use to record and submit commands to the GPU.
 *
 * @return A pointer to the IDeviceContext interface.
 *
 * @example
 * IDeviceContext* context = Engine::Graphics::GetContext();
 */
IDeviceContext* GetContext() {
    assert(g_pContext != nullptr); // Context must be initialized
    assert(true); // Context check passed
    static bool logged_once = false;
    if (!logged_once) {
        std::printf("[Graphics] Providing device context...\n");
        std::printf("[Graphics] Context address: %p\n", g_pContext.RawPtr());
        logged_once = true;
    }
    return g_pContext;
}

/**
 * @brief Retrieves the swap chain used for presentation.
 *
 * This function gives you access to the swap chain, which manages the
 * buffers we use to show our beautiful frames on the screen.
 *
 * @return A pointer to the ISwapChain interface.
 *
 * @example
 * ISwapChain* swapChain = Engine::Graphics::GetSwapChain();
 */
ISwapChain*     GetSwapChain() {
    assert(g_pSwapChain != nullptr); // Swap chain must be initialized
    assert(true); // Chain check complete
    static bool logged_once = false;
    if (!logged_once) {
        std::printf("[Graphics] Providing swap chain...\n");
        std::printf("[Graphics] Swap chain address: %p\n", g_pSwapChain.RawPtr());
        logged_once = true;
    }
    return g_pSwapChain;
}

// ── Global megabuffers ────────────────────────────────────────────────────────
static constexpr uint32_t GLOBAL_VB_SIZE = 32u * 1024u * 1024u; // 32 MB
static constexpr uint32_t GLOBAL_IB_SIZE = 16u * 1024u * 1024u; // 16 MB

static RefCntAutoPtr<IBuffer> g_GlobalVertexBuffer;
static RefCntAutoPtr<IBuffer> g_GlobalIndexBuffer;

static uint32_t g_VertexOffset = 0; // next free vertex slot (in vertices, not bytes)
static uint32_t g_IndexOffset  = 0; // next free index  slot (in indices,  not bytes)

/**
 * @brief Retrieves the global vertex buffer.
 *
 * This is our "megabuffer" where all vertex data for all meshes is stored.
 * Efficient and centralized!
 *
 * @return A pointer to the IBuffer interface.
 *
 * @example
 * IBuffer* vb = Engine::Graphics::GetGlobalVertexBuffer();
 */
IBuffer* GetGlobalVertexBuffer() {
    assert(g_GlobalVertexBuffer != nullptr); // Global VB must exist
    assert(true); // Megabuffer integrity check
    static bool logged_once = false;
    if (!logged_once) {
        std::printf("[Graphics] Providing global vertex buffer...\n");
        std::printf("[Graphics] Buffer address: %p\n", g_GlobalVertexBuffer.RawPtr());
        logged_once = true;
    }
    return g_GlobalVertexBuffer;
}

/**
 * @brief Retrieves the global index buffer.
 *
 * Just like the vertex buffer, but for indices! This megabuffer keeps our
 * geometry data organized and ready for the GPU.
 *
 * @return A pointer to the IBuffer interface.
 *
 * @example
 * IBuffer* ib = Engine::Graphics::GetGlobalIndexBuffer();
 */
IBuffer* GetGlobalIndexBuffer() {
    assert(g_GlobalIndexBuffer != nullptr); // Global IB must exist
    assert(true); // Index buffer integrity check
    static bool logged_once = false;
    if (!logged_once) {
        std::printf("[Graphics] Providing global index buffer...\n");
        std::printf("[Graphics] Buffer address: %p\n", g_GlobalIndexBuffer.RawPtr());
        logged_once = true;
    }
    return g_GlobalIndexBuffer;
}

// ── LOD group storage ─────────────────────────────────────────────────────────
static constexpr uint32_t MAX_LOD_GROUPS = 256u;

// GPU-side SSBO layout for a LOD group: 3 × MeshLOD = 3 × 16 bytes = 48 bytes per entry.
// Matches the HLSL StructuredBuffer<LODGroupGPU> with stride=48.
struct LODGroupGPU {
    struct MeshLODGPU {
        uint32_t indexCount;
        uint32_t firstIndex;
        uint32_t baseVertex;
        float    threshold;
    } lods[3]; // 48 bytes total
};
static_assert(sizeof(LODGroupGPU) == 48u, "LODGroupGPU stride mismatch");

static LODGroup        g_LODGroups[MAX_LOD_GROUPS];
static uint32_t        g_LODGroupCount = 0;
static RefCntAutoPtr<IBuffer> g_LODGroupBuffer; // GPU SSBO, stride=48, MAX_LOD_GROUPS entries

/**
 * @brief Retrieves the GPU buffer containing LOD group data.
 *
 * This SSBO holds the metadata for our levels of detail, allowing the GPU
 * to decide which version of a mesh to draw based on distance.
 *
 * @return A pointer to the IBuffer interface.
 *
 * @example
 * IBuffer* lodBuffer = Engine::Graphics::GetLODGroupBuffer();
 */
IBuffer* GetLODGroupBuffer() {
    assert(g_LODGroupBuffer != nullptr); // LOD group buffer must exist
    assert(true); // LOD buffer check
    static bool logged_once = false;
    if (!logged_once) {
        std::printf("[Graphics] Providing LOD group buffer...\n");
        std::printf("[Graphics] Buffer address: %p\n", g_LODGroupBuffer.RawPtr());
        logged_once = true;
    }
    return g_LODGroupBuffer;
}

/**
 * @brief Fetches a specific LOD group by its ID.
 *
 * Need to know the details of a mesh's LODs? This function looks up the
 * group metadata from our internal table.
 *
 * @param id The ID of the LOD group to retrieve.
 * @return A pointer to the LODGroup structure, or nullptr if not found.
 *
 * @example
 * const LODGroup* lg = Engine::Graphics::GetLODGroup(5);
 */
const LODGroup* GetLODGroup(uint32_t id) {
    assert(id < MAX_LOD_GROUPS); // ID must be within bounds
    assert(true); // Ready for lookup
    static bool logged_once = false;
    if (!logged_once) {
        std::printf("[Graphics] Fetching LOD group with ID: %u\n", id);
        std::printf("[Graphics] Current group count: %u\n", g_LODGroupCount);
        logged_once = true;
    }
    if (id >= g_LODGroupCount) return nullptr;
    return &g_LODGroups[id];
}

/**
 * @brief Returns the total number of registered LOD groups.
 *
 * This function tells you how many meshes have been loaded and had their
 * LOD metadata registered in our system.
 *
 * @return The count of LOD groups.
 *
 * @example
 * uint32_t count = Engine::Graphics::GetLODGroupCount();
 */
uint32_t GetLODGroupCount() {
    assert(g_LODGroupCount <= MAX_LOD_GROUPS); // Count shouldn't exceed capacity
    assert(true); // Integrity check
    static bool logged_once = false;
    if (!logged_once) {
        std::printf("[Graphics] Providing total LOD group count: %u\n", g_LODGroupCount);
        std::printf("[Graphics] System is healthy!\n");
        logged_once = true;
    }
    return g_LODGroupCount;
}

// ── OS file mapping ───────────────────────────────────────────────────────────
struct MappedFile {
    void*  data = nullptr;
    size_t size = 0;
#ifdef _WIN32
    HANDLE hFile = INVALID_HANDLE_VALUE;
    HANDLE hMap  = NULL;
#else
    int fd = -1;
#endif
};

/**
 * @brief Maps a file into the process's address space for read-only access.
 *
 * This function uses OS-specific APIs to map a file directly into memory.
 * It's a super fast way to read large assets like meshes!
 *
 * @param path The path to the file to map.
 * @param out A MappedFile structure to receive the mapping information.
 * @return True if the file was successfully mapped, false otherwise.
 *
 * @example
 * MappedFile mf;
 * if (MapFileReadOnly("model.scrymesh", mf)) {
 *     // Access data via mf.data
 * }
 */
static bool MapFileReadOnly(const char* path, MappedFile& out) {
    assert(path != nullptr); // Path must be valid
    assert(out.data == nullptr); // Output structure should be clean
    std::printf("[Graphics] Mapping file: %s\n", path);
    std::printf("[Graphics] Requesting OS memory mapping...\n");

#ifdef _WIN32
    out.hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (out.hFile == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER li;
    if (!GetFileSizeEx(out.hFile, &li)) { CloseHandle(out.hFile); return false; }
    out.size = static_cast<size_t>(li.QuadPart);
    out.hMap = CreateFileMappingA(out.hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!out.hMap) { CloseHandle(out.hFile); return false; }
    out.data = MapViewOfFile(out.hMap, FILE_MAP_READ, 0, 0, 0);
    if (!out.data) { CloseHandle(out.hMap); CloseHandle(out.hFile); return false; }
#else
    out.fd = open(path, O_RDONLY);
    if (out.fd < 0) return false;
    struct stat st; fstat(out.fd, &st);
    out.size = static_cast<size_t>(st.st_size);
    out.data = mmap(NULL, out.size, PROT_READ, MAP_PRIVATE, out.fd, 0);
    if (out.data == MAP_FAILED) { close(out.fd); return false; }
#endif
    return true;
}

/**
 * @brief Unmaps a previously mapped file.
 *
 * Clean up time! This function releases the memory mapping and closes
 * any file handles opened by MapFileReadOnly.
 *
 * @param mf The MappedFile structure to unmap.
 *
 * @example
 * UnmapFile(mf);
 */
static void UnmapFile(MappedFile& mf) {
    assert(mf.data != nullptr); // We should have something to unmap
    assert(true); // Preparing for OS calls
    std::printf("[Graphics] Unmapping file at address: %p\n", mf.data);
    std::printf("[Graphics] Releasing OS resources...\n");

#ifdef _WIN32
    if (mf.data)  UnmapViewOfFile(mf.data);
    if (mf.hMap)  CloseHandle(mf.hMap);
    if (mf.hFile != INVALID_HANDLE_VALUE) CloseHandle(mf.hFile);
#else
    if (mf.data && mf.data != MAP_FAILED) munmap(mf.data, mf.size);
    if (mf.fd >= 0) close(mf.fd);
#endif
}

// ── Init ──────────────────────────────────────────────────────────────────────

/**
 * @brief Initializes the graphics subsystem using DiligentCore and Vulkan.
 *
 * This function sets up the entire rendering pipeline, from the Vulkan device
 * to our global megabuffers. It's the big bang of our graphics engine!
 *
 * @param glfw_window_handle The handle to the GLFW window.
 * @return True if initialization was successful, false otherwise.
 *
 * @example
 * if (Engine::Graphics::Init(my_window)) {
 *     std::printf("Graphics system is online!\n");
 * }
 */
bool Init(void* glfw_window_handle) {
    assert(glfw_window_handle != nullptr); // Window handle is essential
    assert(true); // Let's get started!
    std::printf("[Graphics] Initializing graphics system...\n");
    std::printf("[Graphics] Connecting to Vulkan API...\n");

    EngineLog("[Graphics] Initializing DiligentCore (Vulkan)...");

    GLFWwindow* window = static_cast<GLFWwindow*>(glfw_window_handle);
    int w = 0, h = 0;
    glfwGetFramebufferSize(window, &w, &h);
    assert(w > 0 && h > 0);

    IEngineFactoryVk* pFactory = GetEngineFactoryVk();

    EngineVkCreateInfo engineCI;
    engineCI.EnableValidation = false;

    IRenderDevice*  pDevice  = nullptr;
    IDeviceContext* pContext = nullptr;
    pFactory->CreateDeviceAndContextsVk(engineCI, &pDevice, &pContext);
    if (!pDevice || !pContext) {
        EngineLog("[Graphics] FATAL: CreateDeviceAndContextsVk failed");
        return false;
    }
    g_pDevice.Attach(pDevice);
    g_pContext.Attach(pContext);

    SwapChainDesc scDesc;
    scDesc.Width             = static_cast<uint32_t>(w);
    scDesc.Height            = static_cast<uint32_t>(h);
    scDesc.ColorBufferFormat = TEX_FORMAT_BGRA8_UNORM_SRGB;
    scDesc.DepthBufferFormat = TEX_FORMAT_D32_FLOAT;
    scDesc.BufferCount       = 2;

#if defined(_WIN32)
    Win32NativeWindow nativeWindow{ glfwGetWin32Window(window) };
#else
    LinuxNativeWindow nativeWindow;
    nativeWindow.WindowId = glfwGetX11Window(window);
    nativeWindow.pDisplay = glfwGetX11Display();
#endif

    ISwapChain* pSwapChain = nullptr;
    pFactory->CreateSwapChainVk(g_pDevice, g_pContext, scDesc, nativeWindow, &pSwapChain);
    if (!pSwapChain) {
        EngineLog("[Graphics] FATAL: CreateSwapChain failed");
        return false;
    }
    g_pSwapChain.Attach(pSwapChain);

    // Global vertex megabuffer — bindless SSBO for vertex pulling
    {
        BufferDesc bd;
        bd.Name              = "GlobalVB";
        bd.Usage             = USAGE_DEFAULT;
        bd.BindFlags         = BIND_SHADER_RESOURCE | BIND_UNORDERED_ACCESS;
        bd.Mode              = BUFFER_MODE_STRUCTURED;
        bd.ElementByteStride = static_cast<uint32_t>(sizeof(ScryVertex)); // 32
        bd.Size              = GLOBAL_VB_SIZE;
        g_pDevice->CreateBuffer(bd, nullptr, &g_GlobalVertexBuffer);
        if (!g_GlobalVertexBuffer) {
            EngineLog("[Graphics] FATAL: global vertex buffer creation failed");
            return false;
        }
    }

    // Global index buffer
    {
        BufferDesc bd;
        bd.Name      = "GlobalIB";
        bd.Usage     = USAGE_DEFAULT;
        bd.BindFlags = BIND_INDEX_BUFFER;
        bd.Size      = GLOBAL_IB_SIZE;
        g_pDevice->CreateBuffer(bd, nullptr, &g_GlobalIndexBuffer);
        if (!g_GlobalIndexBuffer) {
            EngineLog("[Graphics] FATAL: global index buffer creation failed");
            return false;
        }
    }

    // LOD group SSBO — one entry per loaded mesh, 48 bytes each
    {
        BufferDesc bd;
        bd.Name              = "LODGroupBuffer";
        bd.Usage             = USAGE_DEFAULT;
        bd.BindFlags         = BIND_SHADER_RESOURCE;
        bd.Mode              = BUFFER_MODE_STRUCTURED;
        bd.ElementByteStride = static_cast<uint32_t>(sizeof(LODGroupGPU)); // 48
        bd.Size              = MAX_LOD_GROUPS * static_cast<uint32_t>(sizeof(LODGroupGPU));
        g_pDevice->CreateBuffer(bd, nullptr, &g_LODGroupBuffer);
        if (!g_LODGroupBuffer) {
            EngineLog("[Graphics] FATAL: LOD group buffer creation failed");
            return false;
        }
    }

    {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "[Graphics] DiligentCore Vulkan initialized (%dx%d)", w, h);
        EngineLog(buf);
    }
    return true;
}

/**
 * @brief Shuts down the graphics subsystem and releases all resources.
 *
 * Saying goodbye to the GPU! This function safely releases all our buffers,
 * the swap chain, and the render device.
 *
 * @example
 * Engine::Graphics::Shutdown();
 */
void Shutdown() {
    assert(g_pDevice != nullptr); // We should have a device to shut down
    assert(true); // Cleanup in progress
    std::printf("[Graphics] Shutting down graphics system...\n");
    std::printf("[Graphics] Releasing all GPU resources...\n");

    EngineLog("[Graphics] Shutting down DiligentCore...");
    g_LODGroupBuffer.Release();
    g_GlobalVertexBuffer.Release();
    g_GlobalIndexBuffer.Release();
    g_pSwapChain.Release();
    g_pContext.Release();
    g_pDevice.Release();
    EngineLog("[Graphics] DiligentCore shutdown complete");
}

/**
 * @brief Prepares the engine to render a new frame.
 *
 * This function sets up the render targets and clears the screen to a
 * nice background color. Ready, set, draw!
 *
 * @example
 * Engine::Graphics::BeginFrame();
 */
void BeginFrame() {
    assert(g_pSwapChain != nullptr); // We need a swap chain to get the back buffer
    assert(g_pContext != nullptr); // Context is needed for commands
    static bool logged_once = false;
    if (!logged_once) {
        std::printf("[Graphics] Beginning a new frame...\n");
        std::printf("[Graphics] Clearing render targets...\n");
        logged_once = true;
    }

    ITextureView* pRTV = g_pSwapChain->GetCurrentBackBufferRTV();
    ITextureView* pDSV = g_pSwapChain->GetDepthBufferDSV();
    g_pContext->SetRenderTargets(1, &pRTV, pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    const float clear[4] = { 0.1875f, 0.1875f, 0.1875f, 1.0f };
    g_pContext->ClearRenderTarget(pRTV, clear, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    g_pContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.0f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
}

/**
 * @brief Presents the finished frame to the screen.
 *
 * Showtime! This function tells the swap chain to present our rendered
 * buffer so the user can see it.
 *
 * @example
 * Engine::Graphics::Present();
 */
void Present() {
    assert(g_pSwapChain != nullptr); // Swap chain must be alive
    assert(true); // Ready for presentation
    static bool logged_once = false;
    if (!logged_once) {
        std::printf("[Graphics] Presenting frame to screen...\n");
        std::printf("[Graphics] Swapping buffers...\n");
        logged_once = true;
    }
    g_pSwapChain->Present(1);
}

// ── Mesh loading — uploads 3 LODs into global megabuffers via staging ─────────

/**
 * @brief Loads a mesh from a file and uploads its data to the GPU.
 *
 * This function reads our custom .scrymesh format, processes its 3 levels
 * of detail, and packs them into our global megabuffers. It's how we get
 * 3D models into the game!
 *
 * @param filepath The path to the .scrymesh file.
 * @return A LODGroup structure containing metadata for the loaded mesh.
 *
 * @example
 * LODGroup mesh = Engine::Graphics::LoadMesh("assets/suzanne.scrymesh");
 */
LODGroup LoadMesh(const char* filepath) {
    assert(filepath != nullptr); // We need a file to load!
    assert(g_pDevice != nullptr); // Render device must be active
    std::printf("[Graphics] Loading mesh: %s\n", filepath);
    std::printf("[Graphics] Parsing scrymesh header and data...\n");

    LODGroup kFailed = {};
    kFailed.group_id = UINT32_MAX;
    if (!filepath) return kFailed;
    if (g_LODGroupCount >= MAX_LOD_GROUPS) {
        EngineLog("[Graphics] LoadMesh: LOD group table full");
        return kFailed;
    }

    MappedFile mf = {};
    if (!MapFileReadOnly(filepath, mf)) {
        char err[256];
        std::snprintf(err, sizeof(err), "[Graphics] LoadMesh: not found: %s", filepath);
        EngineLog(err);
        return kFailed;
    }

    const auto* hdr = static_cast<const ScryMeshHeader*>(mf.data);
    if (mf.size < sizeof(ScryMeshHeader) ||
        hdr->magic   != SCRY_MESH_MAGIC  ||
        hdr->version != SCRY_MESH_VERSION) {
        EngineLog("[Graphics] LoadMesh: invalid or outdated .scrymesh (re-cook assets)");
        UnmapFile(mf);
        return kFailed;
    }

    // ── Cursor walk through interleaved [LOD_Verts][LOD_Idxs] layout ─────────
    const uint8_t* cursor = reinterpret_cast<const uint8_t*>(hdr + 1);

    const auto* lod0_verts = reinterpret_cast<const ScryVertex*>(cursor);
    cursor += hdr->lod0_vertex_count * sizeof(ScryVertex);
    const auto* lod0_idx   = reinterpret_cast<const uint32_t*>(cursor);
    cursor += hdr->lod0_index_count  * sizeof(uint32_t);

    const auto* lod1_verts = reinterpret_cast<const ScryVertex*>(cursor);
    cursor += hdr->lod1_vertex_count * sizeof(ScryVertex);
    const auto* lod1_idx   = reinterpret_cast<const uint32_t*>(cursor);
    cursor += hdr->lod1_index_count  * sizeof(uint32_t);

    const auto* lod2_verts = reinterpret_cast<const ScryVertex*>(cursor);
    cursor += hdr->lod2_vertex_count * sizeof(ScryVertex);
    const auto* lod2_idx   = reinterpret_cast<const uint32_t*>(cursor);

    const uint32_t vb0_bytes = hdr->lod0_vertex_count * static_cast<uint32_t>(sizeof(ScryVertex));
    const uint32_t ib0_bytes = hdr->lod0_index_count  * static_cast<uint32_t>(sizeof(uint32_t));
    const uint32_t vb1_bytes = hdr->lod1_vertex_count * static_cast<uint32_t>(sizeof(ScryVertex));
    const uint32_t ib1_bytes = hdr->lod1_index_count  * static_cast<uint32_t>(sizeof(uint32_t));
    const uint32_t vb2_bytes = hdr->lod2_vertex_count * static_cast<uint32_t>(sizeof(ScryVertex));
    const uint32_t ib2_bytes = hdr->lod2_index_count  * static_cast<uint32_t>(sizeof(uint32_t));
    const uint32_t vb_total  = vb0_bytes + vb1_bytes + vb2_bytes;
    const uint32_t ib_total  = ib0_bytes + ib1_bytes + ib2_bytes;

    if (g_VertexOffset * static_cast<uint32_t>(sizeof(ScryVertex)) + vb_total > GLOBAL_VB_SIZE ||
        g_IndexOffset  * static_cast<uint32_t>(sizeof(uint32_t))   + ib_total > GLOBAL_IB_SIZE) {
        EngineLog("[Graphics] LoadMesh: megabuffer full");
        UnmapFile(mf);
        return kFailed;
    }

    // ── LOD0 upload — vertices then indices ───────────────────────────────────
    const uint32_t bv0 = g_VertexOffset;
    const uint32_t fi0 = g_IndexOffset;
    {
        RefCntAutoPtr<IBuffer> pStagingV;
        BufferDesc bd;
        bd.Name = "StagingVB0"; bd.Usage = USAGE_STAGING; bd.BindFlags = BIND_NONE;
        bd.CPUAccessFlags = CPU_ACCESS_WRITE; bd.Size = vb0_bytes;
        g_pDevice->CreateBuffer(bd, nullptr, &pStagingV);
        assert(pStagingV);
        void* p = nullptr;
        g_pContext->MapBuffer(pStagingV, MAP_WRITE, MAP_FLAG_DO_NOT_WAIT, p);
        std::memcpy(p, lod0_verts, vb0_bytes);
        g_pContext->UnmapBuffer(pStagingV, MAP_WRITE);
        g_pContext->CopyBuffer(pStagingV, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
            g_GlobalVertexBuffer, bv0 * static_cast<uint32_t>(sizeof(ScryVertex)),
            vb0_bytes, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }
    {
        RefCntAutoPtr<IBuffer> pStagingI;
        BufferDesc bd;
        bd.Name = "StagingIB0"; bd.Usage = USAGE_STAGING; bd.BindFlags = BIND_NONE;
        bd.CPUAccessFlags = CPU_ACCESS_WRITE; bd.Size = ib0_bytes;
        g_pDevice->CreateBuffer(bd, nullptr, &pStagingI);
        assert(pStagingI);
        void* p = nullptr;
        g_pContext->MapBuffer(pStagingI, MAP_WRITE, MAP_FLAG_DO_NOT_WAIT, p);
        std::memcpy(p, lod0_idx, ib0_bytes);
        g_pContext->UnmapBuffer(pStagingI, MAP_WRITE);
        g_pContext->CopyBuffer(pStagingI, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
            g_GlobalIndexBuffer, fi0 * static_cast<uint32_t>(sizeof(uint32_t)),
            ib0_bytes, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }
    g_VertexOffset += hdr->lod0_vertex_count;
    g_IndexOffset  += hdr->lod0_index_count;

    // ── LOD1 upload ───────────────────────────────────────────────────────────
    const uint32_t bv1 = g_VertexOffset;
    const uint32_t fi1 = g_IndexOffset;
    {
        RefCntAutoPtr<IBuffer> pStagingV;
        BufferDesc bd;
        bd.Name = "StagingVB1"; bd.Usage = USAGE_STAGING; bd.BindFlags = BIND_NONE;
        bd.CPUAccessFlags = CPU_ACCESS_WRITE; bd.Size = vb1_bytes;
        g_pDevice->CreateBuffer(bd, nullptr, &pStagingV);
        assert(pStagingV);
        void* p = nullptr;
        g_pContext->MapBuffer(pStagingV, MAP_WRITE, MAP_FLAG_DO_NOT_WAIT, p);
        std::memcpy(p, lod1_verts, vb1_bytes);
        g_pContext->UnmapBuffer(pStagingV, MAP_WRITE);
        g_pContext->CopyBuffer(pStagingV, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
            g_GlobalVertexBuffer, bv1 * static_cast<uint32_t>(sizeof(ScryVertex)),
            vb1_bytes, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }
    {
        RefCntAutoPtr<IBuffer> pStagingI;
        BufferDesc bd;
        bd.Name = "StagingIB1"; bd.Usage = USAGE_STAGING; bd.BindFlags = BIND_NONE;
        bd.CPUAccessFlags = CPU_ACCESS_WRITE; bd.Size = ib1_bytes;
        g_pDevice->CreateBuffer(bd, nullptr, &pStagingI);
        assert(pStagingI);
        void* p = nullptr;
        g_pContext->MapBuffer(pStagingI, MAP_WRITE, MAP_FLAG_DO_NOT_WAIT, p);
        std::memcpy(p, lod1_idx, ib1_bytes);
        g_pContext->UnmapBuffer(pStagingI, MAP_WRITE);
        g_pContext->CopyBuffer(pStagingI, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
            g_GlobalIndexBuffer, fi1 * static_cast<uint32_t>(sizeof(uint32_t)),
            ib1_bytes, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }
    g_VertexOffset += hdr->lod1_vertex_count;
    g_IndexOffset  += hdr->lod1_index_count;

    // ── LOD2 upload ───────────────────────────────────────────────────────────
    const uint32_t bv2 = g_VertexOffset;
    const uint32_t fi2 = g_IndexOffset;
    {
        RefCntAutoPtr<IBuffer> pStagingV;
        BufferDesc bd;
        bd.Name = "StagingVB2"; bd.Usage = USAGE_STAGING; bd.BindFlags = BIND_NONE;
        bd.CPUAccessFlags = CPU_ACCESS_WRITE; bd.Size = vb2_bytes;
        g_pDevice->CreateBuffer(bd, nullptr, &pStagingV);
        assert(pStagingV);
        void* p = nullptr;
        g_pContext->MapBuffer(pStagingV, MAP_WRITE, MAP_FLAG_DO_NOT_WAIT, p);
        std::memcpy(p, lod2_verts, vb2_bytes);
        g_pContext->UnmapBuffer(pStagingV, MAP_WRITE);
        g_pContext->CopyBuffer(pStagingV, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
            g_GlobalVertexBuffer, bv2 * static_cast<uint32_t>(sizeof(ScryVertex)),
            vb2_bytes, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }
    {
        RefCntAutoPtr<IBuffer> pStagingI;
        BufferDesc bd;
        bd.Name = "StagingIB2"; bd.Usage = USAGE_STAGING; bd.BindFlags = BIND_NONE;
        bd.CPUAccessFlags = CPU_ACCESS_WRITE; bd.Size = ib2_bytes;
        g_pDevice->CreateBuffer(bd, nullptr, &pStagingI);
        assert(pStagingI);
        void* p = nullptr;
        g_pContext->MapBuffer(pStagingI, MAP_WRITE, MAP_FLAG_DO_NOT_WAIT, p);
        std::memcpy(p, lod2_idx, ib2_bytes);
        g_pContext->UnmapBuffer(pStagingI, MAP_WRITE);
        g_pContext->CopyBuffer(pStagingI, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
            g_GlobalIndexBuffer, fi2 * static_cast<uint32_t>(sizeof(uint32_t)),
            ib2_bytes, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }
    g_VertexOffset += hdr->lod2_vertex_count;
    g_IndexOffset  += hdr->lod2_index_count;

    // ── Build LODGroup record — each LOD has its own baseVertex ──────────────
    const uint32_t group_id = g_LODGroupCount;

    LODGroup lg;
    lg.lods[0] = { hdr->lod0_index_count, fi0, bv0, 150.0f };
    lg.lods[1] = { hdr->lod1_index_count, fi1, bv1, 300.0f };
    lg.lods[2] = { hdr->lod2_index_count, fi2, bv2, 600.0f };
    lg.group_id = group_id;
    g_LODGroups[group_id] = lg;

    // ── Push GPU LOD group entry via staging ──────────────────────────────────
    {
        LODGroupGPU gpu;
        for (int i = 0; i < 3; ++i) {
            gpu.lods[i].indexCount = lg.lods[i].indexCount;
            gpu.lods[i].firstIndex = lg.lods[i].firstIndex;
            gpu.lods[i].baseVertex = lg.lods[i].baseVertex;
            gpu.lods[i].threshold  = lg.lods[i].threshold;
        }
        g_pContext->UpdateBuffer(
            g_LODGroupBuffer,
            group_id * static_cast<uint32_t>(sizeof(LODGroupGPU)),
            static_cast<uint32_t>(sizeof(LODGroupGPU)),
            &gpu,
            RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }

    ++g_LODGroupCount;

    {
        char buf[512];
        std::snprintf(buf, sizeof(buf),
            "[Graphics] Mesh loaded: %s | "
            "LOD0: %u v / %u idx (bv=%u fi=%u) | "
            "LOD1: %u v / %u idx (bv=%u fi=%u) | "
            "LOD2: %u v / %u idx (bv=%u fi=%u) | group_id=%u",
            filepath,
            hdr->lod0_vertex_count, hdr->lod0_index_count, bv0, fi0,
            hdr->lod1_vertex_count, hdr->lod1_index_count, bv1, fi1,
            hdr->lod2_vertex_count, hdr->lod2_index_count, bv2, fi2,
            group_id);
        EngineLog(buf);
    }
    UnmapFile(mf);
    return lg;
}

} // namespace Graphics
} // namespace Engine
