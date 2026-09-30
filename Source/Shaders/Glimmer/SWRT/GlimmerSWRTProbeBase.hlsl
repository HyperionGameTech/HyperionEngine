#include "../../Include/Defines.hlsli"

#include "GlimmerSWRTCommon.hlsli"

#include "GlimmerProbeTypes.hlsli"

struct GlimmerProbeBaseConstants
{
    GlimmerProbeVolume volume;
    GlimmerGroundParams ground;
};

DECLARE_BUFFER_DYNAMIC(GlimmerProbeBase, CBuffer) cbuffer CBuffer
{
    GlimmerProbeBaseConstants constants;
};

DECLARE_SRV(GlimmerProbeBase, GlimmerGroundTexture) Texture2DArray<float> glimmerGround;
DECLARE_UAV(GlimmerProbeBase, OutProbeBase) RWTexture2DArray<float> OutProbeBase;

#include "../GlimmerGround.hlsli"

[numthreads(64, 1, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint columnsPerCascade = GLIMMER_PROBE_GRID * GLIMMER_PROBE_GRID;

    const uint cascadeIndex = dispatchThreadId.x / columnsPerCascade;
    const uint columnIndex = dispatchThreadId.x % columnsPerCascade;

    if (cascadeIndex >= constants.volume.info.x)
    {
        return;
    }

    const GlimmerProbeCascade cascade = constants.volume.cascades[cascadeIndex];

    const int2 column = cascade.gridOrigin.xy + int2(columnIndex % GLIMMER_PROBE_GRID, columnIndex / GLIMMER_PROBE_GRID);
    const float spacing = cascade.params.x;
    const float2 center = (float2(column) + 0.5) * spacing;

    // ground texels no bigger than half the column spacing, so the footprint minimum sees its features
    const uint startLevel = uint(clamp(int(cascadeIndex) - 1, 0, GLIMMER_GROUND_LEVELS - 1));

    static const float2 offsets[9] = {
        float2(0.0, 0.0),
        float2(-0.5, -0.5), float2(0.5, -0.5), float2(-0.5, 0.5), float2(0.5, 0.5),
        float2(-0.5, 0.0), float2(0.5, 0.0), float2(0.0, -0.5), float2(0.0, 0.5)
    };

    float base = 1e30;
    bool hasGround = false;

    [unroll]
    for (uint sampleIndex = 0; sampleIndex < 9; sampleIndex++)
    {
        float height;
        uint level;

        if (GlimmerSampleGround(constants.ground, center + offsets[sampleIndex] * spacing, startLevel, height, level))
        {
            base = min(base, height);
            hasGround = true;
        }
    }

    if (!hasGround)
    {
        base = constants.volume.params.x;
    }

    OutProbeBase[uint3(GlimmerWrapProbeColumn(column), cascadeIndex)] = base;
}
