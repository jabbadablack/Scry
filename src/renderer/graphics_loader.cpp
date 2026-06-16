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

namespace Engine {
namespace Graphics {

bool MapFileReadOnly(const char* path, MappedFile& out) {
    assert(path != nullptr);
    std::printf("[Graphics] Mapping file: %s\n", path);

#ifdef _WIN32
    HANDLE hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER li;
    if (!GetFileSizeEx(hFile, &li)) { CloseHandle(hFile); return false; }
    out.size = static_cast<size_t>(li.QuadPart);
    HANDLE hMap = CreateFileMappingA(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!hMap) { CloseHandle(hFile); return false; }
    out.data = MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
    out.hFile = (void*)hFile;
    out.hMap = (void*)hMap;
    if (!out.data) { CloseHandle(hMap); CloseHandle(hFile); return false; }
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

void UnmapFile(MappedFile& mf) {
    if (!mf.data) return;
#ifdef _WIN32
    UnmapViewOfFile(mf.data);
    CloseHandle((HANDLE)mf.hMap);
    CloseHandle((HANDLE)mf.hFile);
#else
    if (mf.data != MAP_FAILED) munmap(mf.data, mf.size);
    if (mf.fd >= 0) close(mf.fd);
#endif
}

LODGroup LoadMesh(const char* filepath) {
    assert(filepath != nullptr);
    assert(g_pDevice != nullptr);
    std::printf("[Graphics] Loading mesh: %s\n", filepath);

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

    const uint32_t group_id = g_LODGroupCount;

    LODGroup lg;
    lg.lods[0] = { hdr->lod0_index_count, fi0, bv0, 150.0f };
    lg.lods[1] = { hdr->lod1_index_count, fi1, bv1, 300.0f };
    lg.lods[2] = { hdr->lod2_index_count, fi2, bv2, 600.0f };
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
        g_pContext->UpdateBuffer(
            g_LODGroupBuffer,
            group_id * 48u, // sizeof(LODGroupGPU)
            48u,
            &gpu,
            RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }

    ++g_LODGroupCount;

    UnmapFile(mf);
    return lg;
}

} // namespace Graphics
} // namespace Engine
