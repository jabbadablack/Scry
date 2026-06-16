#pragma once

#ifdef __cplusplus
#include "Graphics/GraphicsEngine/interface/RenderDevice.h"
#include "Graphics/GraphicsEngine/interface/DeviceContext.h"
#include "Graphics/GraphicsEngine/interface/SwapChain.h"
#include "Graphics/GraphicsEngine/interface/Buffer.h"
#include "Graphics/GraphicsEngine/interface/BufferView.h"
#include "Common/interface/RefCntAutoPtr.hpp"

extern "C" {
#endif

#include <stdint.h>

/**
 * @brief Retrieves the Diligent render device.
 */
void* ScryGraphics_GetDevice(void);

/**
 * @brief Retrieves the Diligent device context.
 */
void* ScryGraphics_GetContext(void);

/**
 * @brief Retrieves the Diligent swap chain.
 */
void* ScryGraphics_GetSwapChain(void);

/**
 * @brief Retrieves the global vertex buffer.
 */
void* ScryGraphics_GetGlobalVertexBuffer(void);

/**
 * @brief Retrieves the global index buffer.
 */
void* ScryGraphics_GetGlobalIndexBuffer(void);

/**
 * @brief Retrieves the LOD group buffer.
 */
void* ScryGraphics_GetLODGroupBuffer(void);

#ifdef __cplusplus
}
#endif
