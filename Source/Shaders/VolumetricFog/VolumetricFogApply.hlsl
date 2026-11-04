#include "../Include/Defines.hlsli"

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

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../Include/Scene.hlsli"
#include "../Include/Shared.hlsli"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "../Include/Noise.hlsli"
#include "VolumetricFogCommon.hlsli"

DECLARE_SAMPLER(VolumetricFog, SamplerLinear) SamplerState SamplerLinear;

DECLARE_SRV(VolumetricFog, GBufferDepthTexture) Texture2D GBufferDepthTexture;
DECLARE_SRV(VolumetricFog, IntegratedTexture) Texture3D<float4> IntegratedTexture;

DECLARE_BUFFER_DYNAMIC(VolumetricFog, VolumetricFogApplyConstants) cbuffer VolumetricFogApplyConstants
{
    Camera camera;
    VolumetricFogGrid grid;
    uint4 frameParams; // x = frame counter
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

struct PSOutput
{
    float4 color : SV_Target0;
};

PSOutput PSMain(PSInput input)
{
    PSOutput output;

    const float2 texcoord = input.texcoord;

    uint2 depthDimensions;
    GBufferDepthTexture.GetDimensions(depthDimensions.x, depthDimensions.y);

    const float depth = GBufferDepthTexture.Load(int3(min(uint2(texcoord * float2(depthDimensions)), depthDimensions - 1u), 0)).r;

    const float3 positionWS = ReconstructWorldSpacePositionFromDepth(camera.invProjMat, camera.invViewMat, texcoord, depth).xyz;
    float linearDepth = VolumetricFogLinearDepth(camera.viewProjMat, positionWS);

    // sky pixels can reconstruct to infinity
    if (!(linearDepth < grid.depthParams.y))
    {
        linearDepth = grid.depthParams.y;
    }

    float3 uvw = VolumetricFogUVW(grid, texcoord, linearDepth);
    const uint frame = frameParams.x % 64u;

    uvw.z += (InterleavedGradientNoiseAnimated(input.position.xy, frame) - 0.5) * grid.depthParams.w;

    const float2 cellDither = float2(
        InterleavedGradientNoiseAnimated(input.position.xy + 23.0, frame),
        InterleavedGradientNoiseAnimated(input.position.yx + 61.0, frame)) - 0.5;

    uvw.xy += cellDither * 0.5 / float2(grid.dimensions.xy);

    const float4 fogSample = IntegratedTexture.SampleLevel(SamplerLinear, saturate(uvw), 0);

    output.color = float4(fogSample.rgb, fogSample.a);

    return output;
}

#endif // PIXEL_SHADER
