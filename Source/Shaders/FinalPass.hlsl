#include "include/Defines.hlsli"
#include "include/Shared.hlsli"

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "include/Scene.hlsli"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

DECLARE_SAMPLER(FinalPass, SamplerLinear) SamplerState sampler_linear;

DECLARE_SRV(FinalPass, WorldsBuffer) StructuredBuffer<WorldShaderData> _worlds_buffer;
#define world_shader_data _worlds_buffer[0]

DECLARE_SRV(FinalPass, InTexture) Texture2D src_image;

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

    output.color_output = float4(1.0, 0.0, 0.0,1.0);//SAMPLE_TEXTURE_2D_LOD(sampler_linear, src_image, texcoord, 0.0);

    return output;
}

#endif // PIXEL_SHADER
