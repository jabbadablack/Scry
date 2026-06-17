// Bindless vertex-pulling shader for DiligentCore / Vulkan.
// Vertices are 16-byte aligned: 16-16-16 UNORM pos, 10-10-10 normal, 16-16 half-float UV.
// LOD group supplies the local AABB used to expand position back to local space.

struct MeshLOD     { uint indexCount; uint firstIndex; uint baseVertex; float threshold; };
struct LODGroupGPU { MeshLOD lods[3]; float3 local_aabb_min; float pad0; float3 local_aabb_max; float pad1; };
struct ScryVertex  { uint pos_xy; uint pos_z_pad; uint normal_pack; uint uv_pack; };
struct ScryMat4    { float4 c0; float4 c1; float4 c2; float4 c3; };

StructuredBuffer<ScryVertex>  b_vertices : register(t0);
StructuredBuffer<ScryMat4>    b_instances: register(t1);
StructuredBuffer<LODGroupGPU> b_lodGroups: register(t2);

cbuffer DrawParams : register(b0)
{
    float4 vp_c0;
    float4 vp_c1;
    float4 vp_c2;
    float4 vp_c3;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float3 normal   : TEXCOORD0;
    float2 uv       : TEXCOORD1;
    float4 color    : COLOR0;
};

PSInput VSMain(uint vID : SV_VertexID, uint iID : SV_InstanceID)
{
    ScryVertex vert = b_vertices[vID];

    // Decode 16-bit UNORM position
    float px = float(vert.pos_xy         & 0xFFFFu) / 65535.0;
    float py = float(vert.pos_xy  >> 16u          ) / 65535.0;
    float pz = float(vert.pos_z_pad      & 0xFFFFu) / 65535.0;

    // Decode 10-10-10 UNORM normal (mapped from -1..1 to 0..1 at cook time)
    float nx = (float(vert.normal_pack         & 0x3FFu) / 1023.0) * 2.0 - 1.0;
    float ny = (float((vert.normal_pack >> 10) & 0x3FFu) / 1023.0) * 2.0 - 1.0;
    float nz = (float((vert.normal_pack >> 20) & 0x3FFu) / 1023.0) * 2.0 - 1.0;

    // Decode 16-bit half-float UVs
    float2 uv = float2(f16tof32(vert.uv_pack & 0xFFFFu), f16tof32(vert.uv_pack >> 16u));

    ScryMat4 m = b_instances[iID];

    // lodGroupId is packed into c3.w by the compute culler; restore c3.w=1 for correct transform.
    uint lodGroupId = uint(m.c3.w);
    m.c3.w = 1.0;

    // Expand to local space using baked mesh AABB
    LODGroupGPU grp = b_lodGroups[lodGroupId];
    float3 localPos = grp.local_aabb_min + float3(px, py, pz) * (grp.local_aabb_max - grp.local_aabb_min);

    // Column-major world transform
    float4 worldPos = m.c0 * localPos.x + m.c1 * localPos.y + m.c2 * localPos.z + m.c3;

    // Transform normal by upper-left 3x3 of world matrix (no non-uniform scale assumed)
    float3 worldNormal = normalize(
        m.c0.xyz * nx + m.c1.xyz * ny + m.c2.xyz * nz
    );

    // Column-major viewProj transform
    float4 clipPos = vp_c0 * worldPos.x + vp_c1 * worldPos.y + vp_c2 * worldPos.z + vp_c3 * worldPos.w;

    PSInput output;
    output.position = clipPos;
    output.normal   = worldNormal;
    output.uv       = uv;
    output.color    = float4(1.0, 1.0, 1.0, 1.0);
    return output;
}

float4 PSMain(PSInput input) : SV_Target
{
    float3 N     = normalize(input.normal);
    float3 L     = normalize(float3(0.5, 1.0, -0.5));
    float  NdotL = saturate(dot(N, L));
    float3 col   = input.color.rgb * (0.15 + 0.85 * NdotL);
    return float4(col, 1.0);
}
