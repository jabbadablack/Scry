#ifndef ENGINE_MEMORY_BLOCK_POOL_HPP
#define ENGINE_MEMORY_BLOCK_POOL_HPP

#include "../OS/types.h"
#include "../debug/assert.h"
#include "handle.hpp"
#include "tracked_heap.hpp"
#include <new>
#include <type_traits>
#include <vector>

namespace engine {

template <typename T, size_t ElementsPerBlock = 64>
class BlockPool {
public:
    ENGINE_INLINE BlockPool() = default;
    ENGINE_INLINE ~BlockPool();

    BlockPool(const BlockPool&) = delete;
    BlockPool& operator=(const BlockPool&) = delete;
    BlockPool(BlockPool&&) = delete;
    BlockPool& operator=(BlockPool&&) = delete;

    [[nodiscard]] ENGINE_INLINE Handle<T> Push(const T& item);
    [[nodiscard]] ENGINE_INLINE T& Get(Handle<T> handle);
    [[nodiscard]] ENGINE_INLINE const T& Get(Handle<T> handle) const;

    [[nodiscard]] ENGINE_INLINE size_t Size() const noexcept { return m_currentElement; }
    [[nodiscard]] ENGINE_INLINE bool IsEmpty() const noexcept { return m_currentElement == 0; }

private:
    std::vector<T*> m_blocks;
    size_t m_currentElement = 0;
};

} // namespace engine

#include "block_pool.inl"

#endif // ENGINE_MEMORY_BLOCK_POOL_HPP
