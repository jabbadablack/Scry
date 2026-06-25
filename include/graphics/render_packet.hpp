#ifndef ENGINE_GRAPHICS_RENDER_PACKET_HPP
#define ENGINE_GRAPHICS_RENDER_PACKET_HPP

#include "graphics_types.hpp"
#include "../math/algebra.hpp"

namespace engine::graphics {

    struct RenderPacket {
        u64                   sort_key = 0;
        RenderPass            pass     = RenderPass::Opaque;

        PipelineHandle        pipeline;
        BufferHandle          vertex_buffer;
        BufferHandle          index_buffer;
        BufferHandle          instance_buffer;
        BufferHandle          indirect_buffer;
        u32                   indirect_offset = 0;

        TextureHandle         texture;
        engine::math::Matrix4 transform;
        u32                   index_count = 0;
    };

} // namespace engine::graphics

#endif // ENGINE_GRAPHICS_RENDER_PACKET_HPP
