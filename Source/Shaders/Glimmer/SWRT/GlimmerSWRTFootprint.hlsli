#ifndef GLIMMER_SWRT_FOOTPRINT_HLSLI
#define GLIMMER_SWRT_FOOTPRINT_HLSLI

#include "GlimmerSWRTCommon.hlsli"

#define GLIMMER_MASK_QUERY_SLACK 0.05
#define GLIMMER_MASK_MAX_STEPS 64

uint GlimmerMaskMaxExactLevel(GlimmerFootprintMaskParams mask)
{
    return min(mask.info.y - 1u, firstbitlow(mask.info.x));
}

bool GlimmerMaskAnyInBox(GlimmerFootprintMaskParams mask, float2 boxMinXZ, float2 boxMaxXZ, float boxMinY, float boxMaxY)
{
    if (mask.originCellSize.w == 0.0)
    {
        return true;
    }

    const float resolution = float(mask.info.x);

    const float2 minCoord = max((boxMinXZ - mask.originCellSize.xy) / mask.originCellSize.z, 0.0);
    const float2 maxCoord = min((boxMaxXZ - mask.originCellSize.xy) / mask.originCellSize.z, resolution - 0.001);

    if (any(minCoord > maxCoord))
    {
        return false;
    }

    // the finest level whose cells are as wide as the box, so it touches at most 2x2 of them
    const uint maxLevel = GlimmerMaskMaxExactLevel(mask);
    const float2 size = maxCoord - minCoord;

    uint level = 0;

    while (level < maxLevel && float(1u << level) < max(size.x, size.y))
    {
        level++;
    }

    const uint levelResolution = GlimmerMaskLevelResolution(mask, level);
    const uint levelOffset = GlimmerMaskLevelOffset(mask, level);

    const int2 minCell = int2(floor(minCoord / float(1u << level)));
    const int2 maxCell = min(int2(floor(maxCoord / float(1u << level))), int2(levelResolution - 1u, levelResolution - 1u));

    const uint encodedLow = GlimmerOrderedUintFromFloat(boxMinY - GLIMMER_MASK_QUERY_SLACK);
    const uint encodedHigh = GlimmerOrderedUintFromFloat(boxMaxY + GLIMMER_MASK_QUERY_SLACK);

    for (int y = minCell.y; y <= maxCell.y; y++)
    {
        for (int x = minCell.x; x <= maxCell.x; x++)
        {
            const uint cellIndex = (levelOffset + uint(y) * levelResolution + uint(x)) * 2u;

            // an empty cell has min > max, so it can't satisfy both
            if (footprintMask[cellIndex] <= encodedHigh && footprintMask[cellIndex + 1u] >= encodedLow)
            {
                return true;
            }
        }
    }

    return false;
}

bool GlimmerMaskTraceRay(
    GlimmerFootprintMaskParams mask,
    uint level,
    float3 origin,
    float3 direction,
    float tMax,
    out float tFirst,
    out float tLast,
    out float tCovered)
{
    tFirst = 0.0;
    tLast = tMax;
    tCovered = tMax;

    if (mask.originCellSize.w == 0.0)
    {
        return true;
    }

    level = min(level, GlimmerMaskMaxExactLevel(mask));

    const uint resolution = GlimmerMaskLevelResolution(mask, level);
    const float cellSize = mask.originCellSize.z * float(1u << level);
    const float extent = float(resolution) * cellSize;

    // a ray along an axis would divide by zero
    const float2 safeDirection = float2(select(abs(direction.x) < 1e-6, 1e-6, direction.x), select(abs(direction.z) < 1e-6, 1e-6, direction.z));
    const float2 inverseDirection = 1.0 / safeDirection;

    const float2 localOrigin = origin.xz - mask.originCellSize.xy;

    // where the ray is inside the mask's extent
    const float2 t0 = (0.0 - localOrigin) * inverseDirection;
    const float2 t1 = (extent - localOrigin) * inverseDirection;

    const float tEnterMask = max(min(t0.x, t1.x), min(t0.y, t1.y));
    const float tExitMask = min(min(max(t0.x, t1.x), max(t0.y, t1.y)), tMax);

    // SWRT covers a prefix of the ray, so one that starts outside the mask isn't covered at all
    if (tEnterMask > 1e-4 || tExitMask <= 0.0)
    {
        tFirst = 0.0;
        tLast = 0.0;
        tCovered = 0.0;

        return false;
    }

    const float tEnd = tExitMask;
    tCovered = tEnd;

    const uint levelOffset = GlimmerMaskLevelOffset(mask, level);

    const float2 position = localOrigin / cellSize;

    int2 cell = clamp(int2(floor(position)), int2(0, 0), int2(resolution - 1u, resolution - 1u));

    const int2 step = int2(select(safeDirection.x > 0.0, 1, -1), select(safeDirection.y > 0.0, 1, -1));
    const float2 tDelta = abs(cellSize * inverseDirection);

    float2 tNext = (float2(cell + max(step, int2(0, 0))) - position) * cellSize * inverseDirection;

    float tCurrent = 0.0;
    bool found = false;
    bool complete = false;

    [loop]
    for (uint stepIndex = 0; stepIndex < GLIMMER_MASK_MAX_STEPS; stepIndex++)
    {
        const float tCellEnd = min(min(tNext.x, tNext.y), tEnd);

        const float yA = origin.y + direction.y * tCurrent;
        const float yB = origin.y + direction.y * tCellEnd;

        const uint encodedLow = GlimmerOrderedUintFromFloat(min(yA, yB) - GLIMMER_MASK_QUERY_SLACK);
        const uint encodedHigh = GlimmerOrderedUintFromFloat(max(yA, yB) + GLIMMER_MASK_QUERY_SLACK);

        const uint cellIndex = (levelOffset + uint(cell.y) * resolution + uint(cell.x)) * 2u;

        if (footprintMask[cellIndex] <= encodedHigh && footprintMask[cellIndex + 1u] >= encodedLow)
        {
            if (!found)
            {
                tFirst = tCurrent;
                found = true;
            }

            tLast = tCellEnd;
        }

        if (tCellEnd >= tEnd)
        {
            complete = true;

            break;
        }

        if (tNext.x < tNext.y)
        {
            cell.x += step.x;
            tNext.x += tDelta.x;
        }
        else
        {
            cell.y += step.y;
            tNext.y += tDelta.y;
        }

        tCurrent = tCellEnd;

        // left the mask through float error at its edge; the rest of the ray is past what it covers
        if (any(cell < 0) || any(cell >= int(resolution)))
        {
            complete = true;

            break;
        }
    }

    if (!complete)
    {
        // out of steps: everything from here on counts as occupied
        if (!found)
        {
            tFirst = tCurrent;
            found = true;
        }

        tLast = tEnd;
    }

    if (!found)
    {
        tFirst = 0.0;
        tLast = 0.0;

        return false;
    }

    tFirst = max(tFirst - GLIMMER_MASK_QUERY_SLACK, 0.0);
    tLast = min(tLast + GLIMMER_MASK_QUERY_SLACK, tEnd);

    return true;
}

#endif
