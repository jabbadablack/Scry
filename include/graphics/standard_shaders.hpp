#ifndef ENGINE_GRAPHICS_STANDARD_SHADERS_HPP
#define ENGINE_GRAPHICS_STANDARD_SHADERS_HPP

namespace engine::graphics::shaders {

    inline const char* UberPBR_VS = R"(
cbuffer PushConstants {
    float4x4 g_Transform;
    uint     g_TextureIndex;
    float3   g_Pad;
};

struct VSInput {
    float3 Pos    : ATTRIB0; // Vertex Buffer
    float3 Normal : ATTRIB1;
    float2 UV     : ATTRIB2;
    
    // Instancing Support (if bound to slot 1)
    float4x4 InstanceTransform : ATTRIB3; 
    uint     InstanceTexIndex  : ATTRIB7;
};

struct PSInput {
    float4 Pos      : SV_POSITION;
    float3 WorldPos : POSITION;
    float3 Normal   : NORMAL;
    float2 UV       : TEXCOORD;
    uint   TexIndex : TEXINDEX;
};

void main(in VSInput VSIn, out PSInput PSIn, uint InstanceID : SV_InstanceID) {
    // Determine if using fallback Push Constants or Hardware Instancing
    float4x4 finalTransform = (InstanceID > 0) ? VSIn.InstanceTransform : g_Transform;
    uint     finalTexIndex  = (InstanceID > 0) ? VSIn.InstanceTexIndex : g_TextureIndex;

    float4 worldPos = mul(float4(VSIn.Pos, 1.0), finalTransform);
    
    // Basic projection (In a real engine, pass ViewProj via another constant buffer)
    PSIn.Pos      = worldPos; 
    PSIn.WorldPos = worldPos.xyz;
    PSIn.Normal   = normalize(mul(VSIn.Normal, (float3x3)finalTransform));
    PSIn.UV       = VSIn.UV;
    PSIn.TexIndex = finalTexIndex;
}
)";

    inline const char* UberPBR_PS = R"(
Texture2D    g_Textures[1024];
SamplerState g_Textures_sampler;

struct PSInput {
    float4 Pos      : SV_POSITION;
    float3 WorldPos : POSITION;
    float3 Normal   : NORMAL;
    float2 UV       : TEXCOORD;
    uint   TexIndex : TEXINDEX;
};

struct PSOutput {
    float4 Color : SV_TARGET;
};

void main(in PSInput PSIn, out PSOutput PSOut) {
    // Bindless texture lookup
    float4 albedo = g_Textures[NonUniformResourceIndex(PSIn.TexIndex)].Sample(g_Textures_sampler, PSIn.UV);
    
    // Simple Directional Lighting (Uber Base)
    float3 lightDir = normalize(float3(0.5, -1.0, 0.5));
    float  nDotL    = max(dot(PSIn.Normal, -lightDir), 0.1);
    
    PSOut.Color = float4(albedo.rgb * nDotL, albedo.a);
}
)";

} // namespace engine::graphics::shaders

#endif // ENGINE_GRAPHICS_STANDARD_SHADERS_HPP
