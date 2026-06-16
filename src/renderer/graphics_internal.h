#pragma once
#include <engine/renderer/core.h>
#include <engine/renderer/mesh.h>
#include <engine/renderer/backend.h>
#include <engine/CookedAsset.h>
#include "Common/interface/RefCntAutoPtr.hpp"

namespace Engine {
namespace Graphics {

// ── Diligent globals ──────────────────────────────────────────────────────────
extern Diligent::RefCntAutoPtr<Diligent::IRenderDevice>  g_pDevice;
extern Diligent::RefCntAutoPtr<Diligent::IDeviceContext> g_pContext;
extern Diligent::RefCntAutoPtr<Diligent::ISwapChain>     g_pSwapChain;

// ── Global megabuffers ────────────────────────────────────────────────────────
static constexpr uint32_t GLOBAL_VB_SIZE = 32u * 1024u * 1024u; // 32 MB
static constexpr uint32_t GLOBAL_IB_SIZE = 16u * 1024u * 1024u; // 16 MB

extern Diligent::RefCntAutoPtr<Diligent::IBuffer> g_GlobalVertexBuffer;
extern Diligent::RefCntAutoPtr<Diligent::IBuffer> g_GlobalIndexBuffer;

extern uint32_t g_VertexOffset; 
extern uint32_t g_IndexOffset;  

// ── LOD group storage ─────────────────────────────────────────────────────────
static constexpr uint32_t MAX_LOD_GROUPS = 256u;

struct LODGroupGPU {
    struct MeshLODGPU {
        uint32_t indexCount;
        uint32_t firstIndex;
        uint32_t baseVertex;
        float    threshold;
    } lods[3]; // 48 bytes total
};

extern LODGroup        g_LODGroups[MAX_LOD_GROUPS];
extern uint32_t        g_LODGroupCount;
extern Diligent::RefCntAutoPtr<Diligent::IBuffer> g_LODGroupBuffer;

// ── OS file mapping ───────────────────────────────────────────────────────────
struct MappedFile {
    void*  data = nullptr;
    size_t size = 0;
#ifdef _WIN32
    void* hFile; 
    void* hMap;
#else
    int fd;
#endif
};

bool MapFileReadOnly(const char* path, MappedFile& out);
void UnmapFile(MappedFile& mf);

bool InitResources();
void ShutdownResources();

} // namespace Graphics
} // namespace Engine
