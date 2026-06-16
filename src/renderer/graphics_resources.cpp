#include "graphics_internal.h"
#include <cassert>
#include <cstdio>

using namespace Diligent;

namespace Engine {
namespace Graphics {

// ── Global megabuffers ────────────────────────────────────────────────────────
RefCntAutoPtr<IBuffer> g_GlobalVertexBuffer;
RefCntAutoPtr<IBuffer> g_GlobalIndexBuffer;

uint32_t g_VertexOffset = 0;
uint32_t g_IndexOffset  = 0;

IBuffer* GetGlobalVertexBuffer() {
    assert(g_GlobalVertexBuffer != nullptr);
    return g_GlobalVertexBuffer;
}

IBuffer* GetGlobalIndexBuffer() {
    assert(g_GlobalIndexBuffer != nullptr);
    return g_GlobalIndexBuffer;
}

// ── LOD group storage ─────────────────────────────────────────────────────────
LODGroup        g_LODGroups[MAX_LOD_GROUPS];
uint32_t        g_LODGroupCount = 0;
RefCntAutoPtr<IBuffer> g_LODGroupBuffer;

IBuffer* GetLODGroupBuffer() {
    assert(g_LODGroupBuffer != nullptr);
    return g_LODGroupBuffer;
}

const LODGroup* GetLODGroup(uint32_t id) {
    assert(id < MAX_LOD_GROUPS);
    if (id >= g_LODGroupCount) return nullptr;
    return &g_LODGroups[id];
}

uint32_t GetLODGroupCount() {
    return g_LODGroupCount;
}

bool InitResources() {
    // Global vertex megabuffer
    {
        BufferDesc bd;
        bd.Name              = "GlobalVB";
        bd.Usage             = USAGE_DEFAULT;
        bd.BindFlags         = BIND_SHADER_RESOURCE | BIND_UNORDERED_ACCESS;
        bd.Mode              = BUFFER_MODE_STRUCTURED;
        bd.ElementByteStride = 32u; // sizeof(ScryVertex)
        bd.Size              = GLOBAL_VB_SIZE;
        g_pDevice->CreateBuffer(bd, nullptr, &g_GlobalVertexBuffer);
        if (!g_GlobalVertexBuffer) return false;
    }

    // Global index buffer
    {
        BufferDesc bd;
        bd.Name      = "GlobalIB";
        bd.Usage     = USAGE_DEFAULT;
        bd.BindFlags = BIND_INDEX_BUFFER;
        bd.Size      = GLOBAL_IB_SIZE;
        g_pDevice->CreateBuffer(bd, nullptr, &g_GlobalIndexBuffer);
        if (!g_GlobalIndexBuffer) return false;
    }

    // LOD group SSBO
    {
        BufferDesc bd;
        bd.Name              = "LODGroupBuffer";
        bd.Usage             = USAGE_DEFAULT;
        bd.BindFlags         = BIND_SHADER_RESOURCE;
        bd.Mode              = BUFFER_MODE_STRUCTURED;
        bd.ElementByteStride = 48u; // sizeof(LODGroupGPU)
        bd.Size              = MAX_LOD_GROUPS * 48u;
        g_pDevice->CreateBuffer(bd, nullptr, &g_LODGroupBuffer);
        if (!g_LODGroupBuffer) return false;
    }

    return true;
}

void ShutdownResources() {
    g_LODGroupBuffer.Release();
    g_GlobalVertexBuffer.Release();
    g_GlobalIndexBuffer.Release();
}

} // namespace Graphics
} // namespace Engine
