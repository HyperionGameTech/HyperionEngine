#include "../Include/Defines.hlsli"
#include "../Include/CloudNoise.hlsli"

DECLARE_UAV(VolumetricFog, OutNoise) RWTexture3D<float4> OutNoise;

DECLARE_BUFFER_DYNAMIC(VolumetricFog, VolumetricFogNoiseConstants) cbuffer VolumetricFogNoiseConstants
{
    uint dimensions;
    uint _pad0;
    uint _pad1;
    uint _pad2;
};

static const uint NoiseSeed = 19u;

[numthreads(8, 8, 4)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    if (any(dispatchThreadId >= dimensions))
    {
        return;
    }

    const float3 uvw = (float3(dispatchThreadId) + 0.5) / float(dimensions);

    const float perlin = saturate(PeriodicGradientFbm(uvw * 4.0, int3(4, 4, 4), NoiseSeed, 4u) * 0.8 + 0.5);
    const float worleyLow = PeriodicWorleyFbm(uvw * 4.0, 4, NoiseSeed + 7u);
    const float worleyHigh = PeriodicWorleyFbm(uvw * 8.0, 8, NoiseSeed + 13u);

    // wisps: Perlin billows eroded by Worley cells
    const float wisps = saturate(RemapCloudValue(perlin, worleyLow - 1.0, 1.0, 0.0, 1.0));

    const float coverage = saturate(PeriodicGradientFbm(uvw * 2.0, int3(2, 2, 2), NoiseSeed + 29u, 3u) * 0.9 + 0.5);

    OutNoise[dispatchThreadId] = float4(wisps, worleyHigh, coverage, 1.0);
}
