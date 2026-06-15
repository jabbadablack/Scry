#pragma once
#include <engine/engine.h>
#include <cstdint>

/* Public graphics API — Diligent Engine details are hidden in graphics.cpp.
 * Call Init() once from EngineRun after the SDL window exists.
 * Call Present() at the end of each frame, after ecs_progress.
 * LoadMesh() reads a .scrymesh file and returns an opaque handle.
 */

namespace Engine {
namespace Graphics {

static constexpr uint32_t MAX_MESHES   = 256u;
static constexpr uint32_t INVALID_MESH = 0xFFFFFFFFu;

ENGINE_API bool     Init(void* sdl_window_handle);
ENGINE_API void     Shutdown();
/* BeginFrame — acquire the back buffer, clear colour + depth.
 * Must be called once per frame before ecs_progress so the swap chain
 * image is acquired even when no mesh entities exist. */
ENGINE_API void     BeginFrame();
ENGINE_API void     Present();

/* Returns 0xFFFFFFFF on failure. */
ENGINE_API uint32_t LoadMesh(const char* filepath);
ENGINE_API void     FreeMesh(uint32_t handle);

} // namespace Graphics
} // namespace Engine
