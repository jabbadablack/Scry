#ifndef ENGINE_RESOURCE_STB_IMPL_INL
#define ENGINE_RESOURCE_STB_IMPL_INL

#include "memory/tracked_heap.hpp"
#include <cstring>

namespace engine::io::detail {

    struct StbAllocationHeader {
        size_t size;
        size_t alignment;
        void* original_ptr;
    };

    // StbFree defined first to allow usage in StbRealloc, utilizing the size = 0 parameter
    inline void StbFree(void* p) {
        engine::TrackedHeap::Deallocate(p, 0);
    }

    inline void* StbMalloc(size_t sz) {
        if (sz == 0) return nullptr;
        return engine::TrackedHeap::Allocate(sz, 16);
    }

    inline void* StbRealloc(void* p, size_t newsz) {
        if (p == nullptr) {
            return StbMalloc(newsz);
        }
        if (newsz == 0) {
            StbFree(p);
            return nullptr;
        }

        // Fetch old size from TrackedHeap AllocationHeader
        StbAllocationHeader* header = reinterpret_cast<StbAllocationHeader*>(
            reinterpret_cast<uintptr_t>(p) - sizeof(StbAllocationHeader));
        size_t oldsz = header->size;

        void* new_ptr = engine::TrackedHeap::Allocate(newsz, 16);
        if (new_ptr != nullptr) {
            size_t copy_size = oldsz < newsz ? oldsz : newsz;
            std::memcpy(new_ptr, p, copy_size);
            engine::TrackedHeap::Deallocate(p, oldsz);
        }
        return new_ptr;
    }

} // namespace engine::io::detail

// Wire stb_image allocator hooks to our tracked heap wrappers
#define STBI_MALLOC(sz)          engine::io::detail::StbMalloc(sz)
#define STBI_REALLOC(p, newsz)    engine::io::detail::StbRealloc(p, newsz)
#define STBI_FREE(p)             engine::io::detail::StbFree(p)

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#endif // ENGINE_RESOURCE_STB_IMPL_INL
