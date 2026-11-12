#include "../Include/Defines.hlsli"
#include "../Include/Shared.hlsli"

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../Include/Scene.hlsli"
#include "../Include/EnvProbes.hlsli"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "GlimmerCommon.hlsli"
#include "GlimmerLight.hlsli"

DECLARE_BUFFER_DYNAMIC(GlimmerSkyBins, CBuffer) cbuffer CBuffer
{
    uint4 info; // x = the sky probe's color texture index
};

DECLARE_SRV(GlimmerSkyBins, EnvProbesColorTexture) TextureCubeArray envProbesColorTexture;
DECLARE_SAMPLER(GlimmerSkyBins, SamplerLinear) SamplerState skySampler;

/// the sky's mean radiance in each light term, weighted as the term weights it; w = the term's share of the samples
DECLARE_UAV(GlimmerSkyBins, OutBins) RWStructuredBuffer<float4> OutBins;

#define GLIMMER_SKY_BINS_GROUP_SIZE 64
#define GLIMMER_SKY_BINS_SAMPLES_PER_THREAD 64

groupshared float4 gsBins[GLIMMER_SKY_BINS_GROUP_SIZE][GLIMMER_LIGHT_TERMS];

[numthreads(GLIMMER_SKY_BINS_GROUP_SIZE, 1, 1)]
void CSMain(uint groupIndex : SV_GroupIndex)
{
    const uint numSamples = GLIMMER_SKY_BINS_GROUP_SIZE * GLIMMER_SKY_BINS_SAMPLES_PER_THREAD;

    float4 sums[GLIMMER_LIGHT_TERMS];

    [unroll]
    for (uint clearTerm = 0; clearTerm < GLIMMER_LIGHT_TERMS; clearTerm++)
    {
        sums[clearTerm] = (float4)0.0;
    }

    [loop]
    for (uint sampleIndex = 0; sampleIndex < GLIMMER_SKY_BINS_SAMPLES_PER_THREAD; sampleIndex++)
    {
        const float3 direction = GlimmerSphericalFibonacci(groupIndex * GLIMMER_SKY_BINS_SAMPLES_PER_THREAD + sampleIndex, numSamples);

        float3 radiance = EnvProbeSample(skySampler, envProbesColorTexture, info.x, direction, 0.0).rgb;

        if (any(HYP_ISNAN(radiance)) || any(HYP_ISINF(radiance)))
        {
            radiance = (float3)0.0;
        }

        float basis[GLIMMER_LIGHT_TERMS];
        GlimmerLightBasis(direction, basis);

        [unroll]
        for (uint term = 0; term < GLIMMER_LIGHT_TERMS; term++)
        {
            sums[term] += float4(radiance, 1.0) * basis[term];
        }
    }

    [unroll]
    for (uint storeTerm = 0; storeTerm < GLIMMER_LIGHT_TERMS; storeTerm++)
    {
        gsBins[groupIndex][storeTerm] = sums[storeTerm];
    }

    GroupMemoryBarrierWithGroupSync();

    [unroll]
    for (uint stride = GLIMMER_SKY_BINS_GROUP_SIZE / 2; stride > 0; stride >>= 1)
    {
        if (groupIndex < stride)
        {
            [unroll]
            for (uint term = 0; term < GLIMMER_LIGHT_TERMS; term++)
            {
                gsBins[groupIndex][term] += gsBins[groupIndex + stride][term];
            }
        }

        GroupMemoryBarrierWithGroupSync();
    }

    if (groupIndex < GLIMMER_LIGHT_TERMS)
    {
        const float4 sum = gsBins[0][groupIndex];

        OutBins[groupIndex] = float4(sum.rgb / max(sum.a, 1e-6), sum.a / float(numSamples));
    }
}
