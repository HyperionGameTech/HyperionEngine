#ifndef GLIMMER_RELIGHT_HLSLI
#define GLIMMER_RELIGHT_HLSLI

#define GLIMMER_RELIGHT_GROUND 0u
#define GLIMMER_RELIGHT_SPAN_TOP 1u
#define GLIMMER_RELIGHT_LAYERS 2u

#define GLIMMER_RELIGHT_MAX_RATIO 64.0

struct GlimmerRelightParams
{
    int4 levels[GLIMMER_GROUND_LEVELS]; // xy = absolute texel of the lit window's origin, z = 1 once all of it has been lit
};

uint GlimmerRelightSlice(uint level, uint layer)
{
    return level * GLIMMER_RELIGHT_LAYERS + layer;
}

bool GlimmerRelightCovers(bool isGroundHit, float3 hitNormal)
{
    return isGroundHit || hitNormal.y > 0.5;
}

#endif

#if defined(GLIMMER_RELIGHT_WITH_SAMPLING) && !defined(GLIMMER_RELIGHT_SAMPLING_HLSLI)
#define GLIMMER_RELIGHT_SAMPLING_HLSLI

bool GlimmerSampleRelight(GlimmerRelightParams params, GlimmerGroundParams ground, uint level, float2 worldXZ, uint layer, out float4 outRelight)
{
    outRelight = (float4)0.0;

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

    outRelight = glimmerRelight.Load(int4(GlimmerWrapGroundTexel(texel), GlimmerRelightSlice(level, layer), 0));

    return outRelight.a >= 0.0;
}

#endif
