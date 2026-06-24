#ifndef ENGINE_CORE_ARRAY_HPP
#define ENGINE_CORE_ARRAY_HPP

#include "../OS/types.h"
#include "../debug/assert.h"
#include "chained_arena.hpp"
#include <new>
#include <type_traits>
#include <cstddef>

namespace engine {


    template <typename T>
    class ArenaArray {
    public:
        // Constructor that pre-allocates memory from a ChainedArena
        ENGINE_INLINE ArenaArray(ChainedArena& arena, size_t capacity);

        // Destructor that manually calls element destructors
        ENGINE_INLINE ~ArenaArray();

        // Disable copy semantics
        ArenaArray(const ArenaArray&) = delete;
        ArenaArray& operator=(const ArenaArray&) = delete;

        // Support move semantics
        ENGINE_INLINE ArenaArray(ArenaArray&& other) noexcept;
        ENGINE_INLINE ArenaArray& operator=(ArenaArray&& other) noexcept;

        // Populate elements
        ENGINE_INLINE void PushBack(const T& value);
        ENGINE_INLINE void PushBack(T&& value);

        ENGINE_INLINE void Clear() noexcept;

        // Element Access & Iteration APIs
        [[nodiscard]] ENGINE_INLINE T* begin() noexcept;
        [[nodiscard]] ENGINE_INLINE const T* begin() const noexcept;

        [[nodiscard]] ENGINE_INLINE T* end() noexcept;
        [[nodiscard]] ENGINE_INLINE const T* end() const noexcept;

        [[nodiscard]] ENGINE_INLINE T* data() noexcept;
        [[nodiscard]] ENGINE_INLINE const T* data() const noexcept;

        [[nodiscard]] ENGINE_INLINE size_t size() const noexcept;
        [[nodiscard]] ENGINE_INLINE size_t capacity() const noexcept;

        [[nodiscard]] ENGINE_INLINE T& operator[](size_t index);
        [[nodiscard]] ENGINE_INLINE const T& operator[](size_t index) const;

    private:
        T* m_data;
        size_t m_size;
        size_t m_capacity;
    };


} // namespace engine

// Include inline template implementation
#include "array.inl"

#endif // ENGINE_CORE_ARRAY_HPP
