#ifndef GLIMMER_HEIGHTFIELD_HLSLI
#define GLIMMER_HEIGHTFIELD_HLSLI

#include "GlimmerGround.hlsli"

// The ground plus the spans of solids and canopy splatted above it; expects StructuredBuffer<uint> glimmerSpans to be declared

// solids that block at least this much of a horizontal ray through their texel block rays outright; sparser ones only dim them
#define GLIMMER_SPAN_SOLID_THRESHOLD 0.4

// the fraction of leaf area that blocks light along any direction, for randomly oriented leaves
#define GLIMMER_LEAF_PROJECTION 0.5

#define GLIMMER_HEIGHTFIELD_MISS 0u
#define GLIMMER_HEIGHTFIELD_GROUND 1u
#define GLIMMER_HEIGHTFIELD_SOLID 2u

struct GlimmerSpanSample
{
    float solidMin;
    float solidMax;
    float solidity;
    float3 solidAlbedo;

    float canopyMin;
    float canopyMax;
    float leafArea;
    float3 canopyAlbedo;
};

bool GlimmerSampleSpans(GlimmerSpanParams params, uint level, float2 worldXZ, out GlimmerSpanSample outSample)
{
    outSample = (GlimmerSpanSample)0;

    const GlimmerSpanLevel spanLevel = params.levels[level];

    if (spanLevel.window.z == 0)
    {
        return false;
    }

    const int2 texel = int2(floor(worldXZ * spanLevel.params.y));

    if (any(texel < spanLevel.window.xy) || any(texel >= spanLevel.window.xy + GLIMMER_GROUND_RESOLUTION))
    {
        return false;
    }

    const uint baseIndex = GlimmerSpanTexelIndex(level, texel);

    const uint solidMin = glimmerSpans[baseIndex + GLIMMER_SPAN_SOLID_MIN];
    const uint solidMax = glimmerSpans[baseIndex + GLIMMER_SPAN_SOLID_MAX];
    const uint canopyMin = glimmerSpans[baseIndex + GLIMMER_SPAN_CANOPY_MIN];
    const uint canopyMax = glimmerSpans[baseIndex + GLIMMER_SPAN_CANOPY_MAX];

    outSample.solidMin = 1e30;
    outSample.solidMax = -1e30;
    outSample.canopyMin = 1e30;
    outSample.canopyMax = -1e30;

    if (solidMin <= solidMax)
    {
        const uint solidAreaFixed = glimmerSpans[baseIndex + GLIMMER_SPAN_SOLID_AREA];

        outSample.solidMin = GlimmerFloatFromOrderedUint(solidMin);
        outSample.solidMax = GlimmerFloatFromOrderedUint(solidMax);
        outSample.solidity = float(solidAreaFixed) / GLIMMER_SPAN_AREA_SCALE;
        outSample.solidAlbedo = float3(
            glimmerSpans[baseIndex + GLIMMER_SPAN_SOLID_ALBEDO + 0],
            glimmerSpans[baseIndex + GLIMMER_SPAN_SOLID_ALBEDO + 1],
            glimmerSpans[baseIndex + GLIMMER_SPAN_SOLID_ALBEDO + 2]) / float(max(solidAreaFixed, 1u));
    }

    if (canopyMin <= canopyMax)
    {
        const uint leafAreaFixed = glimmerSpans[baseIndex + GLIMMER_SPAN_LEAF_AREA];

        outSample.canopyMin = GlimmerFloatFromOrderedUint(canopyMin);
        outSample.canopyMax = GlimmerFloatFromOrderedUint(canopyMax);
        outSample.leafArea = float(leafAreaFixed) / GLIMMER_SPAN_AREA_SCALE;
        outSample.canopyAlbedo = float3(
            glimmerSpans[baseIndex + GLIMMER_SPAN_CANOPY_ALBEDO + 0],
            glimmerSpans[baseIndex + GLIMMER_SPAN_CANOPY_ALBEDO + 1],
            glimmerSpans[baseIndex + GLIMMER_SPAN_CANOPY_ALBEDO + 2]) / float(max(leafAreaFixed, 1u));
    }

    return true;
}

/*! How much of a horizontal ray through the texel its solid surface blocks: the surface projected onto a vertical plane over the
 *  texel's cross section. Walls and trunks come out near or above 1, the branches spread through a tree's crown far below. */
float GlimmerSpanSolidFill(GlimmerSpanSample spanSample, float texelSize)
{
    const float thickness = max(spanSample.solidMax - spanSample.solidMin, texelSize);

    return spanSample.solidity * texelSize / (3.14159265 * thickness);
}

// Length of the segment from height y0 to y1 (over segmentLength) that lies within [spanMin, spanMax]
float GlimmerSpanOverlapLength(float y0, float y1, float segmentLength, float spanMin, float spanMax)
{
    if (spanMax <= spanMin)
    {
        return 0.0;
    }

    const float low = min(y0, y1);
    const float high = max(y0, y1);

    if (high - low < 1e-4)
    {
        return (low >= spanMin && low <= spanMax) ? segmentLength : 0.0;
    }

    const float overlap = max(min(high, spanMax) - max(low, spanMin), 0.0);

    return segmentLength * overlap / (high - low);
}

struct GlimmerHeightfieldHit
{
    float t;
    uint kind;
    uint level;
    float3 normal;
    float3 albedo;
    float transmittance; // how much of what's behind (the hit, or the sky) still reaches the origin
    float3 inscatter;    // light the canopy scattered toward the origin along the way
};

/*! Marches the ray through the heightfield: the ground, solid spans (blocking, but only from solidsFromT on, for callers that traced what's
 *  before it) and canopy slabs, which dim the ray by their leaf area and scatter light into it when accumulateCanopy is set. */
bool GlimmerTraceHeightfield(
    GlimmerGroundParams groundParams,
    GlimmerSpanParams spanParams,
    float3 origin,
    float3 direction,
    float tMax,
    uint startLevel,
    float solidsFromT,
    float foliageExtinction,
    bool accumulateCanopy,
    out GlimmerHeightfieldHit hit)
{
    hit.t = tMax;
    hit.kind = GLIMMER_HEIGHTFIELD_MISS;
    hit.level = startLevel;
    hit.normal = float3(0.0, 1.0, 0.0);
    hit.albedo = (float3)0.0;
    hit.transmittance = 1.0;
    hit.inscatter = (float3)0.0;

    float groundHeight;
    uint level;

    const bool hasGround = GlimmerSampleGround(groundParams, origin.xz, startLevel, groundHeight, level);

    if (hasGround && origin.y <= groundHeight)
    {
        hit.t = 0.0;
        hit.kind = GLIMMER_HEIGHTFIELD_GROUND;
        hit.level = level;

        return true;
    }

    uint stepLevel = hasGround ? level : startLevel;
    uint stepsAtLevel = 0;

    float previousT = 0.0;
    float t = 0.0;

    [loop]
    for (uint stepIndex = 0; stepIndex < GLIMMER_GROUND_MAX_STEPS && t < tMax; stepIndex++)
    {
        t = min(t + groundParams.levels[stepLevel].params.x, tMax);

        const float3 position = origin + direction * t;
        const float3 previousPosition = origin + direction * previousT;

        GlimmerSpanSample spanSample;

        if (GlimmerSampleSpans(spanParams, stepLevel, 0.5 * (position.xz + previousPosition.xz), spanSample))
        {
            const float segmentLength = t - previousT;

            // canopy, and solids too sparse to block outright, dim the ray
            const float canopyLength = GlimmerSpanOverlapLength(previousPosition.y, position.y, segmentLength, spanSample.canopyMin, spanSample.canopyMax);

            if (canopyLength > 0.0)
            {
                const float canopyThickness = max(spanSample.canopyMax - spanSample.canopyMin, 0.5);
                const float extinction = GLIMMER_LEAF_PROJECTION * foliageExtinction * spanSample.leafArea / canopyThickness;

                const float segmentTransmittance = exp(-extinction * canopyLength);

                if (accumulateCanopy)
                {
                    const float3 midpoint = origin + direction * (0.5 * (previousT + t));
                    const float3 radiance = GlimmerCanopyRadiance(midpoint, spanSample.canopyAlbedo, max(spanSample.canopyMax - midpoint.y, 0.0), extinction);

                    hit.inscatter += hit.transmittance * (1.0 - segmentTransmittance) * radiance;
                }

                hit.transmittance *= segmentTransmittance;
            }

            if (t > solidsFromT)
            {
                const float solidLength = GlimmerSpanOverlapLength(previousPosition.y, position.y, segmentLength, spanSample.solidMin, spanSample.solidMax);

                if (solidLength > 0.0)
                {
                    const float solidFill = GlimmerSpanSolidFill(spanSample, groundParams.levels[stepLevel].params.x);

                    if (solidFill >= GLIMMER_SPAN_SOLID_THRESHOLD)
                    {
                        const bool enteredFromAbove = previousPosition.y > spanSample.solidMax;
                        const bool enteredFromBelow = previousPosition.y < spanSample.solidMin;

                        hit.t = max(previousT, solidsFromT);
                        hit.kind = GLIMMER_HEIGHTFIELD_SOLID;
                        hit.level = stepLevel;
                        hit.normal = enteredFromAbove
                            ? float3(0.0, 1.0, 0.0)
                            : (enteredFromBelow ? float3(0.0, -1.0, 0.0) : -normalize(float3(direction.x, 0.0, direction.z) + float3(1e-5, 0.0, 0.0)));
                        hit.albedo = spanSample.solidAlbedo;

                        return true;
                    }

                    // a sparse solid blocks this fraction of a ray crossing its texel, spread along the span
                    hit.transmittance *= exp(-solidFill / groundParams.levels[stepLevel].params.x * solidLength);
                }
            }

            if (hit.transmittance < 0.02)
            {
                hit.t = t;
                hit.transmittance = 0.0;

                return false;
            }
        }

        if (!GlimmerSampleGround(groundParams, position.xz, stepLevel, groundHeight, level))
        {
            // off the heightfield: nothing left to hit
            return false;
        }

        if (position.y <= groundHeight)
        {
            float lowT = previousT;
            float highT = t;

            [unroll]
            for (uint refineIndex = 0; refineIndex < 6; refineIndex++)
            {
                const float midT = 0.5 * (lowT + highT);
                const float3 midPosition = origin + direction * midT;

                float midHeight;
                uint midLevel;

                if (GlimmerSampleGround(groundParams, midPosition.xz, stepLevel, midHeight, midLevel) && midPosition.y <= midHeight)
                {
                    highT = midT;
                }
                else
                {
                    lowT = midT;
                }
            }

            hit.t = highT;
            hit.kind = GLIMMER_HEIGHTFIELD_GROUND;
            hit.level = level;
            hit.normal = GlimmerGroundNormal(groundParams, (origin + direction * highT).xz, level);

            return true;
        }

        previousT = t;

        if (direction.y > 0.0 && position.y > groundHeight + 2000.0)
        {
            return false;
        }

        if (++stepsAtLevel >= GLIMMER_GROUND_STEPS_PER_LEVEL && stepLevel + 1 < GLIMMER_GROUND_LEVELS)
        {
            stepLevel++;
            stepsAtLevel = 0;
        }
    }

    return false;
}

#endif
