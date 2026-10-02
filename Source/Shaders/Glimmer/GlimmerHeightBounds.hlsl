#include "../Include/Defines.hlsli"

PERMUTE(MODE, TILES, WINDOW)

#include "GlimmerCommon.hlsli"

struct GlimmerHeightBoundsConstants
{
    GlimmerGroundParams ground;
    GlimmerSpanParams spans;
    int4 info; // x = level, yz = absolute texel of the first tile to rebuild
};

DECLARE_BUFFER_DYNAMIC(GlimmerHeightBounds, CBuffer) cbuffer CBuffer
{
    GlimmerHeightBoundsConstants constants;
};

DECLARE_SRV(GlimmerHeightBounds, GlimmerGroundTexture) Texture2DArray<float> glimmerGround;
DECLARE_SRV(GlimmerHeightBounds, GlimmerSpansBuffer) StructuredBuffer<uint> glimmerSpans;
DECLARE_UAV(GlimmerHeightBounds, OutHeightBounds) RWStructuredBuffer<float> OutHeightBounds;

#define GROUP_SIZE (GLIMMER_HEIGHT_BOUNDS_TILE_TEXELS * GLIMMER_HEIGHT_BOUNDS_TILE_TEXELS)

// a tile's samples reach a texel past each side, through the ground's bilinear filter
#define TILE_SAMPLE_TEXELS (GLIMMER_HEIGHT_BOUNDS_TILE_TEXELS + 2)

groupshared uint gsDataMax;
groupshared uint gsUnknown;

[numthreads(GROUP_SIZE, 1, 1)]
void CSMain(uint3 groupId : SV_GroupID, uint groupIndex : SV_GroupIndex)
{
    const uint level = uint(constants.info.x);
    const uint levelBase = level * GLIMMER_HEIGHT_BOUNDS_STRIDE;

    if (groupIndex == 0u)
    {
        gsDataMax = GlimmerOrderedUintFromFloat(-GLIMMER_HEIGHT_UNBOUNDED);
        gsUnknown = 0u;
    }

    GroupMemoryBarrierWithGroupSync();

#if defined(MODE_TILES)
    const int2 tileMin = constants.info.yz + int2(groupId.xy) * GLIMMER_HEIGHT_BOUNDS_TILE_TEXELS;
    const int4 validRect = constants.ground.levels[level].validRect;

    float dataMax = -GLIMMER_HEIGHT_UNBOUNDED;
    bool isUnknown = false;

    for (uint sampleIndex = groupIndex; sampleIndex < TILE_SAMPLE_TEXELS * TILE_SAMPLE_TEXELS; sampleIndex += GROUP_SIZE)
    {
        const int2 texel = tileMin - 1 + int2(sampleIndex % TILE_SAMPLE_TEXELS, sampleIndex / TILE_SAMPLE_TEXELS);

        float height = GLIMMER_NO_GROUND_HEIGHT;

        if (all(texel >= validRect.xy) && all(texel < validRect.zw))
        {
            height = glimmerGround.Load(int4(GlimmerWrapGroundTexel(texel), level, 0));
        }

        if (height > GLIMMER_NO_GROUND_HEIGHT + 1.0)
        {
            dataMax = max(dataMax, height);
        }
        else
        {
            isUnknown = true;
        }
    }

    const GlimmerSpanLevel spanLevel = constants.spans.levels[level];
    const int2 texel = tileMin + int2(groupIndex % GLIMMER_HEIGHT_BOUNDS_TILE_TEXELS, groupIndex / GLIMMER_HEIGHT_BOUNDS_TILE_TEXELS);

    if (spanLevel.window.z != 0 && all(texel >= spanLevel.window.xy) && all(texel < spanLevel.window.xy + GLIMMER_GROUND_RESOLUTION))
    {
        const uint baseIndex = GlimmerSpanTexelIndex(level, texel);

        const uint solidMin = glimmerSpans[baseIndex + GLIMMER_SPAN_SOLID_MIN];
        const uint solidMax = glimmerSpans[baseIndex + GLIMMER_SPAN_SOLID_MAX];
        const uint canopyMin = glimmerSpans[baseIndex + GLIMMER_SPAN_CANOPY_MIN];
        const uint canopyMax = glimmerSpans[baseIndex + GLIMMER_SPAN_CANOPY_MAX];

        if (solidMin <= solidMax)
        {
            dataMax = max(dataMax, GlimmerFloatFromOrderedUint(solidMin) + GlimmerSpanHeight(GlimmerFloatFromOrderedUint(solidMin), GlimmerFloatFromOrderedUint(solidMax)));
        }

        if (canopyMin <= canopyMax)
        {
            dataMax = max(dataMax, GlimmerFloatFromOrderedUint(canopyMin) + GlimmerSpanHeight(GlimmerFloatFromOrderedUint(canopyMin), GlimmerFloatFromOrderedUint(canopyMax)));
        }
    }

    uint previous;
    InterlockedMax(gsDataMax, GlimmerOrderedUintFromFloat(dataMax), previous);

    if (isUnknown)
    {
        InterlockedOr(gsUnknown, 1u, previous);
    }

    GroupMemoryBarrierWithGroupSync();

    if (groupIndex == 0u)
    {
        const uint tileIndex = GlimmerHeightBoundsTile(tileMin);
        const float tileDataMax = GlimmerFloatFromOrderedUint(gsDataMax);

        OutHeightBounds[levelBase + GLIMMER_HEIGHT_BOUNDS_SKIP + tileIndex] = gsUnknown != 0u ? GLIMMER_HEIGHT_UNBOUNDED : tileDataMax;
        OutHeightBounds[levelBase + GLIMMER_HEIGHT_BOUNDS_DATA + tileIndex] = tileDataMax;
    }
#elif defined(MODE_WINDOW)
    uint previous;
    InterlockedMax(gsDataMax, GlimmerOrderedUintFromFloat(OutHeightBounds[levelBase + GLIMMER_HEIGHT_BOUNDS_DATA + groupIndex]), previous);

    GroupMemoryBarrierWithGroupSync();

    if (groupIndex == 0u)
    {
        OutHeightBounds[levelBase + GLIMMER_HEIGHT_BOUNDS_WINDOW] = GlimmerFloatFromOrderedUint(gsDataMax);
    }
#endif
}
