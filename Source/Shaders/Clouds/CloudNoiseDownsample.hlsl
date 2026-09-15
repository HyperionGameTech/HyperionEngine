#include "../include/Defines.hlsli"

DECLARE_SRV(Clouds, InNoise) Texture3D<float4> InNoise;
DECLARE_UAV(Clouds, OutNoise) RWTexture3D<float4> OutNoise;

DECLARE_BUFFER_DYNAMIC(Clouds, CloudNoiseDownsampleConstants) cbuffer CloudNoiseDownsampleConstants
{
    uint4 sourceDimensions;
    uint4 targetDimensions;
};

// 2x2x2 box filter. Coordinates wrap so the tiling survives into odd sized or 1 texel mips
[numthreads(4, 4, 4)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    if (any(dispatchThreadId >= targetDimensions.xyz))
    {
        return;
    }

    const uint3 sourceTexel = dispatchThreadId * 2u;

    float4 sum = float4(0.0, 0.0, 0.0, 0.0);

    for (uint z = 0; z < 2; z++)
    {
        for (uint y = 0; y < 2; y++)
        {
            for (uint x = 0; x < 2; x++)
            {
                const uint3 sampleTexel = (sourceTexel + uint3(x, y, z)) % sourceDimensions.xyz;

                sum += InNoise.Load(int4(sampleTexel, 0));
            }
        }
    }

    OutNoise[dispatchThreadId] = sum * 0.125;
}
