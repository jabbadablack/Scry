#include <bgfx_compute.sh>

#define INTENT_VISIBLE   1u
#define INTENT_DESTROYED 2u

// Entity intent flags uploaded each frame in ECS order
BUFFER_RO(b_intents,  uint, 0);

// Indirect draw args: 5 uints per slot
//   [0]=indexCount  [1]=instanceCount  [2]=firstIndex
//   [3]=vertexOffset  [4]=firstInstance
BUFFER_RW(b_indirect, uint, 1);

// x=indexCount  y=numEntities  z=mode (0=clear, 1=count)
uniform vec4 u_meshParams;

NUM_THREADS(64, 1, 1)
void main()
{
    uint mode = uint(u_meshParams.z);

    if (mode == 0u) {
        // Clear pass: single dispatch(1,1,1), thread 0 initialises the command
        if (gl_GlobalInvocationID.x == 0u) {
            b_indirect[0] = uint(u_meshParams.x); // indexCount
            b_indirect[1] = 0u;                    // instanceCount (accumulates below)
            b_indirect[2] = 0u;                    // firstIndex
            b_indirect[3] = 0u;                    // vertexOffset
            b_indirect[4] = 0u;                    // firstInstance
        }
        return;
    }

    // Count pass: atomic-add 1 for each visible entity
    uint entityId = gl_GlobalInvocationID.x;
    if (entityId >= uint(u_meshParams.y)) return;

    uint intent = b_intents[entityId];
    if ((intent & INTENT_VISIBLE) != 0u && (intent & INTENT_DESTROYED) == 0u) {
        atomicAdd(b_indirect[1], 1u);
    }
}
