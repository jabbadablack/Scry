#include "graphics_internal.h"
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

extern "C" {

bool MapFileReadOnly(const char* path, MappedFile* out) {
    assert(path != nullptr);

#ifdef _WIN32
    HANDLE hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER li;
    if (!GetFileSizeEx(hFile, &li)) { CloseHandle(hFile); return false; }
    out->size = static_cast<size_t>(li.QuadPart);
    HANDLE hMap = CreateFileMappingA(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!hMap) { CloseHandle(hFile); return false; }
    out->data = MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
    out->hFile = (void*)hFile;
    out->hMap = (void*)hMap;
    if (!out->data) { CloseHandle(hMap); CloseHandle(hFile); return false; }
#else
    out->fd = open(path, O_RDONLY);
    if (out->fd < 0) return false;
    struct stat st; fstat(out->fd, &st);
    out->size = static_cast<size_t>(st.st_size);
    out->data = mmap(NULL, out->size, PROT_READ, MAP_PRIVATE, out->fd, 0);
    if (out->data == MAP_FAILED) { close(out->fd); return false; }
#endif
    return true;
}

void UnmapFile(MappedFile* mf) {
    if (!mf->data) return;
#ifdef _WIN32
    UnmapViewOfFile(mf->data);
    CloseHandle((HANDLE)mf->hMap);
    CloseHandle((HANDLE)mf->hFile);
#else
    if (mf->data != MAP_FAILED) munmap(mf->data, mf->size);
    if (mf->fd >= 0) close(mf->fd);
#endif
}

ScryLODGroup ScryGraphics_LoadMesh(const char* filepath) {
    assert(filepath != nullptr);
    assert(g_pDevice != nullptr);

    ScryLODGroup kFailed = { .group_id = UINT32_MAX };
    if (!filepath) return kFailed;
    if (g_LODGroupCount >= MAX_LOD_GROUPS) {
        return kFailed;
    }

    MappedFile mf = {0};
    if (!MapFileReadOnly(filepath, &mf)) {
        return kFailed;
    }

    const ScryMeshHeader* hdr = static_cast<const ScryMeshHeader*>(mf.data);
    if (mf.size < sizeof(ScryMeshHeader) ||
        hdr->magic   != SCRY_MESH_MAGIC  ||
        hdr->version != SCRY_MESH_VERSION) {
        UnmapFile(&mf);
        return kFailed;
    }

    const uint8_t* cursor = reinterpret_cast<const uint8_t*>(hdr + 1);

    const ScryVertex* lod0_verts = reinterpret_cast<const ScryVertex*>(cursor);
    cursor += hdr->lod0_vertex_count * sizeof(ScryVertex);
    const uint32_t* lod0_idx = reinterpret_cast<const uint32_t*>(cursor);
    cursor += hdr->lod0_index_count * sizeof(uint32_t);

    const ScryVertex* lod1_verts = reinterpret_cast<const ScryVertex*>(cursor);
    cursor += hdr->lod1_vertex_count * sizeof(ScryVertex);
    const uint32_t* lod1_idx = reinterpret_cast<const uint32_t*>(cursor);
    cursor += hdr->lod1_index_count * sizeof(uint32_t);

    const ScryVertex* lod2_verts = reinterpret_cast<const ScryVertex*>(cursor);
    cursor += hdr->lod2_vertex_count * sizeof(ScryVertex);
    const uint32_t* lod2_idx = reinterpret_cast<const uint32_t*>(cursor);

    const uint32_t vb0_bytes = hdr->lod0_vertex_count * 8u;
    const uint32_t ib0_bytes = hdr->lod0_index_count * 4u;
    const uint32_t vb1_bytes = hdr->lod1_vertex_count * 8u;
    const uint32_t ib1_bytes = hdr->lod1_index_count * 4u;
    const uint32_t vb2_bytes = hdr->lod2_vertex_count * 8u;
    const uint32_t ib2_bytes = hdr->lod2_index_count * 4u;
    const uint32_t vb_total  = vb0_bytes + vb1_bytes + vb2_bytes;
    const uint32_t ib_total  = ib0_bytes + ib1_bytes + ib2_bytes;

    if (g_VertexOffset * 8u + vb_total > GLOBAL_VB_SIZE ||
        g_IndexOffset * 4u + ib_total > GLOBAL_IB_SIZE) {
        UnmapFile(&mf);
        return kFailed;
    }

    const uint32_t bv0 = g_VertexOffset;
    const uint32_t fi0 = g_IndexOffset;
    {
        RefCntAutoPtr<IBuffer> pStagingV;
        BufferDesc bd; bd.Usage = USAGE_STAGING; bd.CPUAccessFlags = CPU_ACCESS_WRITE; bd.Size = vb0_bytes;
        g_pDevice->CreateBuffer(bd, nullptr, &pStagingV);
        void* p = nullptr; g_pContext->MapBuffer(pStagingV, MAP_WRITE, MAP_FLAG_DO_NOT_WAIT, p);
        std::memcpy(p, lod0_verts, vb0_bytes); g_pContext->UnmapBuffer(pStagingV, MAP_WRITE);
        g_pContext->CopyBuffer(pStagingV, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
            g_GlobalVertexBuffer, bv0 * 8u, vb0_bytes, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }
    {
        RefCntAutoPtr<IBuffer> pStagingI;
        BufferDesc bd; bd.Usage = USAGE_STAGING; bd.CPUAccessFlags = CPU_ACCESS_WRITE; bd.Size = ib0_bytes;
        g_pDevice->CreateBuffer(bd, nullptr, &pStagingI);
        void* p = nullptr; g_pContext->MapBuffer(pStagingI, MAP_WRITE, MAP_FLAG_DO_NOT_WAIT, p);
        std::memcpy(p, lod0_idx, ib0_bytes); g_pContext->UnmapBuffer(pStagingI, MAP_WRITE);
        g_pContext->CopyBuffer(pStagingI, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
            g_GlobalIndexBuffer, fi0 * 4u, ib0_bytes, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }
    g_VertexOffset += hdr->lod0_vertex_count;
    g_IndexOffset  += hdr->lod0_index_count;

    const uint32_t bv1 = g_VertexOffset;
    const uint32_t fi1 = g_IndexOffset;
    {
        RefCntAutoPtr<IBuffer> pStagingV;
        BufferDesc bd; bd.Usage = USAGE_STAGING; bd.CPUAccessFlags = CPU_ACCESS_WRITE; bd.Size = vb1_bytes;
        g_pDevice->CreateBuffer(bd, nullptr, &pStagingV);
        void* p = nullptr; g_pContext->MapBuffer(pStagingV, MAP_WRITE, MAP_FLAG_DO_NOT_WAIT, p);
        std::memcpy(p, lod1_verts, vb1_bytes); g_pContext->UnmapBuffer(pStagingV, MAP_WRITE);
        g_pContext->CopyBuffer(pStagingV, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
            g_GlobalVertexBuffer, bv1 * 8u, vb1_bytes, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }
    {
        RefCntAutoPtr<IBuffer> pStagingI;
        BufferDesc bd; bd.Usage = USAGE_STAGING; bd.CPUAccessFlags = CPU_ACCESS_WRITE; bd.Size = ib1_bytes;
        g_pDevice->CreateBuffer(bd, nullptr, &pStagingI);
        void* p = nullptr; g_pContext->MapBuffer(pStagingI, MAP_WRITE, MAP_FLAG_DO_NOT_WAIT, p);
        std::memcpy(p, lod1_idx, ib1_bytes); g_pContext->UnmapBuffer(pStagingI, MAP_WRITE);
        g_pContext->CopyBuffer(pStagingI, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
            g_GlobalIndexBuffer, fi1 * 4u, ib1_bytes, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }
    g_VertexOffset += hdr->lod1_vertex_count;
    g_IndexOffset  += hdr->lod1_index_count;

    const uint32_t bv2 = g_VertexOffset;
    const uint32_t fi2 = g_IndexOffset;
    {
        RefCntAutoPtr<IBuffer> pStagingV;
        BufferDesc bd; bd.Usage = USAGE_STAGING; bd.CPUAccessFlags = CPU_ACCESS_WRITE; bd.Size = vb2_bytes;
        g_pDevice->CreateBuffer(bd, nullptr, &pStagingV);
        void* p = nullptr; g_pContext->MapBuffer(pStagingV, MAP_WRITE, MAP_FLAG_DO_NOT_WAIT, p);
        std::memcpy(p, lod2_verts, vb2_bytes); g_pContext->UnmapBuffer(pStagingV, MAP_WRITE);
        g_pContext->CopyBuffer(pStagingV, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
            g_GlobalVertexBuffer, bv2 * 8u, vb2_bytes, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }
    {
        RefCntAutoPtr<IBuffer> pStagingI;
        BufferDesc bd; bd.Usage = USAGE_STAGING; bd.CPUAccessFlags = CPU_ACCESS_WRITE; bd.Size = ib2_bytes;
        g_pDevice->CreateBuffer(bd, nullptr, &pStagingI);
        void* p = nullptr; g_pContext->MapBuffer(pStagingI, MAP_WRITE, MAP_FLAG_DO_NOT_WAIT, p);
        std::memcpy(p, lod2_idx, ib2_bytes); g_pContext->UnmapBuffer(pStagingI, MAP_WRITE);
        g_pContext->CopyBuffer(pStagingI, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
            g_GlobalIndexBuffer, fi2 * 4u, ib2_bytes, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }
    g_VertexOffset += hdr->lod2_vertex_count;
    g_IndexOffset  += hdr->lod2_index_count;

    const uint32_t group_id = g_LODGroupCount;
    ScryLODGroup lg;
    lg.lods[0].indexCount = hdr->lod0_index_count; lg.lods[0].firstIndex = fi0; lg.lods[0].baseVertex = bv0; lg.lods[0].threshold = 150.0f;
    lg.lods[1].indexCount = hdr->lod1_index_count; lg.lods[1].firstIndex = fi1; lg.lods[1].baseVertex = bv1; lg.lods[1].threshold = 300.0f;
    lg.lods[2].indexCount = hdr->lod2_index_count; lg.lods[2].firstIndex = fi2; lg.lods[2].baseVertex = bv2; lg.lods[2].threshold = 600.0f;
    lg.group_id = group_id;
    g_LODGroups[group_id] = lg;

    {
        LODGroupGPU gpu;
        for (int i = 0; i < 3; ++i) {
            gpu.lods[i].indexCount = lg.lods[i].indexCount;
            gpu.lods[i].firstIndex = lg.lods[i].firstIndex;
            gpu.lods[i].baseVertex = lg.lods[i].baseVertex;
            gpu.lods[i].threshold  = lg.lods[i].threshold;
        }
        gpu.local_aabb_min[0] = hdr->aabb_min[0];
        gpu.local_aabb_min[1] = hdr->aabb_min[1];
        gpu.local_aabb_min[2] = hdr->aabb_min[2];
        gpu.pad0 = 0.0f;
        gpu.local_aabb_max[0] = hdr->aabb_max[0];
        gpu.local_aabb_max[1] = hdr->aabb_max[1];
        gpu.local_aabb_max[2] = hdr->aabb_max[2];
        gpu.pad1 = 0.0f;
        g_pContext->UpdateBuffer(g_LODGroupBuffer, group_id * 80u, 80u, &gpu, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }

    g_LODGroupCount++;
    UnmapFile(&mf);
    return lg;
}

} // extern "C"
