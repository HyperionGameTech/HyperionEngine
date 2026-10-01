#include "../include/Defines.hlsli"

struct FogVolumeTemporalConstants
{
    uint4 params;   // xy = extent, z = 1 when the history is usable
    float4 blend;   // x = history weight
};

DECLARE_BUFFER_DYNAMIC(FogVolumeTemporal, CBuffer) cbuffer CBuffer
{
    FogVolumeTemporalConstants constants;
};

DECLARE_SRV(FogVolumeTemporal, CurrentTexture) Texture2D<float4> currentTexture;
DECLARE_SRV(FogVolumeTemporal, HistoryTexture) Texture2D<float4> historyTexture;
DECLARE_SRV(FogVolumeTemporal, VelocityTexture) Texture2D<float4> velocityTexture;
DECLARE_SAMPLER(FogVolumeTemporal, SamplerLinear) SamplerState samplerLinear;

DECLARE_UAV(FogVolumeTemporal, OutTexture) RWTexture2D<float4> outTexture;

[numthreads(8, 8, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint2 extent = constants.params.xy;
    const uint2 coord = dispatchThreadId.xy;

    if (any(coord >= extent))
    {
        return;
    }

    const float4 current = currentTexture.Load(int3(coord, 0));

    float4 neighbourMin = current;
    float4 neighbourMax = current;

    [unroll]
    for (int y = -1; y <= 1; y++)
    {
        [unroll]
        for (int x = -1; x <= 1; x++)
        {
            const int2 neighbour = clamp(int2(coord) + int2(x, y), int2(0, 0), int2(extent) - 1);
            const float4 value = currentTexture.Load(int3(neighbour, 0));

            neighbourMin = min(neighbourMin, value);
            neighbourMax = max(neighbourMax, value);
        }
    }

    const float2 uv = (float2(coord) + 0.5) / float2(extent);
    const float2 previousUv = uv - velocityTexture.SampleLevel(samplerLinear, uv, 0).xy;

    float4 result = current;

    if (constants.params.z != 0u && all(previousUv >= 0.0) && all(previousUv <= 1.0))
    {
        const float4 history = clamp(historyTexture.SampleLevel(samplerLinear, previousUv, 0), neighbourMin, neighbourMax);

        result = lerp(current, history, constants.blend.x);
    }

    outTexture[coord] = any(isnan(result)) ? current : result;
}
