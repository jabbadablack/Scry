#ifndef ENGINE_MEMORY_CHAINED_ARENA_HPP
#define ENGINE_MEMORY_CHAINED_ARENA_HPP

#include "../OS/types.h"
#include "../debug/assert.h"
#include "tracked_heap.hpp"
#include <atomic>
#include <mutex>
#include <cstddef>
#include <algorithm>

namespace engine {

    class ChainedArena {
    public:
        struct Block {
            std::byte*           data;
            size_t               capacity;
            std::atomic<size_t>  offset;
            Block*               next;
        };

        ENGINE_INLINE explicit ChainedArena(size_t blockSize = 64 * 1024);
        ENGINE_INLINE ~ChainedArena();

        ChainedArena(const ChainedArena&)            = delete;
        ChainedArena& operator=(const ChainedArena&) = delete;
        ChainedArena(ChainedArena&&)                 = delete;
        ChainedArena& operator=(ChainedArena&&)      = delete;

        [[nodiscard]] ENGINE_INLINE std::byte* Allocate(size_t size, size_t alignment);
        ENGINE_INLINE void Clear() noexcept;

        [[nodiscard]] ENGINE_INLINE size_t GetBlockSize() const noexcept { return m_blockSize; }

    private:
        ENGINE_INLINE Block* AllocBlock(size_t capacity);

        Block*               m_head;
        std::atomic<Block*>  m_current;
        std::mutex           m_expandMutex;
        size_t               m_blockSize;
    };

} // namespace engine

#include "chained_arena.inl"

#endif // ENGINE_MEMORY_CHAINED_ARENA_HPP
