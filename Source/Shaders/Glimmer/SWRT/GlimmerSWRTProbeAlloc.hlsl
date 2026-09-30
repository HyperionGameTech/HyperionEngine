#include "../../Include/Defines.hlsli"

#include "GlimmerSWRTCommon.hlsli"
#include "GlimmerProbeTypes.hlsli"

// its types now, its lookups once the texture is declared
#define GLIMMER_SH_OCCUPANCY_NO_TRACE
#include "../SH/GlimmerSHOccupancy.hlsli"
#undef GLIMMER_SH_OCCUPANCY_NO_TRACE

// Keeps the probe blocks where the solids are, then picks this frame's probes to trace:
//  ALLOC (one group) frees the blocks that scrolled out of their level's window or haven't been wanted for a while, and hands free slots
//        to the wanted blocks without one, nearest first
//  LIST (a thread per probe of the pool) marks probes under the ground, and lists the probes due for an update
PERMUTE(MODE, ALLOC, LIST)

// Must match GlimmerProbeAllocConstants in GlimmerSWRTProbeVolume.cpp
struct GlimmerProbeAllocConstants
{
    GlimmerProbeVolume volume;
    GlimmerGroundParams ground;
    GlimmerSHOccupancyParams occupancy;
    uint4 budget;   // x = probes traced per frame, y = frames an unwanted block keeps its slot, z = updates between retries of probes inside solids
    float4 params;  // x = block margin in spacings, y = how far above the ground a solid has to be to want probes around it
    float4 viewer;  // xyz = viewer position
};

DECLARE_BUFFER_DYNAMIC(GlimmerSWRTProbeAlloc, CBuffer) cbuffer CBuffer
{
    GlimmerProbeAllocConstants constants;
};

DECLARE_SRV(GlimmerSWRTProbeAlloc, GlimmerGroundTexture) Texture2DArray<float> glimmerGround;
DECLARE_SRV(GlimmerSWRTProbeAlloc, GlimmerSHOccupancyTexture) Texture3D<float4> glimmerSHOccupancy;

#include "../SH/GlimmerSHOccupancy.hlsli"

DECLARE_UAV(GlimmerSWRTProbeAlloc, OutBlockTable) RWStructuredBuffer<uint> OutBlockTable;
DECLARE_UAV(GlimmerSWRTProbeAlloc, OutSlots) RWStructuredBuffer<int4> OutSlots; // xyz = block, w = level (-1 when free)
DECLARE_UAV(GlimmerSWRTProbeAlloc, OutSlotAges) RWStructuredBuffer<uint> OutSlotAges; // frames since the block was last wanted
DECLARE_UAV(GlimmerSWRTProbeAlloc, OutStates) RWStructuredBuffer<uint4> OutStates;
DECLARE_UAV(GlimmerSWRTProbeAlloc, OutSH) RWStructuredBuffer<float4> OutSH;
DECLARE_UAV(GlimmerSWRTProbeAlloc, OutVisibility) RWStructuredBuffer<float2> OutVisibility;
DECLARE_UAV(GlimmerSWRTProbeAlloc, OutTrend) RWStructuredBuffer<float4> OutTrend;
// [0] = probes listed this frame, [1] = active probes this frame, [2] = active probes last frame, [3] = blocks allocated,
// [4] = wanted blocks the pool had no slot for
DECLARE_UAV(GlimmerSWRTProbeAlloc, OutCounters) RWStructuredBuffer<uint> OutCounters;
DECLARE_UAV(GlimmerSWRTProbeAlloc, OutUpdateList) RWStructuredBuffer<uint> OutUpdateList;

#include "../GlimmerGround.hlsli"

#define GLIMMER_PROBES_NO_SAMPLING
#include "GlimmerProbes.hlsli"

#define GLIMMER_PROBE_BURIED_MARGIN 0.25

void GlimmerResetProbe(uint probeIndex)
{
    OutStates[probeIndex] = uint4(GlimmerPackProbeFlags(GLIMMER_PROBE_STATE_ACTIVE, 0u, 0u, 0u), GlimmerPackProbeOffset((float3)0.0), 0u, 0u);

    [unroll]
    for (uint channel = 0; channel < 3; channel++)
    {
        OutSH[probeIndex * 3u + channel] = (float4)0.0;
    }

    // open in every direction until traced
    for (uint texel = 0; texel < GLIMMER_PROBE_VISIBILITY_TEXELS; texel++)
    {
        OutVisibility[probeIndex * GLIMMER_PROBE_VISIBILITY_TEXELS + texel] = float2(GLIMMER_PROBE_DEPTH_RANGE, GLIMMER_PROBE_DEPTH_RANGE * GLIMMER_PROBE_DEPTH_RANGE);
    }

    OutTrend[probeIndex] = (float4)0.0;
}

void GlimmerFreeSlot(uint slot)
{
    OutSlots[slot] = int4(0, 0, 0, -1);

    for (uint probe = 0; probe < GLIMMER_PROBES_PER_BLOCK; probe++)
    {
        OutStates[slot * GLIMMER_PROBES_PER_BLOCK + probe] = uint4(GlimmerPackProbeFlags(GLIMMER_PROBE_STATE_FREE, 0u, 0u, 0u), 0u, 0u, 0u);
    }
}

#if defined(MODE_ALLOC)

// a solid in the voxel around P that stands far enough off the ground to shape the light around it (roads and rocks lying on the
// ground don't). The voxels come from the occupancy cascade whose spacing matches the probes' (or a coarser one past its window),
// so a sample per probe cell sees every solid in it
bool GlimmerIsStandingSolid(float3 P, uint firstCascade)
{
    [loop]
    for (uint cascadeIndex = firstCascade; cascadeIndex < GLIMMER_SH_CASCADES; cascadeIndex++)
    {
        const GlimmerSHOccupancyCascade cascade = constants.occupancy.cascades[cascadeIndex];

        if (!GlimmerSHOccupancyContains(cascade, P))
        {
            continue;
        }

        const int3 voxel = int3(floor(P * cascade.params.y));

        if (glimmerSHOccupancy.Load(int4(GlimmerSHOccupancyTexel(cascadeIndex, voxel - cascade.origin.xyz), 0)).a <= 0.5)
        {
            return false;
        }

        float groundHeight;
        uint groundLevel;

        if (GlimmerSampleGround(constants.ground, P.xz, 0u, groundHeight, groundLevel))
        {
            const float voxelBottom = float(voxel.y) * cascade.params.x;

            return voxelBottom >= groundHeight + constants.params.y;
        }

        return true;
    }

    return false;
}

bool GlimmerIsBlockWanted(uint levelIndex, int3 block)
{
    const GlimmerProbeLevel level = constants.volume.levels[levelIndex];

    const float spacing = level.params.x;
    const float blockSize = spacing * float(GLIMMER_PROBE_BLOCK);

    const float3 blockMin = float3(block) * blockSize;

    // every level allocates around the solids on its own, even inside a finer level's window: the finer level only has blocks
    // right by the solids, and its probes can be buried or inside them, so the coarser one is what's left where it has nothing

    const float margin = constants.params.x * spacing;
    const int steps = GLIMMER_PROBE_BLOCK + 2 * int(ceil(constants.params.x));

    const float3 sampleMin = blockMin - margin + 0.5 * spacing;

    [loop]
    for (int z = 0; z < steps; z++)
    {
        [loop]
        for (int y = 0; y < steps; y++)
        {
            [loop]
            for (int x = 0; x < steps; x++)
            {
                if (GlimmerIsStandingSolid(sampleMin + float3(x, y, z) * spacing, levelIndex))
                {
                    return true;
                }
            }
        }
    }

    return false;
}

// every level's window cells, a few per thread
#define ALLOC_GROUP_SIZE 1024
#define ALLOC_CELLS (GLIMMER_PROBE_LEVELS * GLIMMER_PROBE_WINDOW_BLOCKS)
#define ALLOC_CELLS_PER_THREAD ((ALLOC_CELLS + ALLOC_GROUP_SIZE - 1) / ALLOC_GROUP_SIZE)
#define PRIORITY_BUCKETS (GLIMMER_PROBE_LEVELS * GLIMMER_PROBE_WINDOW)

groupshared uint gsFreeSlots[GLIMMER_PROBE_POOL_BLOCKS];
groupshared uint gsFreeCount;
groupshared uint gsBucketCounts[PRIORITY_BUCKETS];
groupshared uint gsBucketStarts[PRIORITY_BUCKETS];

[numthreads(ALLOC_GROUP_SIZE, 1, 1)]
void CSMain(uint groupIndex : SV_GroupIndex)
{
    if (groupIndex == 0u)
    {
        OutCounters[2] = OutCounters[1];
        OutCounters[0] = 0u;
        OutCounters[1] = 0u;
        OutCounters[3] = 0u;
        OutCounters[4] = 0u;

        gsFreeCount = 0u;
    }

    if (groupIndex < PRIORITY_BUCKETS)
    {
        gsBucketCounts[groupIndex] = 0u;
    }

    // blocks that scrolled out of their window give their slots back
    for (uint slotIndex = groupIndex; slotIndex < GLIMMER_PROBE_POOL_BLOCKS; slotIndex += ALLOC_GROUP_SIZE)
    {
        const int4 slot = OutSlots[slotIndex];

        if (slot.w >= 0 && (slot.w >= GLIMMER_PROBE_LEVELS || !GlimmerIsBlockInWindow(constants.volume.levels[slot.w], slot.xyz)))
        {
            GlimmerFreeSlot(slotIndex);
        }
    }

    AllMemoryBarrierWithGroupSync();

    int3 blocks[ALLOC_CELLS_PER_THREAD];
    uint tableIndices[ALLOC_CELLS_PER_THREAD];
    uint buckets[ALLOC_CELLS_PER_THREAD];
    uint ranksInBucket[ALLOC_CELLS_PER_THREAD];
    bool hasSlots[ALLOC_CELLS_PER_THREAD];
    bool needsSlots[ALLOC_CELLS_PER_THREAD];

    [unroll]
    for (uint cellOffset = 0; cellOffset < ALLOC_CELLS_PER_THREAD; cellOffset++)
    {
        const uint globalCell = groupIndex + cellOffset * ALLOC_GROUP_SIZE;

        blocks[cellOffset] = (int3)0;
        tableIndices[cellOffset] = 0u;
        buckets[cellOffset] = 0u;
        ranksInBucket[cellOffset] = 0u;
        hasSlots[cellOffset] = false;
        needsSlots[cellOffset] = false;

        if (globalCell >= ALLOC_CELLS)
        {
            continue;
        }

        const uint levelIndex = globalCell / GLIMMER_PROBE_WINDOW_BLOCKS;
        const uint cellIndex = globalCell % GLIMMER_PROBE_WINDOW_BLOCKS;

        const GlimmerProbeLevel level = constants.volume.levels[levelIndex];

        const int3 localBlock = int3(cellIndex % GLIMMER_PROBE_WINDOW, (cellIndex / GLIMMER_PROBE_WINDOW) % GLIMMER_PROBE_WINDOW, cellIndex / (GLIMMER_PROBE_WINDOW * GLIMMER_PROBE_WINDOW));
        const int3 block = level.windowOrigin.xyz + localBlock;

        const uint tableIndex = GlimmerProbeBlockTableIndex(levelIndex, block);
        const uint existingSlot = OutBlockTable[tableIndex];

        bool hasSlot = false;

        if (existingSlot < GLIMMER_PROBE_POOL_BLOCKS)
        {
            hasSlot = all(OutSlots[existingSlot] == int4(block, int(levelIndex)));
        }

        const bool isWanted = GlimmerIsBlockWanted(levelIndex, block);

        if (hasSlot)
        {
            if (isWanted)
            {
                OutSlotAges[existingSlot] = 0u;
            }
            else
            {
                const uint age = OutSlotAges[existingSlot] + 1u;
                OutSlotAges[existingSlot] = age;

                if (age > constants.budget.y)
                {
                    GlimmerFreeSlot(existingSlot);
                    hasSlot = false;
                }
            }
        }

        if (!hasSlot)
        {
            OutBlockTable[tableIndex] = GLIMMER_PROBE_NO_SLOT;
        }

        // nearest first, and a finer level before a coarser one: bucketed by level and ring of blocks around the viewer's
        const int3 viewerBlock = int3(floor(constants.viewer.xyz / (level.params.x * float(GLIMMER_PROBE_BLOCK))));
        const int3 ringDistance = abs(block - viewerBlock);
        const uint ring = min(uint(max(ringDistance.x, max(ringDistance.y, ringDistance.z))), GLIMMER_PROBE_WINDOW - 1u);

        blocks[cellOffset] = block;
        tableIndices[cellOffset] = tableIndex;
        buckets[cellOffset] = levelIndex * GLIMMER_PROBE_WINDOW + ring;
        hasSlots[cellOffset] = hasSlot;
        needsSlots[cellOffset] = isWanted && !hasSlot;

        if (needsSlots[cellOffset])
        {
            uint rankInBucket;
            InterlockedAdd(gsBucketCounts[buckets[cellOffset]], 1u, rankInBucket);

            ranksInBucket[cellOffset] = rankInBucket;
        }
    }

    AllMemoryBarrierWithGroupSync();

    for (uint freeSlotIndex = groupIndex; freeSlotIndex < GLIMMER_PROBE_POOL_BLOCKS; freeSlotIndex += ALLOC_GROUP_SIZE)
    {
        if (OutSlots[freeSlotIndex].w < 0)
        {
            uint freeIndex;
            InterlockedAdd(gsFreeCount, 1u, freeIndex);

            gsFreeSlots[freeIndex] = freeSlotIndex;
        }
    }

    if (groupIndex == 0u)
    {
        uint start = 0u;

        for (uint bucketIndex = 0; bucketIndex < PRIORITY_BUCKETS; bucketIndex++)
        {
            gsBucketStarts[bucketIndex] = start;
            start += gsBucketCounts[bucketIndex];
        }
    }

    GroupMemoryBarrierWithGroupSync();

    [unroll]
    for (uint allocOffset = 0; allocOffset < ALLOC_CELLS_PER_THREAD; allocOffset++)
    {
        const uint globalCell = groupIndex + allocOffset * ALLOC_GROUP_SIZE;

        if (globalCell >= ALLOC_CELLS)
        {
            continue;
        }

        bool hasSlot = hasSlots[allocOffset];

        if (needsSlots[allocOffset])
        {
            const uint rank = gsBucketStarts[buckets[allocOffset]] + ranksInBucket[allocOffset];

            if (rank < gsFreeCount)
            {
                const uint slot = gsFreeSlots[rank];

                OutSlots[slot] = int4(blocks[allocOffset], int(globalCell / GLIMMER_PROBE_WINDOW_BLOCKS));
                OutSlotAges[slot] = 0u;
                OutBlockTable[tableIndices[allocOffset]] = slot;

                for (uint probe = 0; probe < GLIMMER_PROBES_PER_BLOCK; probe++)
                {
                    GlimmerResetProbe(slot * GLIMMER_PROBES_PER_BLOCK + probe);
                }

                hasSlot = true;
            }
            else
            {
                InterlockedAdd(OutCounters[4], 1u);
            }
        }

        if (hasSlot)
        {
            InterlockedAdd(OutCounters[3], 1u);
        }
    }
}

#else // MODE_LIST

[numthreads(64, 1, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint probeIndex = dispatchThreadId.x;

    if (probeIndex >= GLIMMER_PROBE_POOL_PROBES)
    {
        return;
    }

    const int4 slot = OutSlots[probeIndex / GLIMMER_PROBES_PER_BLOCK];

    if (slot.w < 0 || slot.w >= GLIMMER_PROBE_LEVELS)
    {
        return;
    }

    uint4 state = OutStates[probeIndex];
    uint probeState = GlimmerProbeStateOf(state);

    const GlimmerProbeLevel level = constants.volume.levels[slot.w];
    const float spacing = level.params.x;
    const float3 gridPosition = GlimmerProbeGridPosition(level, GlimmerProbeCoord(slot.xyz, GlimmerLocalProbeOf(probeIndex)));

    float3 offset = GlimmerUnpackProbeOffset(state.y);

    float groundHeight;
    uint groundLevel;

    bool isBuried = false;

    if (GlimmerSampleGround(constants.ground, gridPosition.xz + offset.xz * spacing, 0u, groundHeight, groundLevel))
    {
        // how far up (in spacings) the probe has to sit to clear the ground
        const float lift = (groundHeight + GLIMMER_PROBE_BURIED_MARGIN - gridPosition.y) / spacing;

        if (lift > offset.y)
        {
            if (lift <= GLIMMER_PROBE_MAX_OFFSET)
            {
                // lifted just clear of the ground rather than lost, so the ground under its neighbours keeps a probe above it
                offset.y = min(lift + 0.01, GLIMMER_PROBE_MAX_OFFSET);
                state.y = GlimmerPackProbeOffset(offset);

                OutStates[probeIndex] = state;
            }
            else
            {
                isBuried = true;
            }
        }
    }

    if (isBuried != (probeState == GLIMMER_PROBE_STATE_BURIED))
    {
        // a probe that comes out of the ground (the ground under it streamed in or changed) starts over
        probeState = isBuried ? GLIMMER_PROBE_STATE_BURIED : GLIMMER_PROBE_STATE_ACTIVE;

        state.x = GlimmerPackProbeFlags(probeState, 0u, 0u, 0u);
        state.w = 0u;

        OutStates[probeIndex] = state;
    }

    if (probeState == GLIMMER_PROBE_STATE_BURIED)
    {
        return;
    }

    if (probeState == GLIMMER_PROBE_STATE_ACTIVE)
    {
        InterlockedAdd(OutCounters[1], 1u);
    }

    // every active probe gets its turn about every period frames, spread evenly by a hash of the probe
    const uint probesPerFrame = max(constants.budget.x, 1u);
    const uint period = max((OutCounters[2] + probesPerFrame - 1u) / probesPerFrame, 1u);
    const uint phase = constants.volume.info.z + GlimmerProbeHash(probeIndex);

    bool isDue;

    if (probeState == GLIMMER_PROBE_STATE_INSIDE)
    {
        isDue = phase % (period * max(constants.budget.z, 1u)) == 0u;
    }
    else
    {
        isDue = GlimmerProbeUpdates(state) == 0u || phase % period == 0u;
    }

    if (!isDue)
    {
        return;
    }

    uint listIndex;
    InterlockedAdd(OutCounters[0], 1u, listIndex);

    if (listIndex < constants.budget.x)
    {
        OutUpdateList[listIndex] = probeIndex;
    }
}

#endif
