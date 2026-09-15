#include "../include/Defines.hlsli"
#include "../include/Shared.hlsli"

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../include/Scene.hlsli"
#include "../include/EnvProbes.hlsli"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "../include/Clouds.hlsli"
#include "../include/CloudNoise.hlsli"
#include "../include/Atmosphere.hlsli"

// Composites clouds over the sky probe's capture before it's convolved, so ambient and reflections include them.
// InSkyTexture is the cloud-free capture; the faces of OutSkyProbeTexture get sky * transmittance + cloud light

DECLARE_UAV(Clouds, OutSkyProbeTexture) RWTexture2DArray<float4> OutSkyProbeTexture;

DECLARE_SRV(Clouds, InSkyTexture) TextureCube InSkyTexture;

DECLARE_SRV(Clouds, CloudWeatherMapTexture) Texture2DArray CloudWeatherMapTexture;
DECLARE_SRV(Clouds, CloudShapeNoiseTexture) Texture3D CloudShapeNoiseTexture;
DECLARE_SRV(Clouds, CloudDetailNoiseTexture) Texture3D CloudDetailNoiseTexture;

DECLARE_SAMPLER(Clouds, SamplerLinear) SamplerState SamplerLinear;

DECLARE_BUFFER_DYNAMIC(Clouds, CloudSkyProbeConstants) cbuffer CloudSkyProbeConstants
{
    Light sun;
    EnvProbe skyProbe;

    CloudVolume cloudVolume;
    CloudWeatherMap cloudWeatherMap;
    CloudShadowMap cloudShadowMap;

    // the camera the weather map is centered on; the probe itself sits at the world origin
    float4 rayOrigin;

    uint dimensions;
    uint traceSteps;
    uint lightSteps;
    uint hasSun;
};

#include "../include/CloudDensity.hlsli"
#include "../include/CloudMarch.hlsli"

[numthreads(8, 8, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    if (any(dispatchThreadId.xy >= dimensions))
    {
        return;
    }

    const uint face = dispatchThreadId.z;

    const float3 rayDirection = normalize(GetCubemapCoord(face, (float2(dispatchThreadId.xy) + 0.5) / float(dimensions)));
    const float4 sky = InSkyTexture.SampleLevel(SamplerLinear, rayDirection, 0.0);

    OutSkyProbeTexture[dispatchThreadId] = sky;

    const CloudVolumeParams params = cloudVolume.params;

    if (params.enabled == 0)
    {
        return;
    }

    const CloudLayer layer = GetCloudLayer(params, rayOrigin.xyz);
    const float2 rayRange = GetCloudLayerRayRange(layer, rayOrigin.xyz, rayDirection);

    if (rayRange.x >= rayRange.y)
    {
        return;
    }

    const CloudLighting lighting = GetCloudLighting(sun, hasSun != 0, skyProbe, params);

    // no history to average jitter over, and the prefilter blurs away the banding anyway
    const CloudMarchResult march = MarchCloudLayer(layer, lighting, rayOrigin.xyz, rayDirection, rayRange, traceSteps, lightSteps, 0.5);

    const float3 cloudLight = ApplyCloudHaze(march, rayRange.x, params.hazeDistance, sky.rgb);

    OutSkyProbeTexture[dispatchThreadId] = float4(sky.rgb * march.transmittance + cloudLight, sky.a);
}
