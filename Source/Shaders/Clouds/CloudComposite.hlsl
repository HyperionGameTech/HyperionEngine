#include "../include/Defines.hlsli"

DECLARE_SAMPLER(Clouds, SamplerLinear) SamplerState SamplerLinear;

// rgb = cloud light, premultiplied; a = transmittance of the sky behind
DECLARE_SRV(Clouds, InCloudTexture) Texture2D InCloudTexture;

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
    float2 texcoord : TEXCOORD0;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;

    output.position = float4(input.a_position, 1.0);
    output.texcoord = input.a_texcoord0;

    return output;
}

#endif // VERTEX_SHADER

#ifdef PIXEL_SHADER

struct PSInput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

struct PSOutput
{
    float4 color : SV_Target0;
};

// blended as color = src.rgb + dst.rgb * src.a, keeping the destination alpha
PSOutput PSMain(PSInput input)
{
    PSOutput output;

    output.color = SAMPLE_TEXTURE_2D_LOD(SamplerLinear, InCloudTexture, input.texcoord, 0.0);

    return output;
}

#endif // PIXEL_SHADER
