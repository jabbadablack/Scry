#pragma once
#include <engine/engine.h>
#include <cstdint>

namespace Engine {
namespace Graphics {

// Describes where a mesh lives inside the global megabuffers.
// Returned by LoadMesh; stored in MeshData ECS component.
struct MeshAllocation {
    uint32_t indexCount;   // number of indices for this mesh
    uint32_t firstIndex;   // byte offset in units of indices into the global IB
    uint32_t baseVertex;   // index into the global VB for vertex 0 of this mesh
};

ENGINE_API bool Init(void* glfw_window_handle);
ENGINE_API void Shutdown();
ENGINE_API void BeginFrame();
ENGINE_API void Present();

// Uploads a .scrymesh into the global vertex/index megabuffers and returns
// the allocation. Returns {0,0,0} on failure (check indexCount == 0).
ENGINE_API MeshAllocation LoadMesh(const char* filepath);

} // namespace Graphics
} // namespace Engine
