#ifndef GLIMMER_HEIGHTFIELD_HLSLI
#define GLIMMER_HEIGHTFIELD_HLSLI

#include "GlimmerGround.hlsli"

#define GLIMMER_SPAN_SOLID_THRESHOLD 0.4

#define GLIMMER_LEAF_PROJECTION 0.5

#define GLIMMER_HEIGHTFIELD_MISS 0u
#define GLIMMER_HEIGHTFIELD_GROUND 1u
#define GLIMMER_HEIGHTFIELD_SOLID 2u

struct GlimmerSpanSample
{
    float solidMin;
    float solidMax;
    uint solidBins;
    uint4 solidBands;
    float3 solidAlbedo;

    float canopyMin;
    float canopyMax;
    uint canopyBins;
    uint4 canopyBands;
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
        outSample.solidBins = glimmerSpans[baseIndex + GLIMMER_SPAN_SOLID_BINS];
        outSample.solidBands = uint4(
            glimmerSpans[baseIndex + GLIMMER_SPAN_SOLID_BANDS + 0],
            glimmerSpans[baseIndex + GLIMMER_SPAN_SOLID_BANDS + 1],
            glimmerSpans[baseIndex + GLIMMER_SPAN_SOLID_BANDS + 2],
            glimmerSpans[baseIndex + GLIMMER_SPAN_SOLID_BANDS + 3]);
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
        outSample.canopyBins = glimmerSpans[baseIndex + GLIMMER_SPAN_CANOPY_BINS];
        outSample.canopyBands = uint4(
            glimmerSpans[baseIndex + GLIMMER_SPAN_CANOPY_BANDS + 0],
            glimmerSpans[baseIndex + GLIMMER_SPAN_CANOPY_BANDS + 1],
            glimmerSpans[baseIndex + GLIMMER_SPAN_CANOPY_BANDS + 2],
            glimmerSpans[baseIndex + GLIMMER_SPAN_CANOPY_BANDS + 3]);
        outSample.canopyAlbedo = float3(
            glimmerSpans[baseIndex + GLIMMER_SPAN_CANOPY_ALBEDO + 0],
            glimmerSpans[baseIndex + GLIMMER_SPAN_CANOPY_ALBEDO + 1],
            glimmerSpans[baseIndex + GLIMMER_SPAN_CANOPY_ALBEDO + 2]) / float(max(leafAreaFixed, 1u));
    }

    return true;
}

bool GlimmerSpanOccupies(float y, float margin, float spanMin, float spanMax, uint bins)
{
    if (spanMax < spanMin || y < spanMin - margin || y > spanMin + GlimmerSpanHeight(spanMin, spanMax) + margin)
    {
        return false;
    }

    return (bins & GlimmerSpanBinsBetween(y - margin, y + margin, spanMin, spanMax)) != 0u;
}

float GlimmerSpanBandDensity(float spanMin, float spanMax, uint bins, uint4 bands, uint band, float densityScale, float minThickness)
{
    const float occupiedHeight = float(countbits(bins & GlimmerSpanBandMask(band))) * GlimmerSpanHeight(spanMin, spanMax) / float(GLIMMER_SPAN_BINS);

    return GlimmerSpanBandArea(bands, band) * densityScale / max(occupiedHeight, minThickness);
}

float GlimmerSpanSolidFillAt(GlimmerSpanSample spanSample, float y, float texelSize)
{
    if (!GlimmerSpanOccupies(y, 0.0, spanSample.solidMin, spanSample.solidMax, spanSample.solidBins))
    {
        return 0.0;
    }

    const uint band = uint(GlimmerSpanBin(y, spanSample.solidMin, spanSample.solidMax)) / GLIMMER_SPAN_BINS_PER_BAND;

    return texelSize * GlimmerSpanBandDensity(spanSample.solidMin, spanSample.solidMax, spanSample.solidBins, spanSample.solidBands, band, HYP_FMATH_ONE_OVER_PI, texelSize);
}

struct GlimmerSpanCrossing
{
    float length;       // length of the segment inside occupied bins
    float opticalDepth; // sum of density * length over the bands it crosses
    uint hitBins;       // the occupied bins it crosses
    uint denseBins;     // of those, the ones in bands at or past the dense threshold
};

GlimmerSpanCrossing GlimmerCrossSpan(
    float y0,
    float y1,
    float segmentLength,
    float spanMin,
    float spanMax,
    uint bins,
    uint4 bands,
    float densityScale,
    float minThickness,
    float denseThreshold)
{
    GlimmerSpanCrossing crossing = (GlimmerSpanCrossing)0;

    if (spanMax < spanMin || bins == 0u)
    {
        return crossing;
    }

    const float spanHeight = GlimmerSpanHeight(spanMin, spanMax);
    const float binHeight = spanHeight / float(GLIMMER_SPAN_BINS);

    const float low = min(y0, y1);
    const float high = max(y0, y1);

    if (high < spanMin || low > spanMin + spanHeight)
    {
        return crossing;
    }

    crossing.hitBins = bins & GlimmerSpanBinsBetween(low, high, spanMin, spanMax);

    if (crossing.hitBins == 0u)
    {
        return crossing;
    }

    const bool isLevel = high - low < 1e-4;

    [unroll]
    for (uint band = 0; band < GLIMMER_SPAN_BANDS; band++)
    {
        const uint bandHitBins = crossing.hitBins & GlimmerSpanBandMask(band);

        if (bandHitBins == 0u)
        {
            continue;
        }

        const float density = GlimmerSpanBandDensity(spanMin, spanMax, bins, bands, band, densityScale, minThickness);
        const float bandLength = isLevel ? segmentLength : segmentLength * saturate(float(countbits(bandHitBins)) * binHeight / (high - low));

        crossing.length += bandLength;
        crossing.opticalDepth += density * bandLength;

        if (density >= denseThreshold)
        {
            crossing.denseBins |= bandHitBins;
        }
    }

    crossing.length = min(crossing.length, segmentLength);

    return crossing;
}

float GlimmerSpanEntry(float y0, float y1, float spanMin, float spanMax, uint hitBins, out int outSide)
{
    const float binHeight = GlimmerSpanHeight(spanMin, spanMax) / float(GLIMMER_SPAN_BINS);

    outSide = 0;

    if (y1 < y0)
    {
        const float top = spanMin + float(firstbithigh(hitBins) + 1u) * binHeight;

        if (y0 > top)
        {
            outSide = 1;

            return saturate((y0 - top) / (y0 - y1));
        }
    }
    else if (y1 > y0)
    {
        const float bottom = spanMin + float(firstbitlow(hitBins)) * binHeight;

        if (y0 < bottom)
        {
            outSide = -1;

            return saturate((bottom - y0) / (y1 - y0));
        }
    }

    return 0.0;
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

#define GLIMMER_HEIGHTFIELD_MAX_STEPS 128

#define GLIMMER_HEIGHTFIELD_LEVEL_TEXELS 24.0

float GlimmerHeightfieldEscapeHeight(uint level)
{
    float height = -GLIMMER_HEIGHT_UNBOUNDED;

    for (uint levelIndex = level; levelIndex < GLIMMER_GROUND_LEVELS; levelIndex++)
    {
        height = max(height, glimmerHeightBounds[levelIndex * GLIMMER_HEIGHT_BOUNDS_STRIDE + GLIMMER_HEIGHT_BOUNDS_WINDOW]);
    }

    return height;
}

float GlimmerHeightfieldTileTop(GlimmerGroundParams groundParams, uint level, float2 worldXZ)
{
    const GlimmerGroundLevel groundLevel = groundParams.levels[level];
    const int2 texel = int2(floor(worldXZ * groundLevel.params.y));

    if (any(texel < groundLevel.validRect.xy) || any(texel >= groundLevel.validRect.zw))
    {
        return GLIMMER_HEIGHT_UNBOUNDED;
    }

    return glimmerHeightBounds[level * GLIMMER_HEIGHT_BOUNDS_STRIDE + GLIMMER_HEIGHT_BOUNDS_SKIP + GlimmerHeightBoundsTile(texel)];
}

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
    float escapeHeight = GlimmerHeightfieldEscapeHeight(stepLevel);

    float t = 0.0;

    [loop]
    for (uint stepIndex = 0; stepIndex < GLIMMER_HEIGHTFIELD_MAX_STEPS && t < tMax; stepIndex++)
    {
        const float3 previousPosition = origin + direction * t;

        while (stepLevel + 1 < GLIMMER_GROUND_LEVELS
            && (t > GLIMMER_HEIGHTFIELD_LEVEL_TEXELS * groundParams.levels[stepLevel].params.x || !GlimmerGroundLevelCovers(groundParams, stepLevel, previousPosition.xz)))
        {
            stepLevel++;
            escapeHeight = GlimmerHeightfieldEscapeHeight(stepLevel);
        }

        if (direction.y >= 0.0 && previousPosition.y > escapeHeight)
        {
            return false;
        }

        const float texelSize = groundParams.levels[stepLevel].params.x;

        const float tileTop = GlimmerHeightfieldTileTop(groundParams, stepLevel, previousPosition.xz);

        if (previousPosition.y > tileTop)
        {
            const float tileSize = texelSize * float(GLIMMER_HEIGHT_BOUNDS_TILE_TEXELS);
            const float2 tileMin = floor(previousPosition.xz / tileSize) * tileSize;

            float tSkip = tMax;

            if (abs(direction.x) > 1e-6)
            {
                tSkip = min(tSkip, t + ((direction.x > 0.0 ? tileMin.x + tileSize : tileMin.x) - previousPosition.x) / direction.x);
            }

            if (abs(direction.z) > 1e-6)
            {
                tSkip = min(tSkip, t + ((direction.z > 0.0 ? tileMin.y + tileSize : tileMin.y) - previousPosition.z) / direction.z);
            }

            if (direction.y < 0.0)
            {
                tSkip = min(tSkip, t + (previousPosition.y - tileTop) / -direction.y);
            }

            if (tSkip > t + 1e-3 * texelSize)
            {
                t = tSkip;

                continue;
            }
        }

        const float previousT = t;
        t = min(t + texelSize, tMax);

        const float3 position = origin + direction * t;

        GlimmerSpanSample spanSample;

        if (GlimmerSampleSpans(spanParams, stepLevel, 0.5 * (position.xz + previousPosition.xz), spanSample))
        {
            const float segmentLength = t - previousT;

            const GlimmerSpanCrossing canopy = GlimmerCrossSpan(
                previousPosition.y,
                position.y,
                segmentLength,
                spanSample.canopyMin,
                spanSample.canopyMax,
                spanSample.canopyBins,
                spanSample.canopyBands,
                GLIMMER_LEAF_PROJECTION * foliageExtinction,
                0.5,
                GLIMMER_HEIGHT_UNBOUNDED);

            if (canopy.length > 0.0)
            {
                const float segmentTransmittance = exp(-canopy.opticalDepth);

                if (accumulateCanopy)
                {
                    const float3 midpoint = origin + direction * (0.5 * (previousT + t));
                    const float3 radiance = GlimmerCanopyRadiance(
                        midpoint,
                        spanSample.canopyAlbedo,
                        max(spanSample.canopyMax - midpoint.y, 0.0),
                        canopy.opticalDepth / canopy.length);

                    hit.inscatter += hit.transmittance * (1.0 - segmentTransmittance) * radiance;
                }

                hit.transmittance *= segmentTransmittance;
            }

            if (t > solidsFromT)
            {
                const GlimmerSpanCrossing solid = GlimmerCrossSpan(
                    previousPosition.y,
                    position.y,
                    segmentLength,
                    spanSample.solidMin,
                    spanSample.solidMax,
                    spanSample.solidBins,
                    spanSample.solidBands,
                    HYP_FMATH_ONE_OVER_PI,
                    texelSize,
                    GLIMMER_SPAN_SOLID_THRESHOLD / texelSize);

                if (solid.denseBins != 0u)
                {
                    int side;
                    const float entry = GlimmerSpanEntry(previousPosition.y, position.y, spanSample.solidMin, spanSample.solidMax, solid.denseBins, side);

                    hit.t = max(previousT + entry * segmentLength, solidsFromT);
                    hit.t = hit.t > 0.0 ? hit.t : 0.5 * t;
                    hit.kind = GLIMMER_HEIGHTFIELD_SOLID;
                    hit.level = stepLevel;
                    hit.normal = side > 0
                        ? float3(0.0, 1.0, 0.0)
                        : (side < 0 ? float3(0.0, -1.0, 0.0) : -normalize(float3(direction.x, 0.0, direction.z) + float3(1e-5, 0.0, 0.0)));
                    hit.albedo = spanSample.solidAlbedo;

                    return true;
                }

                hit.transmittance *= exp(-solid.opticalDepth);
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
    }

    return false;
}

#endif
