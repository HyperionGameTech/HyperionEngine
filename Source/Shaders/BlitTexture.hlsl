#include "include/defines.inc"
#include "include/shared.inc"

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "include/scene.inc"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

DECLARE_SAMPLER(RenderTextureToScreenDescriptorSet, SamplerLinear) SamplerState sampler_linear;

DECLARE_BUFFER(RenderTextureToScreenDescriptorSet, WorldsBuffer) cbuffer WorldsBuffer
{
    WorldShaderData world_shader_data;
};

DECLARE_SRV(RenderTextureToScreenDescriptorSet, InTexture) Texture2D src_image;

#ifdef VERTEX_SHADER

struct VSInput
{
    HYP_ATTRIBUTE float3 a_position : POSITION;
    HYP_ATTRIBUTE float3 a_normal : NORMAL;
    HYP_ATTRIBUTE float2 a_texcoord0 : TEXCOORD0;
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

    float2 texcoord = input.v_texcoord;

#ifdef HALFRES
    // map texcoords to previous frame's output coords
    texcoord = (texcoord * 0.5) + float2(0.5 * float((world_shader_data.frame_counter - 1u) & 1u), 0.0);
#endif

    output.color_output = SAMPLE_TEXTURE_2D(sampler_linear, src_image, texcoord);

    return output;
}

#endif // PIXEL_SHADER
