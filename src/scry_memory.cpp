#include <scry/scry_memory.hpp>
#include <mimalloc.h>

namespace Scry {
namespace Memory {

bool IsUsingMimalloc(const void* ptr) {
    return mi_is_in_heap_region(ptr);
}

void* AllocInDll(size_t size) {
    return ::operator new(size);
}

void FreeInDll(void* ptr) {
    ::operator delete(ptr);
}


void Arena::Init(void* backing_memory, size_t size) {
    m_backing_memory = static_cast<uint8_t*>(backing_memory);
    m_total_size = size;
    m_offset = 0;
}

void* Arena::Allocate(size_t size, size_t alignment) {
    if (!m_backing_memory || size == 0) {
        return nullptr;
    }

    uintptr_t current_ptr = reinterpret_cast<uintptr_t>(m_backing_memory + m_offset);
    uintptr_t aligned_ptr = (current_ptr + (alignment - 1)) & ~(alignment - 1);
    size_t aligned_offset = aligned_ptr - reinterpret_cast<uintptr_t>(m_backing_memory);

    if (aligned_offset + size > m_total_size) {
        return nullptr;
    }

    m_offset = aligned_offset + size;
    return reinterpret_cast<void*>(aligned_ptr);
}

void Arena::Reset() {
    m_offset = 0;
}

} // namespace Memory
} // namespace Scry

// Global operator new/delete overrides to use mimalloc
void* operator new(size_t size) {
    return mi_malloc(size);
}

void* operator new[](size_t size) {
    return mi_malloc(size);
}

void operator delete(void* p) noexcept {
    mi_free(p);
}

void operator delete[](void* p) noexcept {
    mi_free(p);
}

void operator delete(void* p, size_t size) noexcept {
    mi_free(p);
}

void operator delete[](void* p, size_t size) noexcept {
    mi_free(p);
}

void* operator new(size_t size, const std::nothrow_t&) noexcept {
    return mi_malloc(size);
}

void* operator new[](size_t size, const std::nothrow_t&) noexcept {
    return mi_malloc(size);
}

void operator delete(void* p, const std::nothrow_t&) noexcept {
    mi_free(p);
}

void operator delete[](void* p, const std::nothrow_t&) noexcept {
    mi_free(p);
}
