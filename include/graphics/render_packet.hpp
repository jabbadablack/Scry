#ifndef ENGINE_GRAPHICS_RENDER_PACKET_HPP
#define ENGINE_GRAPHICS_RENDER_PACKET_HPP

#include "graphics_types.hpp"
#include "../math/algebra.hpp"

namespace engine::graphics {

    struct RenderPacket {
        PipelineHandle        pipeline;
        BufferHandle          vertex_buffer;
        BufferHandle          index_buffer;
        BufferHandle          instance_buffer; // New: Bound to slot 1
        BufferHandle          indirect_buffer; // New: Contains DrawIndexedIndirectAttribs
        u32                   indirect_offset = 0;

        TextureHandle         texture;
        engine::math::Matrix4 transform;       // Fallback for non-instanced
        u32                   index_count = 0; // Fallback for non-indirect
    };

} // namespace engine::graphics

#endif // ENGINE_GRAPHICS_RENDER_PACKET_HPP
