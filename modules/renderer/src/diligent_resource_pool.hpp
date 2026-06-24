#ifndef ENGINE_RENDERER_DILIGENT_RESOURCE_POOL_HPP
#define ENGINE_RENDERER_DILIGENT_RESOURCE_POOL_HPP

#include <graphics/graphics_types.hpp>
#include <vector>
#include <debug/assert.h>

#include "Graphics/GraphicsEngine/interface/Buffer.h"
#include "Graphics/GraphicsEngine/interface/Texture.h"
#include "Graphics/GraphicsEngine/interface/PipelineState.h"
#include "Graphics/GraphicsEngine/interface/ShaderResourceBinding.h"
#include "Common/interface/RefCntAutoPtr.hpp"

namespace engine::renderer {

    template <typename T>
    class ResourcePool {
    public:
        struct Slot {
            Diligent::RefCntAutoPtr<T> obj;
            u32                        generation = 0;
        };

        ENGINE_INLINE void Insert(u32 index, u32 generation, Diligent::RefCntAutoPtr<T> obj) {
            if (index >= m_slots.size()) {
                m_slots.resize(index + 1);
            }
            m_slots[index] = Slot{ std::move(obj), generation };
        }

        [[nodiscard]] ENGINE_INLINE T* Get(u32 index, u32 generation) const {
            if (index >= m_slots.size() || m_slots[index].generation != generation) {
                return nullptr;
            }
            return m_slots[index].obj.RawPtr();
        }

        ENGINE_INLINE void Remove(u32 index, u32 generation) {
            if (index < m_slots.size() && m_slots[index].generation == generation) {
                m_slots[index].obj.Release();
                m_slots[index].generation = 0;
            }
        }

    private:
        std::vector<Slot> m_slots;
    };

} // namespace engine::renderer

#endif // ENGINE_RENDERER_DILIGENT_RESOURCE_POOL_HPP
