#ifndef ENGINE_GRAPHICS_RENDER_PACKET_HPP
#define ENGINE_GRAPHICS_RENDER_PACKET_HPP

#include "graphics_types.hpp"
#include "../math/algebra.hpp"

namespace engine::graphics {

    struct RenderPacket {
        PipelineHandle        pipeline;
        BufferHandle          vertex_buffer;
        BufferHandle          index_buffer;
        TextureHandle         texture;
        engine::math::Matrix4 transform;
        u32                   index_count = 0;
    };

} // namespace engine::graphics

#endif // ENGINE_GRAPHICS_RENDER_PACKET_HPP
