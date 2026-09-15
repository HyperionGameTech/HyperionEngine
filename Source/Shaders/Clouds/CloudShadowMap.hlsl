#include "../include/Defines.hlsli"
#include "../include/Clouds.hlsli"
#include "../include/CloudNoise.hlsli"

// Top-down map of how much sunlight gets through the clouds, for points on the y = 0 plane. Marched through the same
// density as the sky, so ground shadows match the clouds above. Filled a band of rows at a time

DECLARE_UAV(Clouds, OutShadowMap) RWTexture2D<float> OutShadowMap;

DECLARE_SRV(Clouds, CloudWeatherMapTexture) Texture2DArray CloudWeatherMapTexture;
DECLARE_SRV(Clouds, CloudShapeNoiseTexture) Texture3D CloudShapeNoiseTexture;
DECLARE_SRV(Clouds, CloudDetailNoiseTexture) Texture3D CloudDetailNoiseTexture;

DECLARE_SAMPLER(Clouds, SamplerLinear) SamplerState SamplerLinear;

DECLARE_BUFFER_DYNAMIC(Clouds, CloudShadowMapConstants) cbuffer CloudShadowMapConstants
{
    CloudVolume cloudVolume;
    CloudWeatherMap cloudWeatherMap;
    CloudShadowMap cloudShadowMap;
};

#include "../include/CloudDensity.hlsli"

static const float ShadowMarchStepLength = 150.0;
static const uint ShadowMarchMinSteps = 8;
static const uint ShadowMarchMaxSteps = 32;

// a low sun crosses tens of kilometers of layer; past this the light is gone anyway
static const float ShadowMarchMaxPathLength = 12000.0;

[numthreads(8, 8, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint2 pixel = uint2(dispatchThreadId.x, cloudShadowMap.generateRowStart + dispatchThreadId.y);

    if (dispatchThreadId.x >= cloudShadowMap.dimensions
        || dispatchThreadId.y >= cloudShadowMap.generateRowCount
        || pixel.y >= cloudShadowMap.dimensions)
    {
        return;
    }

    const CloudVolumeParams params = cloudVolume.params;
    const float3 directionToSun = cloudShadowMap.directionToSun.xyz;

    if (params.enabled == 0 || directionToSun.y <= 0.01)
    {
        OutShadowMap[pixel] = 1.0;

        return;
    }

    const float2 uv = (float2(pixel) + 0.5) / float(cloudShadowMap.dimensions);
    const float3 groundPosition = float3(cloudShadowMap.origin + uv * cloudShadowMap.worldExtent, 0.0).xzy;

    const float layerBase = params.baseAltitude;
    const float layerTop = params.baseAltitude + params.layerThickness;

    const float distanceToLayerBase = layerBase / directionToSun.y;
    const float pathLength = min((layerTop - layerBase) / directionToSun.y, ShadowMarchMaxPathLength);

    const uint numSteps = clamp(uint(ceil(pathLength / ShadowMarchStepLength)), ShadowMarchMinSteps, ShadowMarchMaxSteps);
    const float stepLength = pathLength / float(numSteps);

    float opticalDepth = 0.0;

    for (uint i = 0; i < numSteps; i++)
    {
        const float3 samplePosition = groundPosition + directionToSun * (distanceToLayerBase + (float(i) + 0.5) * stepLength);
        const float heightFraction = (samplePosition.y - layerBase) / params.layerThickness;

        opticalDepth += SampleCloudDensity(samplePosition, heightFraction, false) * CloudExtinction * stepLength;
    }

    OutShadowMap[pixel] = exp(-opticalDepth);
}
