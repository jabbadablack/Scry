#pragma once
/* Private — included only by graphics.cpp and renderer.cpp.
 * Never expose Diligent types through engine/graphics.hpp.
 *
 * Paths verified against build/_deps/DiligentCore at configure time:
 *   Common/interface/RefCntAutoPtr.hpp
 *   Graphics/GraphicsEngine/interface/{RenderDevice,DeviceContext,SwapChain,Buffer,PipelineState,ShaderResourceBinding}.h
 *   Graphics/GraphicsTools/interface/MapHelper.hpp
 */

#include <Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <Graphics/GraphicsEngine/interface/SwapChain.h>
#include <Graphics/GraphicsEngine/interface/Buffer.h>
#include <Graphics/GraphicsEngine/interface/PipelineState.h>
#include <Graphics/GraphicsEngine/interface/ShaderResourceBinding.h>
#include <Graphics/GraphicsTools/interface/MapHelper.hpp>
#include <Common/interface/RefCntAutoPtr.hpp>
#include <cstdint>

namespace Engine {
namespace Graphics {

struct MeshBuffers {
    Diligent::RefCntAutoPtr<Diligent::IBuffer> vertices;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> indices;
    uint32_t                                    index_count = 0;
    bool                                        in_use      = false;
};

/* Accessors used by renderer.cpp (within the same shared library). */
Diligent::IRenderDevice*  GetDevice();
Diligent::IDeviceContext* GetContext();
Diligent::ISwapChain*     GetSwapChain();
MeshBuffers*              GetMesh(uint32_t handle);

} // namespace Graphics
} // namespace Engine
