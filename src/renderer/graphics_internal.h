#pragma once
#include <engine/renderer/core.h>
#include <engine/renderer/mesh.h>
#include <engine/renderer/backend.h>
#include <engine/CookedAsset.h>
#include "Common/interface/RefCntAutoPtr.hpp"

#ifdef __cplusplus
extern "C" {
#endif

// ── Diligent globals ──────────────────────────────────────────────────────────
extern Diligent::RefCntAutoPtr<Diligent::IRenderDevice>  g_pDevice;
extern Diligent::RefCntAutoPtr<Diligent::IDeviceContext> g_pContext;
extern Diligent::RefCntAutoPtr<Diligent::ISwapChain>     g_pSwapChain;

// ── Global megabuffers ────────────────────────────────────────────────────────
static const uint32_t GLOBAL_VB_SIZE = 32u * 1024u * 1024u; // 32 MB
static const uint32_t GLOBAL_IB_SIZE = 16u * 1024u * 1024u; // 16 MB

extern Diligent::RefCntAutoPtr<Diligent::IBuffer> g_GlobalVertexBuffer;
extern Diligent::RefCntAutoPtr<Diligent::IBuffer> g_GlobalIndexBuffer;

extern uint32_t g_VertexOffset; 
extern uint32_t g_IndexOffset;  

// ── LOD group storage ─────────────────────────────────────────────────────────
static const uint32_t MAX_LOD_GROUPS = 256u;

typedef struct LODGroupGPU {
    struct MeshLODGPU {
        uint32_t indexCount;
        uint32_t firstIndex;
        uint32_t baseVertex;
        float    threshold;
    } lods[3]; // 48 bytes total
} LODGroupGPU;

extern ScryLODGroup    g_LODGroups[MAX_LOD_GROUPS];
extern uint32_t        g_LODGroupCount;
extern Diligent::RefCntAutoPtr<Diligent::IBuffer> g_LODGroupBuffer;

// ── OS file mapping ───────────────────────────────────────────────────────────
typedef struct MappedFile {
    void*  data;
    size_t size;
#ifdef _WIN32
    void* hFile; 
    void* hMap;
#else
    int fd;
#endif
} MappedFile;

bool MapFileReadOnly(const char* path, MappedFile* out);
void UnmapFile(MappedFile* mf);

bool InitResources(void);
void ShutdownResources(void);

#ifdef __cplusplus
}
#endif
