#ifndef GLIMMER_RELIGHT_HLSLI
#define GLIMMER_RELIGHT_HLSLI

#include "GlimmerLight.hlsli"

#define GLIMMER_RELIGHT_GROUND 0u
#define GLIMMER_RELIGHT_SPAN_TOP 1u
#define GLIMMER_RELIGHT_SURFACES 2u

#define GLIMMER_RELIGHT_RESOLUTION (GLIMMER_GROUND_RESOLUTION / 2)
#define GLIMMER_RELIGHT_LAYERS ((GLIMMER_RELIGHT_SURFACES * GLIMMER_LIGHT_PARTS + 3) / 4)

struct GlimmerRelightParams
{
    /// xy = absolute texel of the lit window's origin
    /// z = 1 once all of it has been lit
    int4 levels[GLIMMER_GROUND_LEVELS];
};

uint3 GlimmerRelightTexel(int2 texel, uint level, uint surface, uint part)
{
    const uint index = surface * GLIMMER_LIGHT_PARTS + part;
    const uint quadrant = index & 3u;

    const uint2 block = uint2((texel >> 1) & (GLIMMER_RELIGHT_RESOLUTION - 1));

    return uint3(
        block + uint2(quadrant & 1u, quadrant >> 1) * GLIMMER_RELIGHT_RESOLUTION,
        level * GLIMMER_RELIGHT_LAYERS + (index >> 2));
}

/// the texel of each 2x2 block that lights the block.
/// the ground's windows start on even texels, so it's always in with its block.
bool GlimmerRelightLightsBlock(int2 texel)
{
    return all((texel & 1) == 0);
}

bool GlimmerRelightCovers(bool isGroundHit, float3 hitNormal)
{
    return isGroundHit || hitNormal.y > 0.5;
}

#endif

#if defined(GLIMMER_RELIGHT_WITH_SAMPLING) && !defined(GLIMMER_RELIGHT_SAMPLING_HLSLI)
#define GLIMMER_RELIGHT_SAMPLING_HLSLI

bool GlimmerSampleRelight(
    GlimmerRelightParams params,
    GlimmerGroundParams ground,
    uint level,
    float2 worldXZ,
    uint surface,
    out GlimmerLight outReceived,
    out float3 outReceivedSun)
{
    outReceived = GlimmerNoLight();
    outReceivedSun = (float3)0.0;

    const int4 litWindow = params.levels[level];

    if (litWindow.z == 0)
    {
        return false;
    }

    const int2 texel = int2(floor(worldXZ * ground.levels[level].params.y));

    if (any(texel < litWindow.xy) || any(texel >= litWindow.xy + GLIMMER_GROUND_RESOLUTION))
    {
        return false;
    }

    const float4 first = glimmerRelight.Load(int4(GlimmerRelightTexel(texel, level, surface, 0u), 0));

    if (first.a < 0.0)
    {
        return false;
    }

    outReceived.parts[0] = first.rgb;

    for (uint part = 1; part < GLIMMER_LIGHT_PARTS; part++)
    {
        const float4 value = glimmerRelight.Load(int4(GlimmerRelightTexel(texel, level, surface, part), 0));

        outReceived.parts[part] = value.rgb;

        if (part <= 3u)
        {
            outReceivedSun[part - 1u] = value.a;
        }
    }

    return true;
}

#endif
