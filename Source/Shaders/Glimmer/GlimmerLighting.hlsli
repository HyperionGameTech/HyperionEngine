#ifndef GLIMMER_LIGHTING_HLSLI
#define GLIMMER_LIGHTING_HLSLI

#include "GlimmerLight.hlsli"
#include "GlimmerSpans.hlsli"

#define GLIMMER_MAX_ALBEDO 0.98
#define GLIMMER_UNKNOWN_OPENNESS 0.5

float3 GlimmerSkyIrradiance(float3 N)
{
    if (skyProbe.textureIndices == ~0u)
    {
        return (float3)0.0;
    }

    float shBands[9];
    ProjectSHBands(N, shBands);

    return max(EnvProbeSH(skyProbe, shBands), (float3)0.0) * skyProbe.world_position.w * world_shader_data.sky_light_params.x;
}

float GlimmerUncoveredOpenness(float3 P, float3 N)
{
    for (uint level = 0; level < GLIMMER_GROUND_LEVELS; level++)
    {
        const float texelSize = GLIMMER_LIGHTING_SPANS.levels[level].params.x;
        const float3 samplePosition = P + N * texelSize;

        GlimmerSpanSample spanSample;

        if (!GlimmerSampleSpans(GLIMMER_LIGHTING_SPANS, level, samplePosition.xz, spanSample))
        {
            continue;
        }

        const float solidTop = spanSample.solidMin + GlimmerSpanHeight(spanSample.solidMin, spanSample.solidMax);

        if (spanSample.solidBins != 0u && solidTop > samplePosition.y)
        {
            return 0.0;
        }

        return 0.5 + 0.5 * N.y;
    }

    return GLIMMER_UNKNOWN_OPENNESS;
}

// the surfels whose sun epoch the held sun bounces are checked against; with none, no sun bounce is fresh
GlimmerSurfels GlimmerLightingSunSurfels()
{
#ifdef GLIMMER_LIGHTING_SURFELS
    return GLIMMER_LIGHTING_SURFELS;
#else
    return (GlimmerSurfels)0;
#endif
}

/// what P receives facing N, and the sun bounce what it receives from holds for the sun now, per unit of the sun's irradiance
GlimmerLight GlimmerIndirectWithSun(float3 P, float3 N, out float3 outSun)
{
    GlimmerLight light = GlimmerNoLight();
    float weight = 0.0;

    outSun = (float3)0.0;

#ifdef GLIMMER_LIGHTING_SURFELS
    GlimmerLight surfelLight;
    float3 surfelSun;
    const float surfelCoverage = GlimmerSampleSurfelsLightAndSun(GLIMMER_LIGHTING_SURFELS, P, N, surfelLight, surfelSun);

    GlimmerAddLight(light, surfelLight, (float3)surfelCoverage);
    outSun += surfelSun * surfelCoverage;
    weight = surfelCoverage;

    [branch]
    if (weight >= 0.999)
    {
        return light;
    }

    GlimmerLight foliageLight;
    float3 foliageSun;
    const float foliageWeight = GlimmerSampleFoliageLightAndSun(GLIMMER_LIGHTING_SURFELS, P, N, foliageLight, foliageSun) * (1.0 - weight);

    GlimmerAddLight(light, foliageLight, (float3)foliageWeight);
    outSun += foliageSun * foliageWeight;
    weight += foliageWeight;

    [branch]
    if (weight >= 0.999)
    {
        return light;
    }
#endif

    GlimmerLight farField;
    float3 farSun;
    const float farWeight = GlimmerSHSampleVolumeLightAndSun(GLIMMER_LIGHTING_SH, GlimmerLightingSunSurfels(), P, N, farField, farSun) * (1.0 - weight);

    GlimmerAddLight(light, farField, (float3)farWeight);
    outSun += farSun * farWeight;
    weight += farWeight;

    [branch]
    if (weight < 0.999)
    {
        GlimmerAddLight(light, GlimmerDistantLight(N, GlimmerUncoveredOpenness(P, N) * (1.0 - weight)), (float3)1.0);
    }

    return light;
}

#ifdef GLIMMER_LIGHTING_SURFELS
// the sun bounce received at P facing N for every key along the sun's arc: the surfels' in one gather; where they leave some of it to the
// foliage voxels and the SH, that share is gathered for the keys the hit tests (from shadowedStart, GLIMMER_SURFEL_SUN_KEY_STRIDE apart) and counts for those between
// returns the surfels' coverage, which their sun visibility (outVisibility) is good for
float GlimmerIndirectSunKeys(float3 P, float3 N, uint shadowedStart, out float3 outSuns[GLIMMER_SUN_KEYS], out float outVisibility[GLIMMER_SURFEL_SUN_SLOTS])
{
    const float coverage = GlimmerSampleSurfelsSunKeys(GLIMMER_LIGHTING_SURFELS, P, N, outSuns, outVisibility);

    [unroll]
    for (uint coveredKey = 0; coveredKey < GLIMMER_SUN_KEYS; coveredKey++)
    {
        outSuns[coveredKey] *= coverage;
    }

    [branch]
    if (coverage >= 0.999)
    {
        return coverage;
    }

    const uint tracedKey = glimmerSunKey;

    [loop]
    for (uint key = shadowedStart; key < GLIMMER_SUN_KEYS; key += GLIMMER_SURFEL_SUN_KEY_STRIDE)
    {
        glimmerSunKey = key;

        float3 sun;
        GlimmerIndirectWithSun(P, N, sun);

        outSuns[key] += (sun - outSuns[key]) * float(GLIMMER_SURFEL_SUN_KEY_STRIDE);
    }

    glimmerSunKey = tracedKey;

    return coverage;
}
#endif

GlimmerLight GlimmerIndirect(float3 P, float3 N)
{
    float3 sun;

    return GlimmerIndirectWithSun(P, N, sun);
}

GlimmerLight GlimmerIndirectNearestWithSun(float3 P, float3 N, out float3 outSun)
{
    GlimmerLight light;

#ifdef GLIMMER_LIGHTING_SURFELS
    if (GlimmerSampleFoliageLightAndSun(GLIMMER_LIGHTING_SURFELS, P, N, light, outSun) >= 0.5)
    {
        return light;
    }
#endif

    if (GlimmerSHSampleNearestLightAndSun(GLIMMER_LIGHTING_SH, GlimmerLightingSunSurfels(), P, N, 0.0, light, outSun))
    {
        return light;
    }

    outSun = (float3)0.0;

    return GlimmerDistantLight(N, GlimmerUncoveredOpenness(P, N));
}

GlimmerLight GlimmerIndirectNearest(float3 P, float3 N)
{
    float3 sun;

    return GlimmerIndirectNearestWithSun(P, N, sun);
}

// what a surface sends on of what it receives
GlimmerLight GlimmerShade(float3 albedo, GlimmerLight received)
{
    return GlimmerScaleLight(received, min(albedo, (float3)GLIMMER_MAX_ALBEDO));
}

GlimmerLight GlimmerShadeSurface(float3 P, float3 N, float3 albedo)
{
    return GlimmerShade(albedo, GlimmerIndirect(P, N));
}

#ifdef GLIMMER_RELIGHT_SAMPLING_HLSLI

bool GlimmerShadeRelitHit(float3 P, float3 albedo, float3 N, bool isGroundHit, uint level, out GlimmerLight outLight, out float3 outReceivedSun)
{
    outLight = GlimmerNoLight();

    GlimmerLight received;

    if (!GlimmerRelightCovers(isGroundHit, N)
        || !GlimmerSampleRelight(GLIMMER_LIGHTING_RELIGHT, GLIMMER_LIGHTING_GROUND, level, P.xz, isGroundHit ? GLIMMER_RELIGHT_GROUND : GLIMMER_RELIGHT_SPAN_TOP, received, outReceivedSun))
    {
        return false;
    }

    outLight = GlimmerShade(albedo, received);

    return true;
}

#endif

GlimmerLight GlimmerCanopyRadiance(float3 P, float3 weightedAlbedo, GlimmerLight skyTransfer)
{
    GlimmerLight received = skyTransfer;
    GlimmerAddLight(received, GlimmerIndirectNearest(P, float3(0.0, -1.0, 0.0)), (float3)1.0);

    return GlimmerScaleLight(received, weightedAlbedo * 0.5);
}

#endif
