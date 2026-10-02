#ifndef GLIMMER_GROUND_HLSLI
#define GLIMMER_GROUND_HLSLI

#include "GlimmerCommon.hlsli"

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

bool GlimmerGroundLevelCovers(GlimmerGroundParams params, uint level, float2 worldXZ)
{
    const GlimmerGroundLevel groundLevel = params.levels[level];
    const int2 texel0 = int2(floor(worldXZ * groundLevel.params.y - 0.5));

    return all(texel0 >= groundLevel.validRect.xy) && all(texel0 + 1 < groundLevel.validRect.zw);
}

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
