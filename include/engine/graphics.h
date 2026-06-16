#pragma once
#include <engine/engine.h>
#include <cstdint>

namespace Engine {
namespace Graphics {

static constexpr uint32_t MAX_MESHES   = 256u;
static constexpr uint32_t INVALID_MESH = 0xFFFFFFFFu;

ENGINE_API bool     Init(void* glfw_window_handle);
ENGINE_API void     Shutdown();
ENGINE_API void     BeginFrame();
ENGINE_API void     Present();

ENGINE_API uint32_t LoadMesh(const char* filepath);
ENGINE_API void     FreeMesh(uint32_t handle);
ENGINE_API uint32_t GetIndexCount(uint32_t handle);

} // namespace Graphics
} // namespace Engine
