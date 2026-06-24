#ifndef ENGINE_MEMORY_HANDLE_HPP
#define ENGINE_MEMORY_HANDLE_HPP

#include "../OS/types.h"

namespace engine {

    template <typename T>
    struct Handle {
        u32 id = 0;

        [[nodiscard]] ENGINE_INLINE bool IsValid() const noexcept { return id != 0; }
        [[nodiscard]] ENGINE_INLINE u32 GetBlockIndex()   const noexcept { return (id >> 20) - 1; }
        [[nodiscard]] ENGINE_INLINE u32 GetElementIndex() const noexcept { return id & 0xFFFFF; }

        [[nodiscard]] ENGINE_INLINE bool operator==(const Handle<T>& o) const noexcept { return id == o.id; }
        [[nodiscard]] ENGINE_INLINE bool operator!=(const Handle<T>& o) const noexcept { return id != o.id; }
    };

} // namespace engine

#endif // ENGINE_MEMORY_HANDLE_HPP
