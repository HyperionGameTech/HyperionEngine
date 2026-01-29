#include "include/defines.inc"
#include "include/shared.inc"

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "include/scene.inc"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

DECLARE_BUFFER(MergeHalfResTexture, WorldsBuffer) cbuffer WorldsBuffer
{
    WorldShaderData world_shader_data;
};

DECLARE_SAMPLER(MergeHalfResTexture, SamplerNearest) SamplerState sampler_nearest;
DECLARE_SAMPLER(MergeHalfResTexture, SamplerLinear) SamplerState sampler_linear;

DECLARE_SRV(MergeHalfResTexture, InTexture) Texture2D src_image;

DECLARE_BUFFER(MergeHalfResTexture, UniformBuffer) cbuffer UniformBuffer
{
    uint2 dimensions;
};

#ifdef VERTEX_SHADER

struct VSInput
{
    HYP_ATTRIBUTE(0) float3 a_position : POSITION;
    HYP_ATTRIBUTE(1) float3 a_normal : NORMAL;
    HYP_ATTRIBUTE(2) float2 a_texcoord0 : TEXCOORD0;
};

struct VSOutput
{
    float4 position_cs : SV_POSITION;
    float3 v_position : TEXCOORD0;
    float2 v_texcoord : TEXCOORD1;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;

    float4 position = float4(input.a_position, 1.0);

    output.position_cs = position;
    output.v_position = position.xyz;
    output.v_texcoord = input.a_texcoord0;

    return output;
}

#endif // VERTEX_SHADER

#ifdef PIXEL_SHADER

struct PSInput
{
    float4 position_cs : SV_POSITION;
    float3 v_position : TEXCOORD0;
    float2 v_texcoord : TEXCOORD1;
};

struct PSOutput
{
    float4 color_output : SV_Target0;
};

PSOutput PSMain(PSInput input)
{
    PSOutput output;

    float2 texcoord_a = input.v_texcoord * float2(0.5, 1.0);
    float2 texcoord_b = texcoord_a + float2(0.5, 0.0);

    uint2 pixel_coord = (uint2)(input.v_texcoord * (float2(dimensions) - 1.0));
    uint checkerboard = (pixel_coord.x & 1u) ^ (pixel_coord.y & 1u);

    float2 texcoord = lerp(texcoord_a, texcoord_b, float(checkerboard));

    output.color_output = SAMPLE_TEXTURE_2D(sampler_nearest, src_image, texcoord);

    return output;
}

#endif // PIXEL_SHADER
