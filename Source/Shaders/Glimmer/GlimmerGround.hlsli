#ifndef GLIMMER_GROUND_HLSLI
#define GLIMMER_GROUND_HLSLI

#include "GlimmerCommon.hlsli"

// Sampling and marching the ground heightfield clipmap; expects Texture2DArray<float> glimmerGround to be declared

#ifndef GLIMMER_GROUND_MAX_STEPS
#define GLIMMER_GROUND_MAX_STEPS 96
#endif

// steps taken at a level before moving to the next coarser one
#define GLIMMER_GROUND_STEPS_PER_LEVEL 24

bool GlimmerSampleGroundLevel(GlimmerGroundParams params, uint level, float2 worldXZ, out float height)
{
    height = GLIMMER_NO_GROUND_HEIGHT;

    const GlimmerGroundLevel groundLevel = params.levels[level];

    const float2 texelCoord = worldXZ * groundLevel.params.y - 0.5;
    const int2 texel0 = int2(floor(texelCoord));

    if (any(texel0 < groundLevel.validRect.xy) || any(texel0 + 1 >= groundLevel.validRect.zw))
    {
        return false;
    }

    const float2 fraction = texelCoord - float2(texel0);

    const float h00 = glimmerGround.Load(int4(GlimmerWrapGroundTexel(texel0), level, 0));
    const float h10 = glimmerGround.Load(int4(GlimmerWrapGroundTexel(texel0 + int2(1, 0)), level, 0));
    const float h01 = glimmerGround.Load(int4(GlimmerWrapGroundTexel(texel0 + int2(0, 1)), level, 0));
    const float h11 = glimmerGround.Load(int4(GlimmerWrapGroundTexel(texel0 + int2(1, 1)), level, 0));

    if (min(min(h00, h10), min(h01, h11)) <= GLIMMER_NO_GROUND_HEIGHT + 1.0)
    {
        return false;
    }

    height = lerp(lerp(h00, h10, fraction.x), lerp(h01, h11, fraction.x), fraction.y);

    return true;
}

/*! Ground height from the finest level at or above minLevel that covers worldXZ. */
bool GlimmerSampleGround(GlimmerGroundParams params, float2 worldXZ, uint minLevel, out float height, out uint level)
{
    height = GLIMMER_NO_GROUND_HEIGHT;
    level = minLevel;

    [loop]
    for (uint levelIndex = minLevel; levelIndex < GLIMMER_GROUND_LEVELS; levelIndex++)
    {
        if (GlimmerSampleGroundLevel(params, levelIndex, worldXZ, height))
        {
            level = levelIndex;

            return true;
        }
    }

    return false;
}

float3 GlimmerGroundNormal(GlimmerGroundParams params, float2 worldXZ, uint level)
{
    const float texelSize = params.levels[level].params.x;

    float hL, hR, hD, hU;
    uint unusedLevel;

    const bool valid = GlimmerSampleGround(params, worldXZ - float2(texelSize, 0.0), level, hL, unusedLevel)
        && GlimmerSampleGround(params, worldXZ + float2(texelSize, 0.0), level, hR, unusedLevel)
        && GlimmerSampleGround(params, worldXZ - float2(0.0, texelSize), level, hD, unusedLevel)
        && GlimmerSampleGround(params, worldXZ + float2(0.0, texelSize), level, hU, unusedLevel);

    if (!valid)
    {
        return float3(0.0, 1.0, 0.0);
    }

    return normalize(float3(hL - hR, 2.0 * texelSize, hD - hU));
}

/*! Marches the ray against the ground heightfield, refining the crossing by bisection.
 *  Steps start at startLevel's texel size and grow a level at a time, and the march stops where no level covers the ray. */
bool GlimmerTraceGround(GlimmerGroundParams params, float3 origin, float3 direction, float tMax, uint startLevel, out float outT, out uint outLevel)
{
    outT = tMax;
    outLevel = startLevel;

    float groundHeight;
    uint level;

    if (!GlimmerSampleGround(params, origin.xz, startLevel, groundHeight, level))
    {
        return false;
    }

    if (origin.y <= groundHeight)
    {
        outT = 0.0;
        outLevel = level;

        return true;
    }

    uint stepLevel = level;
    uint stepsAtLevel = 0;

    float previousT = 0.0;
    float t = 0.0;

    [loop]
    for (uint stepIndex = 0; stepIndex < GLIMMER_GROUND_MAX_STEPS && t < tMax; stepIndex++)
    {
        t = min(t + params.levels[stepLevel].params.x, tMax);

        const float3 position = origin + direction * t;

        if (!GlimmerSampleGround(params, position.xz, stepLevel, groundHeight, level))
        {
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

                if (GlimmerSampleGround(params, midPosition.xz, stepLevel, midHeight, midLevel) && midPosition.y <= midHeight)
                {
                    highT = midT;
                }
                else
                {
                    lowT = midT;
                }
            }

            outT = highT;
            outLevel = level;

            return true;
        }

        previousT = t;

        // once the ray is well above the terrain it can't hit, head straight out
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

/*! Terrain albedo under worldXZ from the finest level at or above minLevel that has it; the albedo clipmap's alpha is 0 where no terrain patch was seen yet. */
float3 GlimmerSampleGroundAlbedo(Texture2DArray<float4> groundAlbedo, GlimmerGroundParams params, float2 worldXZ, uint minLevel, float3 fallback)
{
    [loop]
    for (uint level = minLevel; level < GLIMMER_GROUND_LEVELS; level++)
    {
        const GlimmerGroundLevel groundLevel = params.levels[level];
        const int2 texel = int2(floor(worldXZ * groundLevel.params.y));

        if (any(texel < groundLevel.validRect.xy) || any(texel >= groundLevel.validRect.zw))
        {
            continue;
        }

        const float4 albedo = groundAlbedo.Load(int4(GlimmerWrapGroundTexel(texel), level, 0));

        if (albedo.a > 0.5)
        {
            return albedo.rgb;
        }
    }

    return fallback;
}

#endif
