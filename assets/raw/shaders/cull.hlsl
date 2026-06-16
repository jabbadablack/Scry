// GPU frustum culler — dispatched once per frame before the opaque pass.
// The indirect args buffer is pre-filled by the CPU (indexCount/firstIndex/baseVertex)
// with instanceCount=0. Each thread atomically increments instanceCount if visible.

#define THREADS 64

struct ScryMat4        { float4 c0; float4 c1; float4 c2; float4 c3; };
struct ShaderAABB      { float3 aabb_min; float pad0; float3 aabb_max; float pad1; };
struct IndirectCommand { uint indexCount; uint instanceCount; uint firstIndex; uint baseVertex; uint firstInstance; };

StructuredBuffer<ScryMat4>          b_matrices        : register(t0);
StructuredBuffer<ShaderAABB>        b_bounds          : register(t1);
RWStructuredBuffer<IndirectCommand> b_indirectArgs    : register(u0);
RWStructuredBuffer<uint>            b_visibleInstances: register(u1);

cbuffer CullParams : register(b0) {
    float4 frustumPlanes[6]; // pre-normalized (xyz=normal, w=d)
    uint4  entityData;       // .x = entityCount; .yzw = padding
};

[numthreads(THREADS, 1, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID) {
    uint gid = DTid.x;
    if (gid >= entityData.x) return;

    ScryMat4  mat  = b_matrices[gid];
    ShaderAABB aabb = b_bounds[gid];

    // Transform AABB center/half-extents to world space via the upper-left 3x3
    float3 lc = (aabb.aabb_min + aabb.aabb_max) * 0.5;
    float3 le = (aabb.aabb_max - aabb.aabb_min) * 0.5;

    float3 wc = mat.c0.xyz * lc.x + mat.c1.xyz * lc.y + mat.c2.xyz * lc.z + mat.c3.xyz;

    float3 we;
    we.x = abs(mat.c0.x) * le.x + abs(mat.c1.x) * le.y + abs(mat.c2.x) * le.z;
    we.y = abs(mat.c0.y) * le.x + abs(mat.c1.y) * le.y + abs(mat.c2.y) * le.z;
    we.z = abs(mat.c0.z) * le.x + abs(mat.c1.z) * le.y + abs(mat.c2.z) * le.z;

    bool visible = true;
    [unroll]
    for (int p = 0; p < 6; ++p) {
        float4 plane = frustumPlanes[p];
        float  r     = abs(plane.x) * we.x + abs(plane.y) * we.y + abs(plane.z) * we.z;
        float  dist  = dot(float4(wc, 1.0), plane);
        if (dist + r < 0.0) { visible = false; break; }
    }

    if (visible) {
        uint slot;
        InterlockedAdd(b_indirectArgs[0].instanceCount, 1u, slot);
        b_visibleInstances[slot] = gid;
    }
}
