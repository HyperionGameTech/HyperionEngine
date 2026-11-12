#ifndef GLIMMER_SURFELS_APPLY_HLSLI
#define GLIMMER_SURFELS_APPLY_HLSLI

#include "GlimmerSurfelTypes.hlsli"

DECLARE_SRV(DeferredPass, GlimmerSurfelsBuffer) StructuredBuffer<uint4> glimmerSurfels;
DECLARE_SRV(DeferredPass, GlimmerSurfelHashBuffer) StructuredBuffer<uint> glimmerSurfelHash;
DECLARE_SRV(DeferredPass, GlimmerSurfelDataBuffer) StructuredBuffer<uint> glimmerSurfelData;
DECLARE_SRV(DeferredPass, GlimmerFoliageBuffer) StructuredBuffer<uint> glimmerFoliage;

#include "GlimmerSurfelsGather.hlsli"

// rgb = irradiance / pi, a = coverage
float4 EvaluateGlimmerSurfels(GlimmerSurfels surfels, float3 P, float3 N)
{
    if (surfels.info.w == 0u)
    {
        return (float4)0.0;
    }

    const int3 firstCell = GlimmerFirstSurfelGatherCell(surfels, P);

    float3 irradianceSum = (float3)0.0;
    float weightSum = 0.0;

    [loop]
    for (uint corner = 0; corner < 8; corner++)
    {
        uint surfelIndex = GlimmerFindSurfelCell(GlimmerSurfelGatherCell(firstCell, corner));

        [loop]
        for (uint step = 0; step < GLIMMER_SURFEL_MAX_PER_CELL && surfelIndex != GLIMMER_SURFEL_NONE; step++)
        {
            const float reach = GlimmerSurfelReach(surfels, glimmerSurfels[surfelIndex], P, N);

            [branch]
            if (reach > 0.0)
            {
                const float4 composed = GlimmerSurfelComposedOf(surfelIndex);
                const float weight = reach * composed.a;

                irradianceSum += composed.rgb * weight;
                weightSum += weight;
            }

            surfelIndex = GlimmerNextSurfel(surfelIndex);
        }
    }

    if (weightSum <= 1e-5)
    {
        return (float4)0.0;
    }

    return float4(irradianceSum / weightSum, GlimmerSurfelCoverage(weightSum));
}

#endif
