#ifndef ENGINE_GRAPHICS_MANAGER_HPP
#define ENGINE_GRAPHICS_MANAGER_HPP

#include "graphics_types.hpp"
#include "../memory/chained_arena.hpp"
#include <atomic>
#include <mutex>
#include <vector>

namespace engine::graphics {

    struct BufferIntent {
        BufferHandle handle;
        BufferDesc   desc;
        const void*  data;
    };

    struct TextureIntent {
        TextureHandle handle;
        TextureDesc   desc;
        const void*   data;
    };

    struct PipelineIntent {
        PipelineHandle handle;
        PipelineDesc   desc;
    };

    struct DeletionIntent {
        u32 id; // We can call it handle_id or id, let's look at the prompt: "struct DeletionIntent { u32 handle_id; u64 target_frame; };"
        u32 handle_id;
        u64 target_frame;
    };

    class IGraphics {
    public:
        ENGINE_INLINE IGraphics() = default;
        ENGINE_INLINE ~IGraphics() = default;

        IGraphics(const IGraphics&)            = delete;
        IGraphics& operator=(const IGraphics&) = delete;
        IGraphics(IGraphics&&)                 = delete;
        IGraphics& operator=(IGraphics&&)      = delete;

        [[nodiscard]] ENGINE_INLINE BufferHandle CreateBuffer(const BufferDesc& desc, const void* initial_data = nullptr);
        [[nodiscard]] ENGINE_INLINE TextureHandle CreateTexture(const TextureDesc& desc, const void* initial_data = nullptr);
        [[nodiscard]] ENGINE_INLINE PipelineHandle CreatePipeline(const PipelineDesc& desc);

        ENGINE_INLINE void DestroyBuffer(BufferHandle handle);
        ENGINE_INLINE void DestroyTexture(TextureHandle handle);
        ENGINE_INLINE void DestroyPipeline(PipelineHandle handle);

        ENGINE_INLINE void FlushCreations(
            std::vector<BufferIntent>& outBuffers,
            std::vector<TextureIntent>& outTextures,
            std::vector<PipelineIntent>& outPipelines);

        ENGINE_INLINE void FlushDeletions(u64 frameCount, std::vector<DeletionIntent>& outDeletions);

    private:
        std::atomic<u32> m_nextIndex{1};
        std::mutex       m_commandMutex;

        std::vector<BufferIntent>   m_bufferIntents;
        std::vector<TextureIntent>  m_textureIntents;
        std::vector<PipelineIntent> m_pipelineIntents;
        std::vector<DeletionIntent> m_deletionIntents;
    };

} // namespace engine::graphics

#include "IGraphics.inl"

#endif // ENGINE_GRAPHICS_MANAGER_HPP
