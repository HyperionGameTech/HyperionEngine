#include "../include/Defines.hlsli"
#include "../include/CloudNoise.hlsli"

PERMUTE(MODE, SHAPE, DETAIL)

DECLARE_UAV(Clouds, OutNoise) RWTexture3D<float4> OutNoise;

DECLARE_BUFFER_DYNAMIC(Clouds, CloudNoiseConstants) cbuffer CloudNoiseConstants
{
    uint dimensions;
    uint sliceStart;
    uint sliceCount;
    uint _pad0;
};

// the textures tile across [0, 1), so every lattice below uses its cell count across the texture as its period
static const uint NoiseSeed = 7u;

float GenerateShapePerlinWorley(float3 uvw)
{
    const float perlin = saturate(PeriodicGradientFbm(uvw * 4.0, int3(4, 4, 4), NoiseSeed, 5u) * 0.8 + 0.5);

    // folding around the midpoint turns smooth hills into rounded billows
    const float billowyPerlin = abs(perlin * 2.0 - 1.0);

    const float worley = PeriodicWorleyFbm(uvw * 4.0, 4, NoiseSeed + 11u);

    // Perlin sets the broad shape, Worley fills its low areas with cells so edges read as puffs
    return saturate(RemapCloudValue(billowyPerlin, 0.0, 1.0, worley, 1.0));
}

[numthreads(8, 8, 4)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    if (dispatchThreadId.x >= dimensions || dispatchThreadId.y >= dimensions || dispatchThreadId.z >= sliceCount)
    {
        return;
    }

    const uint3 texel = uint3(dispatchThreadId.xy, sliceStart + dispatchThreadId.z);
    const float3 uvw = (float3(texel) + 0.5) / float(dimensions);

#if defined(MODE_SHAPE)
    const float perlinWorley = GenerateShapePerlinWorley(uvw);

    const float worleyLow = PeriodicWorleyFbm(uvw * 4.0, 4, NoiseSeed + 23u);
    const float worleyMid = PeriodicWorleyFbm(uvw * 8.0, 8, NoiseSeed + 37u);
    const float worleyHigh = PeriodicWorleyFbm(uvw * 16.0, 16, NoiseSeed + 53u);

    OutNoise[texel] = float4(perlinWorley, worleyLow, worleyMid, worleyHigh);
#else
    const float worleyLow = PeriodicWorleyFbm(uvw * 2.0, 2, NoiseSeed + 67u);
    const float worleyMid = PeriodicWorleyFbm(uvw * 4.0, 4, NoiseSeed + 71u);
    const float worleyHigh = PeriodicWorleyFbm(uvw * 8.0, 8, NoiseSeed + 83u);

    OutNoise[texel] = float4(worleyLow, worleyMid, worleyHigh, 1.0);
#endif
}
