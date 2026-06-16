// GPU frustum culler with LOD selection and dense-pack output.
// One thread per entity. Writes sunk world matrices directly into
// b_outVisibleMatrices at slot-local offsets, no indirection map needed.

#define THREADS 64

struct ScryMat4        { float4 c0; float4 c1; float4 c2; float4 c3; };
struct ShaderAABB      { float3 aabb_min; float pad0; float3 aabb_max; float pad1; };
struct IndirectCommand { uint indexCount; uint instanceCount; uint firstIndex; uint baseVertex; uint firstInstance; };
struct MeshLOD         { uint indexCount; uint firstIndex; uint baseVertex; float threshold; };
struct LODGroupGPU     { MeshLOD lods[3]; }; // 48 bytes, stride matches CPU

StructuredBuffer<ScryMat4>           b_matrices          : register(t0);
StructuredBuffer<ShaderAABB>         b_bounds            : register(t1);
StructuredBuffer<LODGroupGPU>        b_lodGroups         : register(t2);
StructuredBuffer<uint>               b_entityLodIds      : register(t3);
RWStructuredBuffer<IndirectCommand>  b_indirectArgs      : register(u0);
RWStructuredBuffer<ScryMat4>         b_outVisibleMatrices: register(u1);

cbuffer CullParams : register(b0) {
    float4 frustumPlanes[6]; // pre-normalized (xyz=normal, w=d), 96 bytes
    float4 cameraWorldPos;   // .xyz = camera position in world space, 16 bytes
    uint4  entityData;       // .x = entityCount, 16 bytes
};

[numthreads(THREADS, 1, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID) {
    uint gid = DTid.x;
    if (gid >= entityData.x) return;

    ScryMat4   mat  = b_matrices[gid];
    ShaderAABB aabb = b_bounds[gid];

    // ── Distance cull (sphere test) ───────────────────────────────────────────
    float3 entityPos = float3(mat.c3.x, mat.c3.y, mat.c3.z);
    float  dist      = distance(cameraWorldPos.xyz, entityPos);
    if (dist > 600.0) return;

    // ── Frustum cull (OBB vs 6 planes) ───────────────────────────────────────
    float3 lc = (aabb.aabb_min + aabb.aabb_max) * 0.5;
    float3 le = (aabb.aabb_max - aabb.aabb_min) * 0.5;

    float3 wc = mat.c0.xyz * lc.x + mat.c1.xyz * lc.y + mat.c2.xyz * lc.z + mat.c3.xyz;

    float3 we;
    we.x = abs(mat.c0.x) * le.x + abs(mat.c1.x) * le.y + abs(mat.c2.x) * le.z;
    we.y = abs(mat.c0.y) * le.x + abs(mat.c1.y) * le.y + abs(mat.c2.y) * le.z;
    we.z = abs(mat.c0.z) * le.x + abs(mat.c1.z) * le.y + abs(mat.c2.z) * le.z;

    [unroll]
    for (int p = 0; p < 6; ++p) {
        float4 plane = frustumPlanes[p];
        float  r     = abs(plane.x) * we.x + abs(plane.y) * we.y + abs(plane.z) * we.z;
        float  d     = dot(float4(wc, 1.0), plane);
        if (d + r < 0.0) return; // fully outside this plane
    }

    // ── Sinking: smoothly push below ground over the last 25m before cull ────
    float sink = smoothstep(575.0, 600.0, dist) * 10.0;
    ScryMat4 sunkMat = mat;
    sunkMat.c3.y -= sink;

    // ── LOD selection ─────────────────────────────────────────────────────────
    uint lodGroupId  = b_entityLodIds[gid];
    LODGroupGPU lodGrp = b_lodGroups[lodGroupId];

    uint selectedLOD = 0;
    if      (dist > lodGrp.lods[1].threshold) selectedLOD = 2;
    else if (dist > lodGrp.lods[0].threshold) selectedLOD = 1;

    // ── Dense pack into per-slot region of b_outVisibleMatrices ──────────────
    uint mdiSlot = lodGroupId * 3 + selectedLOD;
    uint originalCount;
    InterlockedAdd(b_indirectArgs[mdiSlot].instanceCount, 1u, originalCount);
    uint writeIndex = b_indirectArgs[mdiSlot].firstInstance + originalCount;
    b_outVisibleMatrices[writeIndex] = sunkMat;
}
