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
 */
Diligent::IRenderDevice*  GetDevice();

/**
 * @brief Retrieves the Diligent device context.
 */
Diligent::IDeviceContext* GetContext();

/**
 * @brief Retrieves the Diligent swap chain.
 */
Diligent::ISwapChain*     GetSwapChain();

/**
 * @brief Retrieves the global vertex buffer.
 */
Diligent::IBuffer* GetGlobalVertexBuffer();

/**
 * @brief Retrieves the global index buffer.
 */
Diligent::IBuffer* GetGlobalIndexBuffer();

/**
 * @brief Retrieves the LOD group buffer.
 */
Diligent::IBuffer* GetLODGroupBuffer();

} // namespace Graphics
} // namespace Engine
