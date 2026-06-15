#pragma once
// Internal header — included only by engine source files, not plugins.
// Exposes DiligentCore types for renderer access to graphics resources.

#include "Graphics/GraphicsEngine/interface/RenderDevice.h"
#include "Graphics/GraphicsEngine/interface/DeviceContext.h"
#include "Graphics/GraphicsEngine/interface/SwapChain.h"
#include "Graphics/GraphicsEngine/interface/Buffer.h"
#include "Graphics/GraphicsEngine/interface/BufferView.h"
#include "Common/interface/RefCntAutoPtr.hpp"

#include <cstdint>

namespace Engine {
namespace Graphics {

Diligent::IRenderDevice*  GetDevice();
Diligent::IDeviceContext* GetContext();
Diligent::ISwapChain*     GetSwapChain();

// Returns the IBuffer created with BIND_SHADER_RESOURCE for vertex pulling.
Diligent::IBuffer* GetVertexBuffer(uint32_t handle);
Diligent::IBuffer* GetIndexBuffer(uint32_t handle);

} // namespace Graphics
} // namespace Engine
