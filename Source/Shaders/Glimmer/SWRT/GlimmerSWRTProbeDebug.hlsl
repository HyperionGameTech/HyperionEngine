#include "../../Include/Defines.hlsli"

#include "GlimmerSWRTCommon.hlsli"

#include "GlimmerProbeTypes.hlsli"

// Packs every probe of the clipmap, where it is and what its last trace saw, for the CPU to read back and draw with the DebugDrawer

// Must match GlimmerProbeDebugConstants in GlimmerSWRTProbeDebug.cpp
struct GlimmerProbeDebugConstants
{
    GlimmerProbeVolume volume;
};

DECLARE_BUFFER_DYNAMIC(GlimmerSWRTProbeDebug, CBuffer) cbuffer CBuffer
{
    GlimmerProbeDebugConstants constants;
};

// Must match GlimmerProbeDebugRecord in GlimmerChannel.hpp
struct GlimmerProbeDebugRecord
{
    float4 position; // xyz = where the probe was last traced, or where it will be when it hasn't been yet
    uint4 info;      // x = 1 once traced for its column, y = rays that hit a back face, z = rays that started under the ground, w = updates
    float4 shR;
    float4 shG;
    float4 shB;
};

DECLARE_SRV(GlimmerSWRTProbeDebug, Rays) StructuredBuffer<float4> rays;
DECLARE_SRV(GlimmerSWRTProbeDebug, GlimmerProbeSHTexture) Texture3D<float4> glimmerProbeSH;
DECLARE_SRV(GlimmerSWRTProbeDebug, GlimmerProbeStateTexture) Texture3D<uint2> glimmerProbeState;
DECLARE_SRV(GlimmerSWRTProbeDebug, GlimmerProbeBaseTexture) Texture2DArray<float> glimmerProbeBase;

DECLARE_UAV(GlimmerSWRTProbeDebug, OutRecords) RWStructuredBuffer<GlimmerProbeDebugRecord> OutRecords;

#define GLIMMER_PROBES_NO_SAMPLING
#include "GlimmerProbes.hlsli"

[numthreads(64, 1, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    // a row of groups per cascade
    const uint cascadeIndex = dispatchThreadId.y;
    const uint probeIndex = dispatchThreadId.x;

    if (probeIndex >= GLIMMER_PROBES_PER_CASCADE || cascadeIndex >= constants.volume.info.x)
    {
        return;
    }

    const GlimmerProbeCascade cascade = constants.volume.cascades[cascadeIndex];

    int2 localColumn;
    uint layer;
    GlimmerProbeFromIndex(probeIndex, localColumn, layer);

    const int2 column = cascade.gridOrigin.xy + localColumn;
    const uint3 texel = GlimmerProbeTexel(cascadeIndex, column, layer);
    const uint2 state = glimmerProbeState.Load(int4(texel, 0));

    const bool isTraced = GlimmerIsSameColumn(state.x, column);

    float3 position = GlimmerProbePosition(cascade, cascadeIndex, column, layer);

    if (isTraced)
    {
        position.y = asfloat(state.y);
    }

    const uint probeRecord = cascadeIndex * GLIMMER_PROBES_PER_CASCADE + probeIndex;

    // ray records follow the probe's place in the grid rather than its column, so for the few frames after the grid scrolls they
    // can still hold what the probe that was there before saw
    uint numBackfaces = 0u;
    uint numBuried = 0u;

    if (isTraced)
    {
        const uint numRays = constants.volume.info.y;

        for (uint rayIndex = 0; rayIndex < numRays; rayIndex++)
        {
            const float hitT = rays[probeRecord * numRays + rayIndex].w;

            if (hitT < 0.0)
            {
                numBackfaces++;
            }
            else if (hitT == 0.0)
            {
                numBuried++;
            }
        }
    }

    GlimmerProbeDebugRecord record;
    record.position = float4(position, 0.0);
    record.info = uint4(isTraced ? 1u : 0u, numBackfaces, numBuried, GlimmerProbeUpdates(state.x));
    record.shR = isTraced ? glimmerProbeSH.Load(int4(GlimmerProbeSHTexel(texel, 0u), 0)) : (float4)0.0;
    record.shG = isTraced ? glimmerProbeSH.Load(int4(GlimmerProbeSHTexel(texel, 1u), 0)) : (float4)0.0;
    record.shB = isTraced ? glimmerProbeSH.Load(int4(GlimmerProbeSHTexel(texel, 2u), 0)) : (float4)0.0;

    OutRecords[probeRecord] = record;
}
