#include <bgfx_compute.sh>

#define INTENT_VISIBLE   1u
#define INTENT_DESTROYED 2u

// ECS data uploaded each frame (entity order, not sorted)
BUFFER_RO(b_intents,  uint, 0);
BUFFER_RO(b_meshIds,  uint, 1);

// Indirect draw argument buffer — 5 uints per draw entry:
//   [IndexCount, InstanceCount, FirstIndex, BaseVertex, BaseInstance]
// We atomically increment InstanceCount (offset +1) for each visible entity.
BUFFER_RW(b_indirect, uint, 2);

NUM_THREADS(64, 1, 1)
void main()
{
    uint entityId = gl_GlobalInvocationID.x;
    uint intent   = b_intents[entityId];

    if ((intent & INTENT_VISIBLE) != 0u && (intent & INTENT_DESTROYED) == 0u) {
        uint meshId = b_meshIds[entityId];
        atomicAdd(b_indirect[meshId * 5u + 1u], 1u);
    }
}
