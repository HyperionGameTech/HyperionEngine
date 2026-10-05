#include "../../Include/Defines.hlsli"

#include "GlimmerSWRTCommon.hlsli"
#include "GlimmerProbeTypes.hlsli"

#define GLIMMER_SH_OCCUPANCY_NO_TRACE
#include "../SH/GlimmerSHOccupancy.hlsli"
#undef GLIMMER_SH_OCCUPANCY_NO_TRACE

struct GlimmerProbeDebugConstants
{
    GlimmerProbeVolume volume;
    GlimmerGroundParams ground;
    GlimmerSHOccupancyParams occupancy;
};

DECLARE_BUFFER_DYNAMIC(GlimmerSWRTProbeDebug, CBuffer) cbuffer CBuffer
{
    GlimmerProbeDebugConstants constants;
};

struct GlimmerProbeDebugRecord
{
    float4 position; // xyz = where the probe is (its grid point plus its offset), w = its level, or -1 for a probe of a free slot
    uint4 info;      // x = GLIMMER_PROBE_STATE_* | 0x100 where the SH occupancy has a solid | rays that started inside a solid << 16, y = back face rays of its last update, z = height above the ground (float bits), w = updates
    float4 shR;
    float4 shG;
    float4 shB;
};

DECLARE_SRV(GlimmerSWRTProbeDebug, GlimmerProbeSHBuffer) StructuredBuffer<float4> glimmerProbeSH;
DECLARE_SRV(GlimmerSWRTProbeDebug, GlimmerProbeStatesBuffer) StructuredBuffer<uint4> glimmerProbeStates;
DECLARE_SRV(GlimmerSWRTProbeDebug, GlimmerProbeSlotsBuffer) StructuredBuffer<int4> glimmerProbeSlots;

DECLARE_SRV(GlimmerSWRTProbeDebug, GlimmerGroundTexture) Texture2DArray<float> glimmerGround;
DECLARE_SRV(GlimmerSWRTProbeDebug, GlimmerSHOccupancyTexture) Texture3D<float4> glimmerSHOccupancy;
DECLARE_SRV(GlimmerSWRTProbeDebug, GlimmerSHOccupancyMaskBuffer) StructuredBuffer<uint> glimmerSHOccupancyMask;

DECLARE_UAV(GlimmerSWRTProbeDebug, OutRecords) RWStructuredBuffer<GlimmerProbeDebugRecord> OutRecords;

#include "../SH/GlimmerSHOccupancy.hlsli"
#include "../GlimmerGround.hlsli"

#define GLIMMER_PROBES_NO_SAMPLING
#include "GlimmerProbes.hlsli"

[numthreads(64, 1, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint probeIndex = dispatchThreadId.x;

    if (probeIndex >= GLIMMER_PROBE_POOL_PROBES)
    {
        return;
    }

    const int4 slot = glimmerProbeSlots[probeIndex / GLIMMER_PROBES_PER_BLOCK];
    const uint4 state = glimmerProbeStates[probeIndex];

    GlimmerProbeDebugRecord record;

    if (slot.w < 0 || slot.w >= GLIMMER_PROBE_LEVELS)
    {
        record.position = float4(0.0, 0.0, 0.0, -1.0);
        record.info = (uint4)0u;
        record.shR = (float4)0.0;
        record.shG = (float4)0.0;
        record.shB = (float4)0.0;
    }
    else
    {
        const float3 position = GlimmerProbePosition(constants.volume, probeIndex, slot, state);

        float groundHeight;
        uint groundLevel;

        const bool gs = GlimmerSampleGround(
            constants.ground,
            position.xz,
            0u,
            groundHeight,
            groundLevel);

        const float heightAboveGround = select(gs, position.y - groundHeight, 1e30);
        const bool isOccupied = GlimmerSHOccupancyIsSolid(constants.occupancy, position);

        record.position = float4(position, float(slot.w));
        record.info = uint4(
            GlimmerProbeStateOf(state) | (isOccupied ? 0x100u : 0u) | (GlimmerProbeStartsInside(state) << 16),
            GlimmerProbeBackfaces(state),
            asuint(heightAboveGround),
            GlimmerProbeUpdates(state));
            
        record.shR = glimmerProbeSH[probeIndex * 3u + 0u];
        record.shG = glimmerProbeSH[probeIndex * 3u + 1u];
        record.shB = glimmerProbeSH[probeIndex * 3u + 2u];
    }

    OutRecords[probeIndex] = record;
}
