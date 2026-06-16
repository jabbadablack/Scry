#pragma once
#include "Graphics/GraphicsEngine/interface/RenderDevice.h"
#include "Graphics/GraphicsEngine/interface/DeviceContext.h"
#include "Graphics/GraphicsEngine/interface/SwapChain.h"
#include "Graphics/GraphicsEngine/interface/Buffer.h"
#include "Graphics/GraphicsEngine/interface/BufferView.h"
#include "Common/interface/RefCntAutoPtr.hpp"

namespace Engine {
namespace Graphics {

Diligent::IRenderDevice*  GetDevice();
Diligent::IDeviceContext* GetContext();
Diligent::ISwapChain*     GetSwapChain();

Diligent::IBuffer* GetGlobalVertexBuffer();
Diligent::IBuffer* GetGlobalIndexBuffer();

} // namespace Graphics
} // namespace Engine
