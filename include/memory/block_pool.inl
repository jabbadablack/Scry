#ifndef ENGINE_MEMORY_BLOCK_POOL_INL
#define ENGINE_MEMORY_BLOCK_POOL_INL

namespace engine {

    template <typename T, size_t ElementsPerBlock>
    ENGINE_INLINE BlockPool<T, ElementsPerBlock>::~BlockPool() {
        if constexpr (!std::is_trivially_destructible_v<T>) {
            for (size_t i = 0; i < m_currentElement; ++i) {
                size_t bi = i / ElementsPerBlock;
                size_t ei = i % ElementsPerBlock;
                m_blocks[bi][ei].~T();
            }
        }
        for (T* block : m_blocks) {
            TrackedHeap::Deallocate(block, sizeof(T) * ElementsPerBlock);
        }
    }

    template <typename T, size_t ElementsPerBlock>
    ENGINE_INLINE Handle<T> BlockPool<T, ElementsPerBlock>::Push(const T& item) {
        size_t bi = m_currentElement / ElementsPerBlock;
        size_t ei = m_currentElement % ElementsPerBlock;

        if (bi >= m_blocks.size()) {
            ENGINE_ASSERT(bi < 4094u, "BlockPool: max block count (4094) exceeded");
            void* raw = TrackedHeap::Allocate(sizeof(T) * ElementsPerBlock, alignof(T));
            ENGINE_ASSERT(raw != nullptr, "BlockPool: TrackedHeap allocation failed for new block");
            m_blocks.push_back(static_cast<T*>(raw));
        }

        new (&m_blocks[bi][ei]) T(item);
        ++m_currentElement;

        return Handle<T>{ (static_cast<u32>(bi + 1) << 20) | static_cast<u32>(ei) };
    }

    template <typename T, size_t ElementsPerBlock>
    ENGINE_INLINE T& BlockPool<T, ElementsPerBlock>::Get(Handle<T> handle) {
        ENGINE_ASSERT(handle.IsValid(), "BlockPool::Get called with an invalid handle");
        ENGINE_ASSERT(handle.GetBlockIndex() < m_blocks.size(), "BlockPool::Get: block index out of range");
        ENGINE_ASSERT(handle.GetElementIndex() < ElementsPerBlock, "BlockPool::Get: element index out of range");
        return m_blocks[handle.GetBlockIndex()][handle.GetElementIndex()];
    }

    template <typename T, size_t ElementsPerBlock>
    ENGINE_INLINE const T& BlockPool<T, ElementsPerBlock>::Get(Handle<T> handle) const {
        ENGINE_ASSERT(handle.IsValid(), "BlockPool::Get called with an invalid handle");
        ENGINE_ASSERT(handle.GetBlockIndex() < m_blocks.size(), "BlockPool::Get: block index out of range");
        ENGINE_ASSERT(handle.GetElementIndex() < ElementsPerBlock, "BlockPool::Get: element index out of range");
        return m_blocks[handle.GetBlockIndex()][handle.GetElementIndex()];
    }

} // namespace engine

#endif // ENGINE_MEMORY_BLOCK_POOL_INL
