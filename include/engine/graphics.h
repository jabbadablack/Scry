#pragma once
#include <engine/engine.h>
#include <cstdint>

namespace Engine {
namespace Graphics {

struct MeshLOD {
    uint32_t indexCount;
    uint32_t firstIndex;
    uint32_t baseVertex;
    float    threshold;   // switch to next LOD at this distance (metres)
};

struct LODGroup {
    MeshLOD  lods[3];     // lods[0]=near, lods[1]=mid, lods[2]=far
    uint32_t group_id;    // index into g_LODGroups / g_LODGroupBuffer
};

ENGINE_API bool Init(void* glfw_window_handle);
ENGINE_API void Shutdown();
ENGINE_API void BeginFrame();
ENGINE_API void Present();

// Uploads a .scrymesh (v2, 3 LODs) into the global megabuffers.
// Returns a LODGroup with group_id assigned; failure = group_id == UINT32_MAX.
ENGINE_API LODGroup  LoadMesh(const char* filepath);
ENGINE_API const LODGroup* GetLODGroup(uint32_t id);
ENGINE_API uint32_t  GetLODGroupCount();

} // namespace Graphics
} // namespace Engine
