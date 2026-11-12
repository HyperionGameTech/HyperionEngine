#include "../../Include/Defines.hlsli"

#include "GlimmerSurfelTypes.hlsli"

#define GLIMMER_SURFEL_TRANSFER_MAX_CHANGES 16

struct GlimmerSurfelTransferConstants
{
    GlimmerSurfels surfels;
    GlimmerSurfels previousSurfels;

    /// x = changed boxes
    uint4 dispatch;

    float4 changedMin[GLIMMER_SURFEL_TRANSFER_MAX_CHANGES];
    float4 changedMax[GLIMMER_SURFEL_TRANSFER_MAX_CHANGES];
};

DECLARE_BUFFER_DYNAMIC(GlimmerSurfelTransfer, CBuffer) cbuffer CBuffer
{
    GlimmerSurfelTransferConstants constants;
};

// the previous surfels, as the gather reads them
DECLARE_SRV(GlimmerSurfelTransfer, GlimmerSurfelsBuffer) StructuredBuffer<uint4> glimmerSurfels;
DECLARE_SRV(GlimmerSurfelTransfer, GlimmerSurfelHashBuffer) StructuredBuffer<uint> glimmerSurfelHash;
DECLARE_SRV(GlimmerSurfelTransfer, GlimmerSurfelDataBuffer) StructuredBuffer<uint> glimmerSurfelData;
DECLARE_SRV(GlimmerSurfelTransfer, GlimmerFoliageBuffer) StructuredBuffer<uint> glimmerFoliage;
DECLARE_SRV(GlimmerSurfelTransfer, PreviousSurfelStates) StructuredBuffer<uint2> PreviousSurfelStates;

DECLARE_SRV(GlimmerSurfelTransfer, NewSurfels) StructuredBuffer<uint4> NewSurfels;
DECLARE_SRV(GlimmerSurfelTransfer, NewSurfelHash) StructuredBuffer<uint> NewSurfelHash;

DECLARE_UAV(GlimmerSurfelTransfer, OutSurfelData) RWStructuredBuffer<uint> OutSurfelData;
DECLARE_UAV(GlimmerSurfelTransfer, OutSurfelStates) RWStructuredBuffer<uint2> OutSurfelStates;

#include "GlimmerSurfelsGather.hlsli"

#define GLIMMER_SURFEL_TRANSFER_MIN_FACING 0.9

// a state whose updates are none, so the next blend replaces the light, but which the gather still samples until then
#define GLIMMER_SURFEL_STATE_START_OVER 0xFFFF0000u

bool GlimmerIsChangedAround(float3 P)
{
    for (uint change = 0; change < constants.dispatch.x; change++)
    {
        if (all(P >= constants.changedMin[change].xyz) && all(P <= constants.changedMax[change].xyz))
        {
            return true;
        }
    }

    return false;
}

[numthreads(64, 1, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint surfelIndex = dispatchThreadId.x;

    if (surfelIndex >= min(NewSurfelHash[GLIMMER_SURFEL_COUNTER_SURFELS], GLIMMER_SURFEL_CAPACITY))
    {
        return;
    }

    const uint4 surfel = NewSurfels[surfelIndex];
    const float3 P = asfloat(surfel.xyz);
    const float3 N = GlimmerUnpackSurfelNormal(surfel.w);

    const float reach = constants.previousSurfels.params.z;
    const int3 firstCell = GlimmerFirstSurfelGatherCell(constants.previousSurfels, P);

    uint nearest = GLIMMER_SURFEL_NONE;
    float nearestDistanceSquared = reach * reach;

    [loop]
    for (uint corner = 0; corner < 8; corner++)
    {
        uint previousIndex = GlimmerFindSurfelCell(GlimmerSurfelGatherCell(firstCell, corner));

        [loop]
        for (uint step = 0; step < GLIMMER_SURFEL_MAX_PER_CELL && previousIndex != GLIMMER_SURFEL_NONE; step++)
        {
            const uint4 previous = glimmerSurfels[previousIndex];

            if (dot(N, GlimmerUnpackSurfelNormal(previous.w)) >= GLIMMER_SURFEL_TRANSFER_MIN_FACING)
            {
                const float3 toPrevious = asfloat(previous.xyz) - P;
                const float distanceSquared = dot(toPrevious, toPrevious);

                if (distanceSquared < nearestDistanceSquared)
                {
                    nearestDistanceSquared = distanceSquared;
                    nearest = previousIndex;
                }
            }

            previousIndex = GlimmerNextSurfel(previousIndex);
        }
    }

    if (nearest == GLIMMER_SURFEL_NONE)
    {
        return;
    }

    // all but the hash's link, which the placement wrote
    for (uint field = GLIMMER_SURFEL_DATA_NEXT + 1u; field < GLIMMER_SURFEL_DATA_UINTS; field++)
    {
        OutSurfelData[GlimmerSurfelDataIndex(surfelIndex, field)] = glimmerSurfelData[GlimmerSurfelDataIndex(nearest, field)];
    }

    uint2 state = PreviousSurfelStates[nearest];

    if (GlimmerIsChangedAround(P))
    {
        state.x = GLIMMER_SURFEL_STATE_START_OVER;
    }

    OutSurfelStates[surfelIndex] = state;
}
