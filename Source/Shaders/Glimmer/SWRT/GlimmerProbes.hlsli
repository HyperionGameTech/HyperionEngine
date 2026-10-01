#ifndef GLIMMER_PROBES_HLSLI
#define GLIMMER_PROBES_HLSLI

#include "GlimmerProbeTypes.hlsli"

// slot: xyz = the block's absolute coordinate, w = its level (-1 for a free slot)
float3 GlimmerProbePosition(GlimmerProbeVolume volume, uint probeIndex, int4 slot, uint4 state)
{
    const GlimmerProbeLevel level = volume.levels[slot.w];
    const int3 probeCoord = GlimmerProbeCoord(slot.xyz, GlimmerLocalProbeOf(probeIndex));

    return GlimmerProbeGridPosition(level, probeCoord) + GlimmerUnpackProbeOffset(state.y) * level.params.x;
}

#ifndef GLIMMER_PROBES_NO_SAMPLING

// expects these declared: StructuredBuffer<uint> glimmerProbeBlockTable, StructuredBuffer<float4> glimmerProbeSH (3 per probe),
// StructuredBuffer<uint4> glimmerProbeStates, StructuredBuffer<uint> glimmerProbeVisibility (GLIMMER_PROBE_VISIBILITY_TEXELS per probe, GlimmerPackHalf2)

// Chebyshev bound on how likely the probe sees a point this far (in spacings) toward it, from the depth moments of its rays that way
float GlimmerProbeVisibility(uint probeIndex, float3 probeToPoint, float distanceInSpacings)
{
    if (distanceInSpacings < 1e-4)
    {
        return 1.0;
    }

    // bilinear between the four texels around the direction (clamped rather than wrapped at the octahedron's edges)
    const float2 texelCoord = GlimmerOctahedralEncode(probeToPoint / distanceInSpacings) * float(GLIMMER_PROBE_VISIBILITY_RES) - 0.5;
    const int2 texel0 = int2(floor(texelCoord));
    const float2 fraction = texelCoord - float2(texel0);

    float2 moments = (float2)0.0;

    [unroll]
    for (uint corner = 0; corner < 4; corner++)
    {
        const int2 offset = int2(corner & 1u, corner >> 1);
        const int2 texel = clamp(texel0 + offset, 0, GLIMMER_PROBE_VISIBILITY_RES - 1);
        const float2 bilinear = lerp(1.0 - fraction, fraction, float2(offset));

        moments += GlimmerUnpackHalf2(glimmerProbeVisibility[probeIndex * GLIMMER_PROBE_VISIBILITY_TEXELS + uint(texel.y * GLIMMER_PROBE_VISIBILITY_RES + texel.x)]) * (bilinear.x * bilinear.y);
    }

    if (distanceInSpacings <= moments.x)
    {
        return 1.0;
    }

    const float variance = max(moments.y - moments.x * moments.x, 1e-3);
    const float difference = distanceInSpacings - moments.x;
    const float chebyshev = variance / (variance + difference * difference);

    // sharpened, as DDGI does, so a mostly blocked direction still rejects
    return max(chebyshev * chebyshev * chebyshev, 0.0);
}

/*! One level's irradiance / pi at P, from the probes of its allocated blocks around P that are active, weighted toward those that can see P.
 *  \return how much this level covers P: fades out where the corner probes are missing, where none of them can see P, and toward the
 *  window's edge */
float GlimmerSampleProbeLevel(GlimmerProbeVolume volume, uint levelIndex, float3 P, float3 N, out float3 outIrradiance)
{
    outIrradiance = (float3)0.0;

    const GlimmerProbeLevel level = volume.levels[levelIndex];

    if (level.windowOrigin.w == 0)
    {
        return 0.0;
    }

    const float spacing = level.params.x;

    const float3 gridCoord = P * level.params.y - 0.5;
    const int3 probeCoord0 = int3(floor(gridCoord));
    const float3 fraction = gridCoord - float3(probeCoord0);

    // fade over the window's outer block, so the next level takes over smoothly as the window scrolls
    const float3 windowCoord = gridCoord + 0.5 - float3(level.windowOrigin.xyz * GLIMMER_PROBE_BLOCK);
    const float3 edgeDistance = min(windowCoord, float(GLIMMER_PROBE_WINDOW * GLIMMER_PROBE_BLOCK) - windowCoord);
    const float windowFade = saturate((min(edgeDistance.x, min(edgeDistance.y, edgeDistance.z)) - 1.0) / float(GLIMMER_PROBE_BLOCK));

    if (windowFade <= 0.0)
    {
        return 0.0;
    }

    // the point tested for visibility sits a little off the surface, so rays that end on the surface itself don't count against it
    const float3 visibilityPoint = P + N * (0.2 * spacing);

    float4 sumR = (float4)0.0;
    float4 sumG = (float4)0.0;
    float4 sumB = (float4)0.0;
    float weightSum = 0.0;

    // the trilinear weight of the corners with a probe, and how well the best of them sees P. Corners that can't see P only lose weight
    // to the ones that can: a surface through the cell always has about half of them behind it
    float presentCoverage = 0.0;
    float bestVisibility = 0.0;

    // corners whose probe is under the ground or inside a solid are expected to be missing for a point on the surface: they don't
    // count against coverage, the rest are renormalized. Corners without a traced probe (no block, idle in open air, not updated) do
    float excludedTrilinear = 0.0;

    [unroll]
    for (uint corner = 0; corner < 8; corner++)
    {
        const int3 offset = int3(corner & 1u, (corner >> 1) & 1u, (corner >> 2) & 1u);
        const int3 probeCoord = probeCoord0 + offset;
        const int3 block = probeCoord >> 2;

        if (!GlimmerIsBlockInWindow(level, block))
        {
            continue;
        }

        const uint slot = glimmerProbeBlockTable[GlimmerProbeBlockTableIndex(levelIndex, block)];

        if (slot == GLIMMER_PROBE_NO_SLOT)
        {
            continue;
        }

        const uint probeIndex = GlimmerProbeIndex(slot, probeCoord & (GLIMMER_PROBE_BLOCK - 1));
        const uint4 state = glimmerProbeStates[probeIndex];
        const uint probeState = GlimmerProbeStateOf(state);

        const float3 trilinear = lerp(1.0 - fraction, fraction, float3(offset));
        const float trilinearWeight = trilinear.x * trilinear.y * trilinear.z;

        if (probeState == GLIMMER_PROBE_STATE_BURIED || probeState == GLIMMER_PROBE_STATE_INSIDE)
        {
            excludedTrilinear += trilinearWeight;

            continue;
        }

        if (probeState != GLIMMER_PROBE_STATE_ACTIVE || GlimmerProbeUpdates(state) == 0u)
        {
            continue;
        }

        const float3 probePosition = GlimmerProbeGridPosition(level, probeCoord) + GlimmerUnpackProbeOffset(state.y) * spacing;

        const float3 toProbe = probePosition - P;
        const float toProbeLength = length(toProbe);

        // probes behind the surface see what's behind it
        const float backface = toProbeLength > 1e-4 ? (dot(toProbe / toProbeLength, N) + 1.0) * 0.5 : 1.0;

        const float3 probeToPoint = visibilityPoint - probePosition;
        const float visibility = lerp(1.0, GlimmerProbeVisibility(probeIndex, probeToPoint, length(probeToPoint) / spacing), volume.params.y);

        const float weight = trilinearWeight * (backface * backface + 0.2) * visibility;

        presentCoverage += trilinearWeight;
        bestVisibility = max(bestVisibility, visibility);

        sumR += glimmerProbeSH[probeIndex * 3u + 0u] * weight;
        sumG += glimmerProbeSH[probeIndex * 3u + 1u] * weight;
        sumB += glimmerProbeSH[probeIndex * 3u + 2u] * weight;
        weightSum += weight;
    }

    if (weightSum <= 1e-5)
    {
        return 0.0;
    }

    outIrradiance = GlimmerEvaluateL1(sumR / weightSum, sumG / weightSum, sumB / weightSum, N);

    const float coverage = presentCoverage / max(1.0 - excludedTrilinear, 1e-3);

    return smoothstep(0.3, 0.7, coverage) * smoothstep(0.02, 0.2, bestVisibility) * windowFade;
}

/*! Irradiance / pi from the probe blocks at P, finest level first, with how much they cover P as .a */
float4 SampleGlimmerProbes(GlimmerProbeVolume volume, float3 P, float3 N)
{
    if (volume.info.w == 0u)
    {
        return (float4)0.0;
    }

    float3 irradiance = (float3)0.0;
    float remaining = 1.0;

    [loop]
    for (uint levelIndex = 0; levelIndex < volume.info.x && remaining > 1e-3; levelIndex++)
    {
        float3 levelIrradiance;
        const float levelWeight = GlimmerSampleProbeLevel(volume, levelIndex, P, N, levelIrradiance);

        if (levelWeight <= 0.0)
        {
            continue;
        }

        irradiance += levelIrradiance * levelWeight * remaining;
        remaining *= 1.0 - levelWeight;
    }

    const float coverage = 1.0 - remaining;

    if (coverage <= 1e-4)
    {
        return (float4)0.0;
    }

    return float4(irradiance / coverage, coverage);
}

#endif // GLIMMER_PROBES_NO_SAMPLING

#endif
