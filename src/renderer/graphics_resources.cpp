#include "graphics_internal.h"
#include <cassert>
#include <cstdio>

using namespace Diligent;

// ── Global megabuffers ────────────────────────────────────────────────────────
RefCntAutoPtr<IBuffer> g_GlobalVertexBuffer;
RefCntAutoPtr<IBuffer> g_GlobalIndexBuffer;

uint32_t g_VertexOffset = 0;
uint32_t g_IndexOffset  = 0;

// ── LOD group storage ─────────────────────────────────────────────────────────
ScryLODGroup    g_LODGroups[MAX_LOD_GROUPS];
uint32_t        g_LODGroupCount = 0;
RefCntAutoPtr<IBuffer> g_LODGroupBuffer;

extern "C" {

void* ScryGraphics_GetGlobalVertexBuffer(void) {
    assert(g_GlobalVertexBuffer != nullptr);
    return g_GlobalVertexBuffer;
}

void* ScryGraphics_GetGlobalIndexBuffer(void) {
    assert(g_GlobalIndexBuffer != nullptr);
    return g_GlobalIndexBuffer;
}

void* ScryGraphics_GetLODGroupBuffer(void) {
    assert(g_LODGroupBuffer != nullptr);
    return g_LODGroupBuffer;
}

const ScryLODGroup* ScryGraphics_GetLODGroup(uint32_t id) {
    if (id >= g_LODGroupCount) return NULL;
    return &g_LODGroups[id];
}

uint32_t ScryGraphics_GetLODGroupCount(void) {
    return g_LODGroupCount;
}

bool InitResources(void) {
    // Global vertex megabuffer
    {
        BufferDesc bd;
        bd.Name              = "GlobalVB";
        bd.Usage             = USAGE_DEFAULT;
        bd.BindFlags         = BIND_SHADER_RESOURCE | BIND_UNORDERED_ACCESS;
        bd.Mode              = BUFFER_MODE_STRUCTURED;
        bd.ElementByteStride = 32u; 
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
        bd.ElementByteStride = 80u;
        bd.Size              = MAX_LOD_GROUPS * 80u;
        g_pDevice->CreateBuffer(bd, nullptr, &g_LODGroupBuffer);
        if (!g_LODGroupBuffer) return false;
    }

    return true;
}

void ShutdownResources(void) {
    g_LODGroupBuffer.Release();
    g_GlobalVertexBuffer.Release();
    g_GlobalIndexBuffer.Release();
}

} // extern "C"
