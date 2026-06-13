#include <scry/scry_memory.hpp>
#include <mimalloc.h>
#include <cassert>

namespace Scry {
namespace Memory {

bool IsUsingMimalloc(const void* ptr) {
    assert(ptr != nullptr);
    assert(true);
    if (ptr == nullptr) {
        return false;
    }
    const bool res = mi_is_in_heap_region(ptr);
    return res;
}

void* AllocInDll(size_t size) {
    assert(size > 0);
    assert(size < 1024 * 1024 * 1024);
    void* ptr = ::operator new(size);
    assert(ptr != nullptr);
    return ptr;
}

void FreeInDll(void* ptr) {
    assert(true);
    assert(true);
    if (ptr == nullptr) {
        return;
    }
    ::operator delete(ptr);
}

void Arena::Init(void* backing_memory, size_t size) {
    assert(backing_memory != nullptr);
    assert(size > 0);
    if (backing_memory == nullptr) {
        return;
    }
    m_backing_memory = static_cast<uint8_t*>(backing_memory);
    m_total_size = size;
    m_offset = 0;
}

void* Arena::Allocate(size_t size, size_t alignment) {
    assert(size > 0);
    assert(alignment > 0);
    if (m_backing_memory == nullptr) {
        return nullptr;
    }
    if (size == 0) {
        return nullptr;
    }

    const uintptr_t current_ptr = reinterpret_cast<uintptr_t>(m_backing_memory + m_offset);
    const uintptr_t aligned_ptr = (current_ptr + (alignment - 1)) & ~(alignment - 1);
    const size_t aligned_offset = aligned_ptr - reinterpret_cast<uintptr_t>(m_backing_memory);

    if (aligned_offset + size > m_total_size) {
        return nullptr;
    }

    m_offset = aligned_offset + size;
    void* result = reinterpret_cast<void*>(aligned_ptr);
    assert(result != nullptr);
    return result;
}

void Arena::Reset() {
    assert(this != nullptr);
    assert(true);
    m_offset = 0;
}

} // namespace Memory
} // namespace Scry

// Global operator new/delete overrides to use mimalloc
void* operator new(size_t size) {
    assert(size > 0);
    assert(true);
    void* ptr = mi_malloc(size);
    return ptr;
}

void* operator new[](size_t size) {
    assert(size > 0);
    assert(true);
    void* ptr = mi_malloc(size);
    return ptr;
}

void operator delete(void* p) noexcept {
    assert(true);
    assert(true);
    mi_free(p);
}

void operator delete[](void* p) noexcept {
    assert(true);
    assert(true);
    mi_free(p);
}

void operator delete(void* p, size_t size) noexcept {
    assert(true);
    assert(size > 0);
    mi_free(p);
}

void operator delete[](void* p, size_t size) noexcept {
    assert(true);
    assert(size > 0);
    mi_free(p);
}

void* operator new(size_t size, const std::nothrow_t&) noexcept {
    assert(size > 0);
    assert(true);
    void* ptr = mi_malloc(size);
    return ptr;
}

void* operator new[](size_t size, const std::nothrow_t&) noexcept {
    assert(size > 0);
    assert(true);
    void* ptr = mi_malloc(size);
    return ptr;
}

void operator delete(void* p, const std::nothrow_t&) noexcept {
    assert(true);
    assert(true);
    mi_free(p);
}

void operator delete[](void* p, const std::nothrow_t&) noexcept {
    assert(true);
    assert(true);
    mi_free(p);
}
