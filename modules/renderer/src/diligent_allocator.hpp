#pragma once

#include <memory/tracked_heap.hpp>
#include "Primitives/interface/MemoryAllocator.h"

namespace engine {
namespace renderer {

    class EngineDiligentAllocator final : public Diligent::IMemoryAllocator {
    public:
        void* Allocate(size_t Size,
                       const Diligent::Char* /*dbgDescription*/,
                       const char*           /*dbgFileName*/,
                       const Diligent::Int32 /*dbgLineNumber*/) override {
            return engine::TrackedHeap::Allocate(Size, alignof(std::max_align_t));
        }

        void Free(void* Ptr) override {
            engine::TrackedHeap::Deallocate(Ptr, 0);
        }

        void* AllocateAligned(size_t Size, size_t Alignment,
                              const Diligent::Char* /*dbgDescription*/,
                              const char*           /*dbgFileName*/,
                              const Diligent::Int32 /*dbgLineNumber*/) override {
            return engine::TrackedHeap::Allocate(Size, Alignment);
        }

        void FreeAligned(void* Ptr) override {
            engine::TrackedHeap::Deallocate(Ptr, 0);
        }
    };

} // namespace renderer
} // namespace engine
