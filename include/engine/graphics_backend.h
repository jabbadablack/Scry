#pragma once
#include "Graphics/GraphicsEngine/interface/RenderDevice.h"
#include "Graphics/GraphicsEngine/interface/DeviceContext.h"
#include "Graphics/GraphicsEngine/interface/SwapChain.h"
#include "Graphics/GraphicsEngine/interface/Buffer.h"
#include "Graphics/GraphicsEngine/interface/BufferView.h"
#include "Common/interface/RefCntAutoPtr.hpp"

namespace Engine {
namespace Graphics {

/**
 * @brief Retrieves the Diligent render device.
 * 
 * Need the render device? This function will fetch the global Diligent 
 * render device for you. It's the gateway to all your GPU resource creation!
 * 
 * @return Pointer to the Diligent::IRenderDevice.
 * 
 * @example
 * Diligent::IRenderDevice* device = Engine::Graphics::GetDevice();
 */
Diligent::IRenderDevice*  GetDevice();

/**
 * @brief Retrieves the Diligent device context.
 * 
 * This function returns the primary Diligent device context, which you'll 
 * need for issuing draw commands and managing GPU state.
 * 
 * @return Pointer to the Diligent::IDeviceContext.
 * 
 * @example
 * Diligent::IDeviceContext* context = Engine::Graphics::GetContext();
 */
Diligent::IDeviceContext* GetContext();

/**
 * @brief Retrieves the Diligent swap chain.
 * 
 * Want to present something to the screen? Use this to get the global swap chain!
 * 
 * @return Pointer to the Diligent::ISwapChain.
 * 
 * @example
 * Diligent::ISwapChain* swapChain = Engine::Graphics::GetSwapChain();
 */
Diligent::ISwapChain*     GetSwapChain();

/**
 * @brief Retrieves the global vertex buffer.
 * 
 * This function provides access to the main vertex buffer used by the engine 
 * for rendering geometry.
 * 
 * @return Pointer to the Diligent::IBuffer for vertices.
 * 
 * @example
 * Diligent::IBuffer* vb = Engine::Graphics::GetGlobalVertexBuffer();
 */
Diligent::IBuffer* GetGlobalVertexBuffer();

/**
 * @brief Retrieves the global index buffer.
 * 
 * This function provides access to the main index buffer used for 
 * indexed rendering.
 * 
 * @return Pointer to the Diligent::IBuffer for indices.
 * 
 * @example
 * Diligent::IBuffer* ib = Engine::Graphics::GetGlobalIndexBuffer();
 */
Diligent::IBuffer* GetGlobalIndexBuffer();

/**
 * @brief Retrieves the LOD group buffer.
 * 
 * This function returns the buffer used for managing Level of Detail (LOD) 
 * group information.
 * 
 * @return Pointer to the Diligent::IBuffer for LOD data.
 * 
 * @example
 * Diligent::IBuffer* lodBuffer = Engine::Graphics::GetLODGroupBuffer();
 */
Diligent::IBuffer* GetLODGroupBuffer();

} // namespace Graphics
} // namespace Engine
