#ifndef ENGINE_GRAPHICS_TYPES_HPP
#define ENGINE_GRAPHICS_TYPES_HPP

#include "../OS/types.h"

namespace engine::graphics {

    template <typename Tag>
    struct ResourceHandle {
        u32 id = 0;
        [[nodiscard]] ENGINE_INLINE bool IsValid() const noexcept { return id != 0; }
        [[nodiscard]] ENGINE_INLINE u32 GetIndex() const noexcept { return id & 0xFFFFF; }       // Lower 20 bits
        [[nodiscard]] ENGINE_INLINE u32 GetGeneration() const noexcept { return (id >> 20) & 0xFFF; } // Upper 12 bits
        [[nodiscard]] ENGINE_INLINE bool operator==(const ResourceHandle& o) const noexcept { return id == o.id; }
        [[nodiscard]] ENGINE_INLINE bool operator!=(const ResourceHandle& o) const noexcept { return id != o.id; }
    };

    struct TextureTag {};
    using TextureHandle = ResourceHandle<TextureTag>;

    struct BufferTag {};
    using BufferHandle = ResourceHandle<BufferTag>;

    struct PipelineTag {};
    using PipelineHandle = ResourceHandle<PipelineTag>;

    enum class BufferUsage : u8 {
        Static,
        Dynamic
    };

    enum class BufferBind : u8 {
        Vertex,
        Index,
        Uniform,
        Instance,
        Indirect
    };

    struct BufferDesc {
        u32         size  = 0;
        BufferUsage usage = BufferUsage::Static;
        BufferBind  bind  = BufferBind::Vertex;
    };

    struct TextureDesc {
        u32  width            = 0;
        u32  height           = 0;
        u32  channels         = 4;
        bool is_render_target = false;
    };

    struct PipelineDesc {
        const char* vs_source = nullptr;
        const char* ps_source = nullptr;
    };

} // namespace engine::graphics

#endif // ENGINE_GRAPHICS_TYPES_HPP
