#include "../include/defines.inc"
#include "../include/shared.inc"
#include "../include/scene.inc"

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
    float3 position : POSITION;
    float2 texcoord : TEXCOORD0;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;

    float4 position = float4(input.a_position, 1.0);

    output.position = position.xyz;
    output.texcoord = input.a_texcoord0;

    output.position_cs = position;

    return output;
}

#endif // VERTEX_SHADER

#ifdef PIXEL_SHADER

struct PSInput
{
    float4 position_cs : SV_POSITION;
    float3 position : POSITION;
    float2 texcoord : TEXCOORD0;
};

struct PSOutput
{
    float4 output_color : SV_Target0;
};

DECLARE_SAMPLER(SSGIUpsample, SamplerLinear) SamplerState SamplerLinear;
DECLARE_SAMPLER(SSGIUpsample, SamplerNearest) SamplerState SamplerNearest;

DECLARE_SRV(SSGIUpsample, GBufferNormalsTexture) Texture2D GBufferNormalsTexture;
DECLARE_SRV(SSGIUpsample, GBufferDepthTexture) Texture2D GBufferDepthTexture;

DECLARE_SRV(SSGIUpsample, PrevPassTexture) Texture2D InTexture;

DECLARE_BUFFER_DYNAMIC(SSGIUpsample, CBuffer) cbuffer CBuffer
{
    Camera camera;

    float2 texelSize;
    float depthThreshold;
    float normalThreshold;
};

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../include/shared.inc"
#include "../include/scene.inc"
#include "../include/gbuffer.inc"
#include "../include/packing.inc"
#include "../include/noise.inc"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
    
static const float SpatialWeights[5] = { 0.0545f, 0.2442f, 0.4026f, 0.2442f, 0.0545f };
static const int Radius = 2;

float3 GetNormal(float2 uv) 
{
    return GBufferUnpackNormal(GBufferNormalsTexture.SampleLevel(SamplerNearest, uv, 0));
}

float GetDepth(float2 uv)
{
    return ViewDepth(GBufferDepthTexture.SampleLevel(SamplerNearest, uv, 0).r, camera.near, camera.far);
}

PSOutput PSMain(PSInput i)
{
    float centerDepth = GetDepth(i.texcoord);
    float3 centerNormal = GetNormal(i.texcoord);
    
    float4 colorSum = 0.0f;
    float weightSum = 0.0f;

    for (int x = -Radius; x <= Radius; ++x)
    {
        for (int y = -Radius; y <= Radius; ++y)
        {
            float2 offsetUV = i.texcoord + float2(x, y) * texelSize;
            
            float neighborDepth = GetDepth(offsetUV);
            float3 neighborNormal = GetNormal(offsetUV);
            
            float spatialWeight = SpatialWeights[x + Radius] * SpatialWeights[y + Radius];
            
            float depthDiff = abs(centerDepth - neighborDepth) / centerDepth;
            float depthWeight = exp(-depthDiff / depthThreshold);
            
            float normalDot = max(dot(centerNormal, neighborNormal), 0.0f);
            float normalWeight = pow(normalDot, normalThreshold); 
            
            float finalWeight = spatialWeight * depthWeight * normalWeight;
            
            float4 neighborColor = InTexture.SampleLevel(SamplerLinear, offsetUV, 0);
            
            colorSum += neighborColor * finalWeight;
            weightSum += finalWeight;
        }
    }
    
    PSOutput output;
    output.output_color = colorSum / (weightSum + 0.001f);
    return output;
}

#endif // PIXEL_SHADER
