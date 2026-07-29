#include "include/Defines.hlsli"
#include "include/Shared.hlsli"

PERMUTE(CHECKERBOARDED);

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "include/Scene.hlsli"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

DECLARE_SAMPLER(RenderTextureToScreenDescriptorSet, SamplerLinear) SamplerState sampler_linear;

DECLARE_SRV(RenderTextureToScreenDescriptorSet, WorldsBuffer) StructuredBuffer<WorldShaderData> _worlds_buffer;
#define world_shader_data _worlds_buffer[0]

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
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float2 texcoord : TEXCOORD0;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;

    output.position = float4(input.a_position, 1.0);
    output.normal = input.a_normal;
    output.texcoord = input.a_texcoord0;

    return output;
}

#endif // VERTEX_SHADER

#ifdef PIXEL_SHADER

struct PSInput
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float2 texcoord : TEXCOORD0;
};

struct PSOutput
{
    float4 color_output : SV_Target0;
};

PSOutput PSMain(PSInput input)
{
    PSOutput output;

    float2 texcoord = input.texcoord;

#ifdef CHECKERBOARDED
    // map texcoords to previous frame's output coords
    texcoord = (texcoord * 0.5) + float2(0.5 * float((world_shader_data.frame_counter - 1u) & 1u), 0.0);
#endif

    float4 color = SAMPLE_TEXTURE_2D_LOD(sampler_linear, src_image, texcoord, 0.0);
    //output.color_output = any(isnan(color)) || any(isinf(color)) ? (float4) 0 : color;
    output.color_output = color;
    return output;
}

#endif // PIXEL_SHADER
