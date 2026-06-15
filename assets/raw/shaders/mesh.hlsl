// Bindless vertex-pulling shader for DiligentCore / Vulkan.
// CPU layouts:
//   ScryVertex = { float4 v0{px,py,pz,nx}, float4 v1{ny,nz,u,v} }  — 32 bytes
//   ScryMat4   = { float4 c0..c3 }                                   — 64 bytes (column-major)

struct ScryVertex { float4 v0; float4 v1; };
struct ScryMat4   { float4 c0; float4 c1; float4 c2; float4 c3; };

StructuredBuffer<ScryVertex> b_vertices  : register(t0);
StructuredBuffer<ScryMat4>   b_instances : register(t1);

// viewProj stored as four column vectors — avoids row/column-major ambiguity
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
    float3 normal   : NORMAL;
    float4 color    : COLOR0;
};

PSInput VSMain(uint vID : SV_VertexID, uint iID : SV_InstanceID)
{
    ScryVertex vert = b_vertices[vID];
    float3 position = vert.v0.xyz;
    float3 normal   = float3(vert.v0.w, vert.v1.x, vert.v1.y);

    ScryMat4 m = b_instances[iID];

    // Column-major world transform
    float4 worldPos = m.c0 * position.x + m.c1 * position.y + m.c2 * position.z + m.c3;

    // Column-major viewProj transform
    float4 clipPos  = vp_c0 * worldPos.x + vp_c1 * worldPos.y + vp_c2 * worldPos.z + vp_c3 * worldPos.w;

    PSInput output;
    output.position = clipPos;
    output.normal   = normalize(m.c0.xyz * normal.x + m.c1.xyz * normal.y + m.c2.xyz * normal.z);
    output.color    = float4(1.0, 1.0, 1.0, 1.0);
    return output;
}

float4 PSMain(PSInput input) : SV_Target
{
    float3 L    = normalize(float3(0.5, 1.0, -0.5));
    float  NdotL = saturate(dot(normalize(input.normal), L));
    float3 col  = input.color.rgb * (0.15 + 0.85 * NdotL);
    return float4(col, 1.0);
}
