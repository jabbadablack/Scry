// debug_line.hlsl
// Unlit line renderer. ViewProj is uploaded in cglm column-major layout;
// HLSL sees it as the transpose, so mul(v, M) gives the correct clip-space result.

cbuffer DrawParams : register(b0)
{
    float4x4 g_ViewProj;
};

struct VSIn
{
    float3 Pos   : ATTRIB0;
    float4 Color : ATTRIB1;
};

struct PSIn
{
    float4 Pos   : SV_POSITION;
    float4 Color : COLOR0;
};

PSIn VSMain(VSIn v)
{
    PSIn o;
    o.Pos   = mul(float4(v.Pos, 1.0), g_ViewProj);
    o.Color = v.Color;
    return o;
}

float4 PSMain(PSIn p) : SV_TARGET
{
    return p.Color;
}
