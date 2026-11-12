#ifndef GLIMMER_SURFELS_LIGHT_HLSLI
#define GLIMMER_SURFELS_LIGHT_HLSLI

/// NOTE expects what GlimmerSurfelsGather.hlsli expects

#include "GlimmerSurfelsGather.hlsli"

void GlimmerAddSurfelLight(uint surfelIndex, float weight, inout GlimmerLight ioLight)
{
    const uint base = GlimmerSurfelDataIndex(surfelIndex, GLIMMER_SURFEL_DATA_LIGHT);

    [unroll]
    for (uint term = 0; term < GLIMMER_LIGHT_TERMS; term++)
    {
        const uint packedDirect = glimmerSurfelData[base + GLIMMER_SURFEL_DIRECT_UINT + term / 2u];
        const float direct = f16tof32((term & 1u) != 0u ? (packedDirect >> 16) : (packedDirect & 0xFFFFu));

        const float3 bounce = GlimmerUnpackLightPart(uint2(glimmerSurfelData[base + term * 2u], glimmerSurfelData[base + term * 2u + 1u]));

        ioLight.parts[term] += (bounce + direct) * weight;
    }

    ioLight.parts[GLIMMER_LIGHT_REST] += GlimmerUnpackLightPart(uint2(
        glimmerSurfelData[base + GLIMMER_LIGHT_REST * 2u],
        glimmerSurfelData[base + GLIMMER_LIGHT_REST * 2u + 1u])) * weight;
}

/// what the surfels around P receive facing N, like GlimmerEvaluateStoredLight, and their sun bounce for the sun now as fresh as it is,
/// per unit of the sun's irradiance; returns their coverage
float GlimmerSampleSurfelsLightAndSun(GlimmerSurfels surfels, float3 P, float3 N, out GlimmerLight outLight, out float3 outSun)
{
    outLight = GlimmerNoLight();
    outSun = (float3)0.0;

    if (surfels.info.w == 0u)
    {
        return 0.0;
    }

    const int3 firstCell = GlimmerFirstSurfelGatherCell(surfels, P);

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
                const float weight = reach * GlimmerSurfelComposedOf(surfelIndex).a;

                [branch]
                if (weight > 0.0)
                {
                    GlimmerAddSurfelLight(surfelIndex, weight, outLight);

                    float3 keyedSun;
                    float keyedFreshness;
                    bool hasKeys;
                    GLIMMER_SURFEL_KEYED_SUN(glimmerSurfelData, surfels, surfelIndex, GLIMMER_SURFEL_DATA_SUN_KEYS, keyedSun, keyedFreshness, hasKeys);

                    if (hasKeys)
                    {
                        outSun += keyedSun * (weight * keyedFreshness);
                    }
                    else
                    {
                        const uint sunBase = GlimmerSurfelDataIndex(surfelIndex, GLIMMER_SURFEL_DATA_SUN);
                        const float3 sun = GlimmerUnpackLightPart(uint2(glimmerSurfelData[sunBase], glimmerSurfelData[sunBase + 1u]));

                        outSun += sun * (weight * GlimmerSurfelSunFreshness(surfels, glimmerSurfelData[sunBase + 2u]));
                    }
                    weightSum += weight;
                }
            }

            surfelIndex = GlimmerNextSurfel(surfelIndex);
        }
    }

    if (weightSum <= 1e-5)
    {
        outLight = GlimmerNoLight();
        outSun = (float3)0.0;

        return 0.0;
    }

    outLight = GlimmerScaleLight(outLight, (float3)(1.0 / weightSum));
    outSun /= weightSum;

    return GlimmerSurfelCoverage(weightSum);
}

/// the surfels' sun bounce around P facing N for every key along the sun's arc, as fresh as each is; returns their coverage
float GlimmerSampleSurfelsSunKeys(GlimmerSurfels surfels, float3 P, float3 N, out float3 outSuns[GLIMMER_SUN_KEYS], out float outVisibility[GLIMMER_SURFEL_SUN_SLOTS])
{
    [unroll]
    for (uint clearKey = 0; clearKey < GLIMMER_SUN_KEYS; clearKey++)
    {
        outSuns[clearKey] = (float3)0.0;
    }

    [unroll]
    for (uint clearSlot = 0; clearSlot < GLIMMER_SURFEL_SUN_SLOTS; clearSlot++)
    {
        outVisibility[clearSlot] = 0.0;
    }

    if (surfels.info.w == 0u)
    {
        return 0.0;
    }

    const int3 firstCell = GlimmerFirstSurfelGatherCell(surfels, P);

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
                const float weight = reach * GlimmerSurfelComposedOf(surfelIndex).a;

                [branch]
                if (weight > 0.0)
                {
                    const uint keyBase = GlimmerSurfelDataIndex(surfelIndex, GLIMMER_SURFEL_DATA_SUN_KEYS);
                    const float keyWeight = weight * GlimmerSurfelSunKeyFreshness(
                        GlimmerSurfelSunKeyUpdates(surfels, glimmerSurfelData[GlimmerSurfelDataIndex(surfelIndex, GLIMMER_SURFEL_DATA_SUN_KEY_STATE)]), GLIMMER_SURFEL_SUN_KEY_SETTLE);

                    [unroll]
                    for (uint key = 0; key < GLIMMER_SUN_KEYS; key++)
                    {
                        outSuns[key] += GlimmerUnpackRGB9E5(glimmerSurfelData[keyBase + key]) * keyWeight;
                    }

                    const uint visibilityBase = GlimmerSurfelDataIndex(surfelIndex, GLIMMER_SURFEL_DATA_SUN_VISIBILITY);

                    [unroll]
                    for (uint slot = 0; slot < GLIMMER_SURFEL_SUN_SLOTS; slot++)
                    {
                        outVisibility[slot] += float((glimmerSurfelData[visibilityBase + slot / 4u] >> ((slot % 4u) * 8u)) & 0xFFu) * (weight / 255.0);
                    }

                    weightSum += weight;
                }
            }

            surfelIndex = GlimmerNextSurfel(surfelIndex);
        }
    }

    if (weightSum <= 1e-5)
    {
        return 0.0;
    }

    [unroll]
    for (uint key = 0; key < GLIMMER_SUN_KEYS; key++)
    {
        outSuns[key] /= weightSum;
    }

    [unroll]
    for (uint slot = 0; slot < GLIMMER_SURFEL_SUN_SLOTS; slot++)
    {
        outVisibility[slot] /= weightSum;
    }

    return GlimmerSurfelCoverage(weightSum);
}

/// what the surfels around P receive facing N, like GlimmerEvaluateStoredLight; returns their coverage
float GlimmerSampleSurfelsLight(GlimmerSurfels surfels, float3 P, float3 N, out GlimmerLight outLight)
{
    float3 sun;

    return GlimmerSampleSurfelsLightAndSun(surfels, P, N, outLight, sun);
}

/// what the foliage voxels around P receive facing N, and their sun bounce for the sun now as fresh as it is; returns their coverage
float GlimmerSampleFoliageLightAndSun(GlimmerSurfels surfels, float3 P, float3 N, out GlimmerLight outLight, out float3 outSun)
{
    outLight = GlimmerNoLight();
    outSun = (float3)0.0;

    GlimmerFoliageTaps taps;
    const float coverage = GlimmerGatherFoliage(surfels, P, taps);

    if (taps.weightSum <= 1e-5)
    {
        return 0.0;
    }

    float4 entries[GLIMMER_LIGHT_ENTRIES] = (float4[GLIMMER_LIGHT_ENTRIES])0.0;

    [unroll]
    for (uint tap = 0; tap < 8; tap++)
    {
        [branch]
        if (taps.weights[tap] > 0.0)
        {
            const float weight = taps.weights[tap] / taps.weightSum;

            for (uint entry = 0; entry < GLIMMER_LIGHT_ENTRIES; entry++)
            {
                entries[entry] += GLIMMER_LOAD_FOLIAGE_ENTRY(glimmerFoliage, taps.indices[tap], entry) * weight;
            }

            float4 keyedSun[3];
            float keyedFreshness;
            bool hasKeys;
            GLIMMER_KEYED_SUN_L1(glimmerFoliage, surfels, GlimmerFoliageIndex(taps.indices[tap], GLIMMER_FOLIAGE_SUN_KEYS), GlimmerFoliageIndex(taps.indices[tap], GLIMMER_FOLIAGE_SUN_KEY_STATE), GLIMMER_SURFEL_SUN_KEY_SETTLE, keyedSun, keyedFreshness, hasKeys);

            if (hasKeys)
            {
                outSun += GlimmerEvaluateSunBounce(keyedSun, keyedFreshness, N) * weight;
            }
            else
            {
                float4 sun[3];
                uint sunState;
                GLIMMER_LOAD_SUN_BOUNCE(glimmerFoliage, GlimmerFoliageIndex(taps.indices[tap], GLIMMER_FOLIAGE_SUN), sun, sunState);

                outSun += GlimmerEvaluateSunBounce(sun, GlimmerSurfelSunFreshness(surfels, sunState), N) * weight;
            }
        }
    }

    outLight = GlimmerEvaluateStoredLight(entries, N);

    return coverage;
}

/// what the foliage voxels around P receive facing N; returns their coverage
float GlimmerSampleFoliageLight(GlimmerSurfels surfels, float3 P, float3 N, out GlimmerLight outLight)
{
    float3 sun;

    return GlimmerSampleFoliageLightAndSun(surfels, P, N, outLight, sun);
}

#endif
