#ifndef ENGINE_GRAPHICS_MANAGER_INL
#define ENGINE_GRAPHICS_MANAGER_INL

namespace engine::graphics {

    ENGINE_INLINE BufferHandle IGraphics::CreateBuffer(const BufferDesc& desc, const void* initial_data) {
        u32 idx = m_nextIndex.fetch_add(1, std::memory_order_relaxed);
        BufferHandle handle{ (1 << 20) | idx };

        std::lock_guard<std::mutex> lock(m_commandMutex);
        m_bufferIntents.push_back(BufferIntent{ handle, desc, initial_data });
        return handle;
    }

    ENGINE_INLINE TextureHandle IGraphics::CreateTexture(const TextureDesc& desc, const void* initial_data) {
        u32 idx = m_nextIndex.fetch_add(1, std::memory_order_relaxed);
        TextureHandle handle{ (1 << 20) | idx };

        std::lock_guard<std::mutex> lock(m_commandMutex);
        m_textureIntents.push_back(TextureIntent{ handle, desc, initial_data });
        return handle;
    }

    ENGINE_INLINE PipelineHandle IGraphics::CreatePipeline(const PipelineDesc& desc) {
        u32 idx = m_nextIndex.fetch_add(1, std::memory_order_relaxed);
        PipelineHandle handle{ (1 << 20) | idx };

        std::lock_guard<std::mutex> lock(m_commandMutex);
        m_pipelineIntents.push_back(PipelineIntent{ handle, desc });
        return handle;
    }

    ENGINE_INLINE void IGraphics::DestroyBuffer(BufferHandle handle) {
        std::lock_guard<std::mutex> lock(m_commandMutex);
        m_deletionIntents.push_back(DeletionIntent{ handle.id, 0 });
    }

    ENGINE_INLINE void IGraphics::DestroyTexture(TextureHandle handle) {
        std::lock_guard<std::mutex> lock(m_commandMutex);
        m_deletionIntents.push_back(DeletionIntent{ handle.id, 0 });
    }

    ENGINE_INLINE void IGraphics::DestroyPipeline(PipelineHandle handle) {
        std::lock_guard<std::mutex> lock(m_commandMutex);
        m_deletionIntents.push_back(DeletionIntent{ handle.id, 0 });
    }

    ENGINE_INLINE void IGraphics::FlushCreations(
        std::vector<BufferIntent>& outBuffers,
        std::vector<TextureIntent>& outTextures,
        std::vector<PipelineIntent>& outPipelines)
    {
        std::lock_guard<std::mutex> lock(m_commandMutex);
        outBuffers.swap(m_bufferIntents);
        outTextures.swap(m_textureIntents);
        outPipelines.swap(m_pipelineIntents);
    }

    ENGINE_INLINE void IGraphics::FlushDeletions(u64 frameCount, std::vector<DeletionIntent>& outDeletions) {
        std::lock_guard<std::mutex> lock(m_commandMutex);
        outDeletions.swap(m_deletionIntents);
    }

} // namespace engine::graphics

#endif // ENGINE_GRAPHICS_MANAGER_INL
