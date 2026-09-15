#include "../include/Defines.hlsli"

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../include/Scene.hlsli"
#include "../include/EnvProbes.hlsli"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "../include/Clouds.hlsli"
#include "../include/CloudNoise.hlsli"
#include "../include/Atmosphere.hlsli"

DECLARE_UAV(Clouds, OutCloudTexture) RWTexture2D<float4> OutCloudTexture;
DECLARE_UAV(Clouds, OutCloudDistanceTexture) RWTexture2D<float> OutCloudDistanceTexture;

DECLARE_SRV(Clouds, CloudWeatherMapTexture) Texture2DArray CloudWeatherMapTexture;
DECLARE_SRV(Clouds, CloudShapeNoiseTexture) Texture3D CloudShapeNoiseTexture;
DECLARE_SRV(Clouds, CloudDetailNoiseTexture) Texture3D CloudDetailNoiseTexture;

// Hi-Z mip matching the trace resolution: r = farthest depth in the block
DECLARE_SRV(Clouds, DepthPyramidTexture) Texture2D DepthPyramidTexture;

// the sky probe's capture before clouds were composited into it, for haze
DECLARE_SRV(Clouds, SkyTexture) TextureCube SkyTexture;

DECLARE_SRV(Clouds, BlueNoiseBuffer) StructuredBuffer<int4> BlueNoiseBuffer;

DECLARE_SAMPLER(Clouds, SamplerLinear) SamplerState SamplerLinear;

#include "../include/BlueNoise.hlsli"

DECLARE_BUFFER_DYNAMIC(Clouds, CloudTraceConstants) cbuffer CloudTraceConstants
{
    Camera camera;
    Light sun;
    EnvProbe skyProbe;

    CloudVolume cloudVolume;
    CloudWeatherMap cloudWeatherMap;
    CloudShadowMap cloudShadowMap;

    uint2 traceDimensions;
    uint traceSteps;
    uint lightSteps;

    uint frameCounter;
    uint hasSun;

    // which pixel of each 2x2 half resolution block this frame traces
    uint2 traceOffset;

    uint2 historyDimensions;
    uint _pad0;
    uint _pad1;
};

#include "../include/CloudDensity.hlsli"
#include "../include/CloudMarch.hlsli"

[numthreads(8, 8, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint2 pixel = dispatchThreadId.xy;

    if (any(pixel >= traceDimensions))
    {
        return;
    }

    OutCloudTexture[pixel] = float4(0.0, 0.0, 0.0, 1.0);
    OutCloudDistanceTexture[pixel] = CloudMaxTraceDistance;

    const CloudVolumeParams params = cloudVolume.params;

    if (params.enabled == 0)
    {
        return;
    }

    const uint2 historyPixel = min(pixel * 2u + traceOffset, historyDimensions - 1u);
    const float2 uv = (float2(historyPixel) + 0.5) / float2(historyDimensions);

    uint depthPyramidWidth;
    uint depthPyramidHeight;
    DepthPyramidTexture.GetDimensions(depthPyramidWidth, depthPyramidHeight);

    const int2 depthPyramidTexel = min(int2(uv * float2(depthPyramidWidth, depthPyramidHeight)), int2(depthPyramidWidth, depthPyramidHeight) - 1);

    // the whole block is covered by geometry
    if (DepthPyramidTexture.Load(int3(depthPyramidTexel, 0)).r < 0.9999)
    {
        return;
    }

    float4 farPointView = mul(camera.invProjMat, float4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, 1.0, 1.0));
    farPointView /= farPointView.w;

    const float3 rayDirection = normalize(mul(camera.invViewMat, float4(farPointView.xyz, 0.0)).xyz);
    const float3 rayOrigin = camera.position.xyz;

    const CloudLayer layer = GetCloudLayer(params, rayOrigin);
    const float2 rayRange = GetCloudLayerRayRange(layer, rayOrigin, rayDirection);

    if (rayRange.x >= rayRange.y)
    {
        return;
    }

    const CloudLighting lighting = GetCloudLighting(sun, hasSun != 0, skyProbe, params);

    // varies per frame so the reconstruction averages the banding away
    const float jitter = SampleBlueNoise(int(historyPixel.x), int(historyPixel.y), int(frameCounter % 64u), 0);

    const CloudMarchResult march = MarchCloudLayer(layer, lighting, rayOrigin, rayDirection, rayRange, traceSteps, lightSteps, jitter);

    const float3 skyColor = SkyTexture.SampleLevel(SamplerLinear, rayDirection, 0.0).rgb;

    OutCloudTexture[pixel] = float4(ApplyCloudHaze(march, rayRange.x, params.hazeDistance, skyColor), march.transmittance);
    OutCloudDistanceTexture[pixel] = march.distance;
}
