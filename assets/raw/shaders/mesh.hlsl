// Bindless vertex-pulling shader for DiligentCore / Vulkan.
// Vertices are 8-byte quantized: 10-10-10 UNORM pos + 16/16 half-float UV.
// LOD group supplies the local AABB used to expand position back to local space.
// SV_DrawIndex / 3 identifies the LOD group (3 draw commands per group).

struct MeshLOD     { uint indexCount; uint firstIndex; uint baseVertex; float threshold; };
struct LODGroupGPU { MeshLOD lods[3]; float3 local_aabb_min; float pad0; float3 local_aabb_max; float pad1; };
struct ScryVertex  { uint pos_packed; uint norm_uv_packed; };
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
    float3 worldPos : TEXCOORD0;
    float2 uv       : TEXCOORD1;
    float4 color    : COLOR0;
};

PSInput VSMain(uint vID : SV_VertexID, uint iID : SV_InstanceID)
{
    ScryVertex vert = b_vertices[vID];

    // Unpack 10-10-10 UNORM position
    uint  packed = vert.pos_packed;
    float px = float(packed         & 0x3FFu) / 1023.0;
    float py = float((packed >> 10) & 0x3FFu) / 1023.0;
    float pz = float((packed >> 20) & 0x3FFu) / 1023.0;

    ScryMat4 m = b_instances[iID];

    // lodGroupId is packed into c3.w by the compute culler; restore c3.w=1 for correct transform.
    uint        lodGroupId = uint(m.c3.w);
    m.c3.w = 1.0;

    // Expand to local space using baked mesh AABB
    LODGroupGPU grp = b_lodGroups[lodGroupId];
    float3 localPos = grp.local_aabb_min + float3(px, py, pz) * (grp.local_aabb_max - grp.local_aabb_min);

    // Unpack 16-bit half-float UVs
    uint   uv_packed = vert.norm_uv_packed;
    float2 uv = float2(f16tof32(uv_packed & 0xFFFFu), f16tof32(uv_packed >> 16u));

    // Column-major world transform
    float4 worldPos = m.c0 * localPos.x + m.c1 * localPos.y + m.c2 * localPos.z + m.c3;

    // Column-major viewProj transform
    float4 clipPos = vp_c0 * worldPos.x + vp_c1 * worldPos.y + vp_c2 * worldPos.z + vp_c3 * worldPos.w;

    PSInput output;
    output.position = clipPos;
    output.worldPos = worldPos.xyz;
    output.uv       = uv;
    output.color    = float4(1.0, 1.0, 1.0, 1.0);
    return output;
}

float4 PSMain(PSInput input) : SV_Target
{
    // Geometric normal from screen-space derivatives (no normal data in quantized format)
    float3 N    = normalize(cross(ddx(input.worldPos), ddy(input.worldPos)));
    float3 L    = normalize(float3(0.5, 1.0, -0.5));
    float  NdotL = saturate(dot(N, L));
    float3 col  = input.color.rgb * (0.15 + 0.85 * NdotL);
    return float4(col, 1.0);
}
