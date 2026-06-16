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

/**
 * @brief Initializes the graphics system.
 * 
 * Welcome! This function kicks off the graphics system using your GLFW window.
 * 
 * @param glfw_window_handle Pointer to the GLFW window handle.
 * @return true if initialization was successful, false otherwise.
 * 
 * @example
 * if (Engine::Graphics::Init(myWindow)) {
 *     // Graphics are ready to go!
 * }
 */
ENGINE_API bool Init(void* glfw_window_handle);

/**
 * @brief Shuts down the graphics system.
 * 
 * Time to say goodbye! This function cleans up all graphics resources.
 * 
 * @example
 * Engine::Graphics::Shutdown();
 */
ENGINE_API void Shutdown();

/**
 * @brief Begins a new frame.
 * 
 * Ready to draw? This function prepares everything for a fresh new frame.
 * 
 * @example
 * Engine::Graphics::BeginFrame();
 */
ENGINE_API void BeginFrame();

/**
 * @brief Presents the current frame to the screen.
 * 
 * Tada! This function shows your beautiful work on the screen.
 * 
 * @example
 * Engine::Graphics::Present();
 */
ENGINE_API void Present();

/**
 * @brief Uploads a mesh file into global megabuffers.
 * 
 * This function takes a .scrymesh file and uploads it so you can use it in 
 * your scene. It handles Level of Detail for you!
 * 
 * @param filepath The path to the .scrymesh file.
 * @return A LODGroup structure containing mesh info.
 * 
 * @example
 * Engine::Graphics::LODGroup group = Engine::Graphics::LoadMesh("assets/suzanne.scrymesh");
 */
ENGINE_API LODGroup  LoadMesh(const char* filepath);

/**
 * @brief Retrieves a LOD group by ID.
 * 
 * Need information about a specific mesh group? This function will find it 
 * for you using its ID.
 * 
 * @param id The ID of the LOD group.
 * @return Pointer to the LODGroup, or nullptr if not found.
 * 
 * @example
 * const Engine::Graphics::LODGroup* group = Engine::Graphics::GetLODGroup(myId);
 */
ENGINE_API const LODGroup* GetLODGroup(uint32_t id);

/**
 * @brief Gets the total number of loaded LOD groups.
 * 
 * How many meshes have we loaded? This function gives you the total count.
 * 
 * @return The number of loaded LOD groups.
 * 
 * @example
 * uint32_t count = Engine::Graphics::GetLODGroupCount();
 */
ENGINE_API uint32_t  GetLODGroupCount();

} // namespace Graphics
} // namespace Engine
