#include <engine/graphics.hpp>
#include <engine/CookedAsset.h>
#include <engine/renderer.hpp>

#include <bgfx/bgfx.h>
#include <bgfx/platform.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__APPLE__)
#define GLFW_EXPOSE_NATIVE_COCOA
#else
#define GLFW_EXPOSE_NATIVE_X11
#endif
#include <GLFW/glfw3native.h>

#include <libassert/assert.hpp>
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

namespace Engine {
namespace Graphics {

struct MeshBuffers {
    bgfx::VertexBufferHandle vbh = BGFX_INVALID_HANDLE;
    bgfx::IndexBufferHandle  ibh = BGFX_INVALID_HANDLE;
    uint32_t                 index_count = 0;
    bool                     in_use = false;
};

static MeshBuffers g_meshes[MAX_MESHES];
static uint32_t    g_mesh_count = 0;
static bgfx::VertexLayout g_vertex_layout;

// ── OS Mapping Helpers ────────────────────────────────────────────────────────

struct MappedFile {
    void*  data = nullptr;
    size_t size = 0;
#ifdef _WIN32
    HANDLE hFile = INVALID_HANDLE_VALUE;
    HANDLE hMap = NULL;
#else
    int fd = -1;
#endif
};

static bool MapFileReadOnly(const char* path, MappedFile& out) {
    DEBUG_ASSERT(path != nullptr);
    DEBUG_ASSERT(std::strlen(path) > 0);

#ifdef _WIN32
    out.hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (out.hFile == INVALID_HANDLE_VALUE) return false;

    LARGE_INTEGER li;
    if (!GetFileSizeEx(out.hFile, &li)) { CloseHandle(out.hFile); return false; }
    out.size = static_cast<size_t>(li.QuadPart);

    out.hMap = CreateFileMappingA(out.hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    if (out.hMap == NULL) { CloseHandle(out.hFile); return false; }

    out.data = MapViewOfFile(out.hMap, FILE_MAP_READ, 0, 0, 0);
    if (out.data == NULL) { CloseHandle(out.hMap); CloseHandle(out.hFile); return false; }
#else
    out.fd = open(path, O_RDONLY);
    if (out.fd < 0) return false;

    struct stat st;
    fstat(out.fd, &st);
    out.size = static_cast<size_t>(st.st_size);

    out.data = mmap(NULL, out.size, PROT_READ, MAP_PRIVATE, out.fd, 0);
    if (out.data == MAP_FAILED) { close(out.fd); return false; }
#endif

    DEBUG_ASSERT(out.data != nullptr);
    DEBUG_ASSERT(out.size > 0);
    return true;
}

static void UnmapFile(MappedFile& mf) {
#ifdef _WIN32
    if (mf.data) UnmapViewOfFile(mf.data);
    if (mf.hMap) CloseHandle(mf.hMap);
    if (mf.hFile != INVALID_HANDLE_VALUE) CloseHandle(mf.hFile);
#else
    if (mf.data && mf.data != MAP_FAILED) munmap(mf.data, mf.size);
    if (mf.fd >= 0) close(mf.fd);
#endif
}

// ── Init ──────────────────────────────────────────────────────────────────────

bool Init(void* glfw_window_handle) {
    EngineLog("[Graphics] Initializing BGFX...");
    DEBUG_ASSERT(glfw_window_handle != nullptr);
    
    GLFWwindow* window = static_cast<GLFWwindow*>(glfw_window_handle);
    DEBUG_ASSERT(window != nullptr);

    bgfx::Init init;
    
#if defined(_WIN32) && !defined(_MSC_VER)
    init.type = bgfx::RendererType::Vulkan;
    EngineLog("[Graphics] Forcing Vulkan renderer backend.");
#else
    init.type = bgfx::RendererType::Count; // Auto
#endif
    
    bgfx::PlatformData pd = {};
#if defined(_WIN32)
    pd.nwh = glfwGetWin32Window(window);
#elif defined(__APPLE__)
    pd.nwh = glfwGetCocoaWindow(window);
#else
    pd.nwh = (void*)(uintptr_t)glfwGetX11Window(window);
    pd.ndt = glfwGetX11Display();
#endif
    DEBUG_ASSERT(pd.nwh != nullptr);
    init.platformData = pd;

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    DEBUG_ASSERT(width > 0);
    DEBUG_ASSERT(height > 0);

    init.resolution.width  = (uint32_t)width;
    init.resolution.height = (uint32_t)height;
    init.resolution.reset  = BGFX_RESET_VSYNC;

    if (!bgfx::init(init)) {
        EngineLog("[Graphics] FATAL: bgfx::init failed");
        return false;
    }

    bgfx::setViewRect(0, 0, 0, (uint16_t)width, (uint16_t)height);
    bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x111111FF, 1.0f, 0);

    g_vertex_layout.begin()
        .add(bgfx::Attrib::Position,  3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Normal,    3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();

    DEBUG_ASSERT(g_vertex_layout.getStride() == sizeof(ScryVertex));

    {
        char log_buf[128];
        std::snprintf(log_buf, sizeof(log_buf), "[Graphics] BGFX initialized. Selected Renderer: %s (%dx%d)", 
            bgfx::getRendererName(bgfx::getRendererType()), width, height);
        EngineLog(log_buf);
    }
    return true;
}

void Shutdown() {
    EngineLog("[Graphics] Shutting down BGFX...");
    for (uint32_t i = 0; i < MAX_MESHES; ++i) {
        if (g_meshes[i].in_use) FreeMesh(i);
    }
    bgfx::shutdown();
    EngineLog("[Graphics] BGFX shutdown complete");
}

void BeginFrame() {
    bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x111111FF, 1.0f, 0);
    bgfx::touch(0);
}

void Present() {
    bgfx::frame();
}

uint32_t LoadMesh(const char* filepath) {
    DEBUG_ASSERT(filepath != nullptr);
    DEBUG_ASSERT(g_mesh_count < MAX_MESHES);

    if (!filepath) return INVALID_MESH;
    if (g_mesh_count >= MAX_MESHES) {
        EngineLog("[Graphics] LoadMesh: mesh table full");
        return INVALID_MESH;
    }

    MappedFile mf = {};
    if (!MapFileReadOnly(filepath, mf)) {
        char err_buf[256];
        std::snprintf(err_buf, sizeof(err_buf), "[Graphics] LoadMesh: file not found: %s", filepath);
        EngineLog(err_buf);
        return INVALID_MESH;
    }

    const auto* hdr = static_cast<const ScryMeshHeader*>(mf.data);
    DEBUG_ASSERT(hdr != nullptr);
    DEBUG_ASSERT(hdr->magic == SCRY_MESH_MAGIC);

    if (mf.size < sizeof(ScryMeshHeader) || hdr->magic != SCRY_MESH_MAGIC || hdr->version != SCRY_MESH_VERSION) {
        EngineLog("[Graphics] LoadMesh: invalid .scrymesh header");
        UnmapFile(mf);
        return INVALID_MESH;
    }

    const auto* vertices = reinterpret_cast<const ScryVertex*>(hdr + 1);
    const auto* indices  = reinterpret_cast<const uint32_t*>(vertices + hdr->vertex_count);

    const uint32_t slot = g_mesh_count++;

    // Zero-copy ref to memory, but we copy into BGFX to allow unmapping
    const bgfx::Memory* vmem = bgfx::copy(vertices, hdr->vertex_count * sizeof(ScryVertex));
    const bgfx::Memory* imem = bgfx::copy(indices, hdr->index_count * sizeof(uint32_t));
    DEBUG_ASSERT(vmem != nullptr);
    DEBUG_ASSERT(imem != nullptr);

    g_meshes[slot].vbh = bgfx::createVertexBuffer(vmem, g_vertex_layout);
    // BGFX_BUFFER_INDEX32 is required — cooker writes uint32_t indices
    g_meshes[slot].ibh = bgfx::createIndexBuffer(imem, BGFX_BUFFER_INDEX32);
    DEBUG_ASSERT(bgfx::isValid(g_meshes[slot].vbh));
    DEBUG_ASSERT(bgfx::isValid(g_meshes[slot].ibh));

    g_meshes[slot].index_count = hdr->index_count;
    g_meshes[slot].in_use      = true;

    {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "[Graphics] Mesh loaded: %s (v=%u i=%u slot=%u)",
            filepath, hdr->vertex_count, hdr->index_count, slot);
        EngineLog(buf);
    }

    UnmapFile(mf);
    return slot;
}

void FreeMesh(uint32_t handle) {
    DEBUG_ASSERT(handle < MAX_MESHES);
    if (handle >= MAX_MESHES || !g_meshes[handle].in_use) return;

    if (bgfx::isValid(g_meshes[handle].vbh)) {
        bgfx::destroy(g_meshes[handle].vbh);
        g_meshes[handle].vbh = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(g_meshes[handle].ibh)) {
        bgfx::destroy(g_meshes[handle].ibh);
        g_meshes[handle].ibh = BGFX_INVALID_HANDLE;
    }
    g_meshes[handle].index_count = 0;
    g_meshes[handle].in_use      = false;
}

bgfx::VertexBufferHandle GetVertexBuffer(uint32_t handle) {
    if (handle < MAX_MESHES && g_meshes[handle].in_use) return g_meshes[handle].vbh;
    return BGFX_INVALID_HANDLE;
}

bgfx::IndexBufferHandle GetIndexBuffer(uint32_t handle) {
    if (handle < MAX_MESHES && g_meshes[handle].in_use) return g_meshes[handle].ibh;
    return BGFX_INVALID_HANDLE;
}

} // namespace Graphics
} // namespace Engine
