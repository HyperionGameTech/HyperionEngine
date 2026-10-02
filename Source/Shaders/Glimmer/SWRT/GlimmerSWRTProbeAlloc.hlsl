#include "../../Include/Defines.hlsli"

PERMUTE(MODE, CLASSIFY, ALLOC, LIST)

#include "GlimmerSWRTCommon.hlsli"
#include "GlimmerProbeTypes.hlsli"

#define GLIMMER_SH_OCCUPANCY_NO_TRACE
#include "../SH/GlimmerSHOccupancy.hlsli"
#undef GLIMMER_SH_OCCUPANCY_NO_TRACE

struct GlimmerProbeAllocConstants
{
    GlimmerProbeVolume volume;
    GlimmerGroundParams ground;
    GlimmerSHOccupancyParams occupancy;
    uint4 budget;   // x = probes traced per frame, y = frames an unwanted block keeps its slot, z = updates between retries of probes inside solids, w = pool slots to use
    float4 params;  // x = block margin in spacings, y = how far above the ground a solid has to be to want probes around it, z = frames between looks at a block's solids, w = level classified
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
DECLARE_UAV(GlimmerSWRTProbeAlloc, OutCells) RWStructuredBuffer<int4> OutCells;
DECLARE_UAV(GlimmerSWRTProbeAlloc, OutSlots) RWStructuredBuffer<int4> OutSlots;         // xyz = block, w = level (-1 when free)
DECLARE_UAV(GlimmerSWRTProbeAlloc, OutSlotAges) RWStructuredBuffer<uint> OutSlotAges;   // frames since the block was last wanted
DECLARE_UAV(GlimmerSWRTProbeAlloc, OutStates) RWStructuredBuffer<uint4> OutStates;
DECLARE_UAV(GlimmerSWRTProbeAlloc, OutSH) RWStructuredBuffer<float4> OutSH;
DECLARE_UAV(GlimmerSWRTProbeAlloc, OutVisibility) RWStructuredBuffer<uint> OutVisibility;
DECLARE_UAV(GlimmerSWRTProbeAlloc, OutTrend) RWStructuredBuffer<float4> OutTrend;

// 0 = probes listed this frame
// 1 = active probes this frame
// 2 = active probes last frame
// 3 = blocks allocated,
// 4 = wanted blocks the pool had no slot for
DECLARE_UAV(GlimmerSWRTProbeAlloc, OutCounters) RWStructuredBuffer<uint> OutCounters;
DECLARE_UAV(GlimmerSWRTProbeAlloc, OutUpdateList) RWStructuredBuffer<uint> OutUpdateList;

#include "../GlimmerGround.hlsli"

#define GLIMMER_PROBES_NO_SAMPLING
#include "GlimmerProbes.hlsli"

/////////////////////////////
#define GLIMMER_PROBE_BURIED_MARGIN 0.25

#define GLIMMER_PROBE_CELL_CLASSIFIED 0x1 // xyz is the block the flags are for
#define GLIMMER_PROBE_CELL_SOLIDS 0x2     // solids stand around the block
#define GLIMMER_PROBE_CELL_WANTED 0x4     // no finer level covers them

// a probe checks for solids next to it every this many frames and as soon as it's placed
#define GLIMMER_PROBE_IDLE_PERIOD 16u
/////////////////////////////

// # pool slots in use
uint GlimmerPoolBlocks()
{
    return min(constants.budget.w, uint(GLIMMER_PROBE_POOL_BLOCKS));
}

void GlimmerResetProbe(uint probeIndex)
{
    OutStates[probeIndex] = uint4(GlimmerPackProbeFlags(GLIMMER_PROBE_STATE_ACTIVE, 0u, 0u, 0u), GlimmerPackProbeOffset((float3)0.0), 0u, 0u);

    [unroll]
    for (uint channel = 0; channel < 3; channel++)
    {
        OutSH[probeIndex * 3u + channel] = (float4)0.0;
    }

    // open in every direction until traced
    const uint open = GlimmerPackHalf2(float2(GLIMMER_PROBE_DEPTH_RANGE, GLIMMER_PROBE_DEPTH_RANGE * GLIMMER_PROBE_DEPTH_RANGE));

    for (uint texel = 0; texel < GLIMMER_PROBE_VISIBILITY_TEXELS; texel++)
    {
        OutVisibility[probeIndex * GLIMMER_PROBE_VISIBILITY_TEXELS + texel] = open;
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

bool GlimmerHasSlot(uint levelIndex, int3 block, uint tableIndex)
{
    const uint slot = OutBlockTable[tableIndex];

    return slot < GlimmerPoolBlocks() && all(OutSlots[slot] == int4(block, int(levelIndex)));
}

bool GlimmerIsSolidVoxel(float3 P, uint firstCascade, out int3 outVoxel, out float outSpacing)
{
    outVoxel = (int3)0;
    outSpacing = 0.0;

    [loop]
    for (uint cascadeIndex = firstCascade; cascadeIndex < GLIMMER_SH_CASCADES; cascadeIndex++)
    {
        const GlimmerSHOccupancyCascade cascade = constants.occupancy.cascades[cascadeIndex];

        if (!GlimmerSHOccupancyContains(cascade, P))
        {
            continue;
        }

        outVoxel = int3(floor(P * cascade.params.y));
        outSpacing = cascade.params.x;

        return glimmerSHOccupancy.Load(int4(GlimmerSHOccupancyTexel(cascadeIndex, outVoxel - cascade.origin.xyz), 0)).a > 0.5;
    }

    return false;
}

// a solid voxel that isn't just the ground's own bumps
bool GlimmerIsStandingSolid(float3 P, uint firstCascade)
{
    int3 voxel;
    float spacing;

    if (!GlimmerIsSolidVoxel(P, firstCascade, voxel, spacing))
    {
        return false;
    }

    float groundHeight;
    uint groundLevel;

    if (GlimmerSampleGround(constants.ground, P.xz, 0u, groundHeight, groundLevel))
    {
        return float(voxel.y) * spacing >= groundHeight + constants.params.y;
    }

    return true;
}

#if defined(MODE_CLASSIFY)

#define CLASSIFY_GROUP_SIZE 64

groupshared uint gsHasSolids;

bool GlimmerBlockHasSolids(uint levelIndex, int3 block, uint groupIndex)
{
    const float spacing = constants.volume.levels[levelIndex].params.x;

    const float margin = constants.params.x * spacing;
    const uint steps = uint(GLIMMER_PROBE_BLOCK + 2 * int(ceil(constants.params.x)));

    const float3 sampleMin = float3(block) * (spacing * float(GLIMMER_PROBE_BLOCK)) - margin + 0.5 * spacing;

    if (groupIndex == 0u)
    {
        gsHasSolids = 0u;
    }

    GroupMemoryBarrierWithGroupSync();

    [loop]
    for (uint sampleIndex = groupIndex; sampleIndex < steps * steps * steps && gsHasSolids == 0u; sampleIndex += CLASSIFY_GROUP_SIZE)
    {
        const uint3 step = uint3(sampleIndex % steps, (sampleIndex / steps) % steps, sampleIndex / (steps * steps));

        if (GlimmerIsStandingSolid(sampleMin + float3(step) * spacing, levelIndex))
        {
            gsHasSolids = 1u;

            break;
        }
    }

    GroupMemoryBarrierWithGroupSync();

    return gsHasSolids != 0u;
}

bool GlimmerIsCoveredByFinerLevel(uint levelIndex, int3 block)
{
    if (levelIndex == 0u)
    {
        return false;
    }

    const uint finerIndex = levelIndex - 1u;
    const GlimmerProbeLevel finer = constants.volume.levels[finerIndex];

    [unroll]
    for (uint child = 0; child < 8; child++)
    {
        const int3 finerBlock = block * 2 + int3(child & 1u, (child >> 1) & 1u, (child >> 2) & 1u);
        const int3 local = finerBlock - finer.windowOrigin.xyz;

        // lighting fades the finer level out over its window's outer blocks
        if (finer.windowOrigin.w == 0 || any(local < 2) || any(local >= GLIMMER_PROBE_WINDOW - 2))
        {
            return false;
        }

        const uint tableIndex = GlimmerProbeBlockTableIndex(finerIndex, finerBlock);
        const int4 cell = OutCells[tableIndex];

        if (any(cell.xyz != finerBlock) || (cell.w & GLIMMER_PROBE_CELL_CLASSIFIED) == 0)
        {
            return false;
        }

        if ((cell.w & GLIMMER_PROBE_CELL_WANTED) != 0 && !GlimmerHasSlot(finerIndex, finerBlock, tableIndex))
        {
            return false;
        }
    }

    return true;
}

[numthreads(CLASSIFY_GROUP_SIZE, 1, 1)]
void CSMain(uint3 groupId : SV_GroupID, uint groupIndex : SV_GroupIndex)
{
    const uint cellIndex = groupId.x;
    const uint levelIndex = uint(constants.params.w);

    if (cellIndex >= GLIMMER_PROBE_WINDOW_BLOCKS || levelIndex >= GLIMMER_PROBE_LEVELS)
    {
        return;
    }

    const GlimmerProbeLevel level = constants.volume.levels[levelIndex];

    const int3 localBlock = int3(cellIndex % GLIMMER_PROBE_WINDOW, (cellIndex / GLIMMER_PROBE_WINDOW) % GLIMMER_PROBE_WINDOW, cellIndex / (GLIMMER_PROBE_WINDOW * GLIMMER_PROBE_WINDOW));
    const int3 block = level.windowOrigin.xyz + localBlock;

    const uint tableIndex = GlimmerProbeBlockTableIndex(levelIndex, block);
    const int4 cell = OutCells[tableIndex];

    const bool isClassified = all(cell.xyz == block) && (cell.w & GLIMMER_PROBE_CELL_CLASSIFIED) != 0;

    bool hasSolids;

    if (!isClassified || (GlimmerProbeHash(tableIndex) + constants.volume.info.z) % max(uint(constants.params.z), 1u) == 0u)
    {
        hasSolids = GlimmerBlockHasSolids(levelIndex, block, groupIndex);
    }
    else
    {
        hasSolids = (cell.w & GLIMMER_PROBE_CELL_SOLIDS) != 0;
    }

    if (groupIndex != 0u)
    {
        return;
    }

    const bool isWanted = hasSolids && !GlimmerIsCoveredByFinerLevel(levelIndex, block);

    OutCells[tableIndex] = int4(block, GLIMMER_PROBE_CELL_CLASSIFIED | (hasSolids ? GLIMMER_PROBE_CELL_SOLIDS : 0) | (isWanted ? GLIMMER_PROBE_CELL_WANTED : 0));
}

#elif defined(MODE_ALLOC)

#define ALLOC_GROUP_SIZE 1024
#define ALLOC_CELLS (GLIMMER_PROBE_LEVELS * GLIMMER_PROBE_WINDOW_BLOCKS)

// blocks are ranked by distance to the viewer, in steps of PRIORITY_STEP metres, then by level
#define PRIORITY_STEP 8.0
#define PRIORITY_RINGS 65
#define PRIORITY_BUCKETS (PRIORITY_RINGS * GLIMMER_PROBE_LEVELS)

#define PRIORITY_HYSTERESIS 1u

groupshared uint gsFreeSlots[GLIMMER_PROBE_POOL_BLOCKS];
groupshared uint gsFreeCount;
groupshared uint gsDemand[PRIORITY_BUCKETS];        // wanted blocks, with a slot or not
groupshared uint gsPendingCounts[PRIORITY_BUCKETS]; // wanted blocks without a slot
groupshared uint gsPendingStarts[PRIORITY_BUCKETS];
groupshared uint gsPendingCursors[PRIORITY_BUCKETS];
groupshared uint gsCutoff;                          // the bucket the pool runs out in, PRIORITY_BUCKETS when every wanted block fits

struct GlimmerAllocCell
{
    uint levelIndex;
    int3 block;
    uint tableIndex;
};

GlimmerAllocCell GlimmerGetAllocCell(uint globalCell)
{
    GlimmerAllocCell cell;
    cell.levelIndex = globalCell / GLIMMER_PROBE_WINDOW_BLOCKS;

    const uint cellIndex = globalCell % GLIMMER_PROBE_WINDOW_BLOCKS;
    const int3 localBlock = int3(cellIndex % GLIMMER_PROBE_WINDOW, (cellIndex / GLIMMER_PROBE_WINDOW) % GLIMMER_PROBE_WINDOW, cellIndex / (GLIMMER_PROBE_WINDOW * GLIMMER_PROBE_WINDOW));

    cell.block = constants.volume.levels[cell.levelIndex].windowOrigin.xyz + localBlock;
    cell.tableIndex = GlimmerProbeBlockTableIndex(cell.levelIndex, cell.block);

    return cell;
}

bool GlimmerIsCellWanted(GlimmerAllocCell cell)
{
    const int4 flags = OutCells[cell.tableIndex];

    return all(flags.xyz == cell.block) && (flags.w & GLIMMER_PROBE_CELL_WANTED) != 0;
}

uint GlimmerPriorityBucket(GlimmerAllocCell cell)
{
    const float blockSize = constants.volume.levels[cell.levelIndex].params.x * float(GLIMMER_PROBE_BLOCK);

    const float3 blockMin = float3(cell.block) * blockSize;
    const float3 toBlock = max(max(blockMin - constants.viewer.xyz, constants.viewer.xyz - (blockMin + blockSize)), 0.0);
    const uint ring = min(uint(length(toBlock) / PRIORITY_STEP), PRIORITY_RINGS - 1u);

    return ring * GLIMMER_PROBE_LEVELS + cell.levelIndex;
}

[numthreads(ALLOC_GROUP_SIZE, 1, 1)]
void CSMain(uint groupIndex : SV_GroupIndex)
{
    const uint poolBlocks = GlimmerPoolBlocks();

    if (groupIndex == 0u)
    {
        OutCounters[2] = OutCounters[1];
        OutCounters[0] = 0u;
        OutCounters[1] = 0u;
        OutCounters[3] = 0u;
        OutCounters[4] = 0u;

        gsFreeCount = 0u;
    }

    for (uint bucketIndex = groupIndex; bucketIndex < PRIORITY_BUCKETS; bucketIndex += ALLOC_GROUP_SIZE)
    {
        gsDemand[bucketIndex] = 0u;
        gsPendingCounts[bucketIndex] = 0u;
        gsPendingCursors[bucketIndex] = 0u;
    }

    // blocks that scrolled out of their window give their slots back, as do slots past the pool size when it shrinks
    for (uint slotIndex = groupIndex; slotIndex < GLIMMER_PROBE_POOL_BLOCKS; slotIndex += ALLOC_GROUP_SIZE)
    {
        const int4 slot = OutSlots[slotIndex];

        if (slot.w >= 0 && (slotIndex >= poolBlocks || slot.w >= GLIMMER_PROBE_LEVELS || !GlimmerIsBlockInWindow(constants.volume.levels[slot.w], slot.xyz)))
        {
            GlimmerFreeSlot(slotIndex);
        }
    }

    AllMemoryBarrierWithGroupSync();

    // blocks with a slot age while unwanted and give it back after a while; every wanted block counts toward the demand of its bucket
    for (uint globalCell = groupIndex; globalCell < ALLOC_CELLS; globalCell += ALLOC_GROUP_SIZE)
    {
        const GlimmerAllocCell cell = GlimmerGetAllocCell(globalCell);
        const bool isWanted = GlimmerIsCellWanted(cell);

        bool hasSlot = GlimmerHasSlot(cell.levelIndex, cell.block, cell.tableIndex);

        if (hasSlot)
        {
            const uint slot = OutBlockTable[cell.tableIndex];

            if (isWanted)
            {
                OutSlotAges[slot] = 0u;
            }
            else
            {
                const uint age = OutSlotAges[slot] + 1u;
                OutSlotAges[slot] = age;

                if (age > constants.budget.y)
                {
                    GlimmerFreeSlot(slot);
                    hasSlot = false;
                }
            }
        }

        if (!hasSlot)
        {
            OutBlockTable[cell.tableIndex] = GLIMMER_PROBE_NO_SLOT;
        }

        if (isWanted)
        {
            const uint bucket = GlimmerPriorityBucket(cell);

            InterlockedAdd(gsDemand[bucket], 1u);

            if (!hasSlot)
            {
                InterlockedAdd(gsPendingCounts[bucket], 1u);
            }
        }
    }

    AllMemoryBarrierWithGroupSync();

    if (groupIndex == 0u)
    {
        uint cutoff = PRIORITY_BUCKETS;
        uint demand = 0u;
        uint pendingStart = 0u;

        for (uint bucketIndex = 0; bucketIndex < PRIORITY_BUCKETS; bucketIndex++)
        {
            demand += gsDemand[bucketIndex];

            if (cutoff == PRIORITY_BUCKETS && demand > poolBlocks)
            {
                cutoff = bucketIndex;
            }

            gsPendingStarts[bucketIndex] = pendingStart;
            pendingStart += gsPendingCounts[bucketIndex];
        }

        gsCutoff = cutoff;
    }

    AllMemoryBarrierWithGroupSync();

    const uint cutoff = gsCutoff;

    // when the pool can't fit every wanted block, the ones well past the cutoff give their slots to nearer ones
    // and blocks nobody wants any more don't wait to age out (sad)
    if (cutoff < PRIORITY_BUCKETS)
    {
        for (uint evictCell = groupIndex; evictCell < ALLOC_CELLS; evictCell += ALLOC_GROUP_SIZE)
        {
            const GlimmerAllocCell cell = GlimmerGetAllocCell(evictCell);
            const uint slot = OutBlockTable[cell.tableIndex];

            if (slot == GLIMMER_PROBE_NO_SLOT)
            {
                continue;
            }

            if (!GlimmerIsCellWanted(cell) || GlimmerPriorityBucket(cell) > cutoff + PRIORITY_HYSTERESIS)
            {
                GlimmerFreeSlot(slot);

                OutBlockTable[cell.tableIndex] = GLIMMER_PROBE_NO_SLOT;
            }
        }
    }

    AllMemoryBarrierWithGroupSync();

    for (uint freeSlotIndex = groupIndex; freeSlotIndex < poolBlocks; freeSlotIndex += ALLOC_GROUP_SIZE)
    {
        if (OutSlots[freeSlotIndex].w < 0)
        {
            uint freeIndex;
            InterlockedAdd(gsFreeCount, 1u, freeIndex);

            gsFreeSlots[freeIndex] = freeSlotIndex;
        }
    }

    AllMemoryBarrierWithGroupSync();

    // the wanted blocks that had no slot before the evictions, ranked in priority order, within a bucket in whatever order they come
    // the ones just evicted sit past the cutoff, where nothing is handed out
    for (uint allocCell = groupIndex; allocCell < ALLOC_CELLS; allocCell += ALLOC_GROUP_SIZE)
    {
        const GlimmerAllocCell cell = GlimmerGetAllocCell(allocCell);

        if (OutBlockTable[cell.tableIndex] != GLIMMER_PROBE_NO_SLOT)
        {
            InterlockedAdd(OutCounters[3], 1u);

            continue;
        }

        if (!GlimmerIsCellWanted(cell))
        {
            continue;
        }

        const uint bucket = GlimmerPriorityBucket(cell);

        if (bucket > cutoff + PRIORITY_HYSTERESIS)
        {
            InterlockedAdd(OutCounters[4], 1u);

            continue;
        }

        uint rankInBucket;
        InterlockedAdd(gsPendingCursors[bucket], 1u, rankInBucket);

        const uint rank = gsPendingStarts[bucket] + rankInBucket;

        if (rank >= gsFreeCount)
        {
            InterlockedAdd(OutCounters[4], 1u);

            continue;
        }

        const uint slot = gsFreeSlots[rank];

        OutSlots[slot] = int4(cell.block, int(cell.levelIndex));
        OutSlotAges[slot] = 0u;
        OutBlockTable[cell.tableIndex] = slot;

        for (uint probe = 0; probe < GLIMMER_PROBES_PER_BLOCK; probe++)
        {
            OutStates[slot * GLIMMER_PROBES_PER_BLOCK + probe] = uint4(GlimmerPackProbeFlags(GLIMMER_PROBE_STATE_RESET, 0u, 0u, 0u), 0u, 0u, 0u);
        }

        InterlockedAdd(OutCounters[3], 1u);
    }
}

#else // MODE_LIST

bool GlimmerHasSolidNextToProbe(uint levelIndex, float3 gridPosition, float spacing)
{
    const int radius = clamp(int(ceil(constants.params.x)), 1, 2);

    [loop]
    for (int z = -radius; z <= radius; z++)
    {
        [loop]
        for (int y = -radius; y <= radius; y++)
        {
            [loop]
            for (int x = -radius; x <= radius; x++)
            {
                if (GlimmerIsStandingSolid(gridPosition + float3(x, y, z) * spacing, levelIndex))
                {
                    return true;
                }
            }
        }
    }

    return false;
}

[numthreads(64, 1, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint probeIndex = dispatchThreadId.x;

    if (probeIndex >= GlimmerPoolBlocks() * GLIMMER_PROBES_PER_BLOCK)
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

    if (probeState == GLIMMER_PROBE_STATE_RESET)
    {
        GlimmerResetProbe(probeIndex);

        state = OutStates[probeIndex];
        probeState = GlimmerProbeStateOf(state);
    }

    const GlimmerProbeLevel level = constants.volume.levels[slot.w];
    const float spacing = level.params.x;
    const float3 gridPosition = GlimmerProbeGridPosition(level, GlimmerProbeCoord(slot.xyz, GlimmerLocalProbeOf(probeIndex)));

    float3 offset = GlimmerUnpackProbeOffset(state.y);

    float groundHeight;
    uint groundLevel;

    bool isBuried = false;
    float groundLift = 0.0;

    if (GlimmerSampleGround(constants.ground, gridPosition.xz + offset.xz * spacing, 0u, groundHeight, groundLevel))
    {
        const float lift = (groundHeight + GLIMMER_PROBE_BURIED_MARGIN - gridPosition.y) / spacing;

        groundLift = clamp(lift + 0.01, 0.0, GLIMMER_PROBE_MAX_OFFSET);

        if (lift > offset.y)
        {
            if (lift <= GLIMMER_PROBE_MAX_OFFSET)
            {
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
        probeState = select(isBuried, GLIMMER_PROBE_STATE_BURIED, GLIMMER_PROBE_STATE_ACTIVE);

        state.x = GlimmerPackProbeFlags(probeState, 0u, 0u, 0u);
        state.w = 0u;

        OutStates[probeIndex] = state;
    }

    if (probeState == GLIMMER_PROBE_STATE_BURIED)
    {
        return;
    }

    const uint phase = constants.volume.info.z + GlimmerProbeHash(probeIndex);

    if (probeState == GLIMMER_PROBE_STATE_IDLE || probeState == GLIMMER_PROBE_STATE_ACTIVE)
    {
        const bool isPlaced = probeState == GLIMMER_PROBE_STATE_ACTIVE && GlimmerProbeUpdates(state) == 0u;

        if (isPlaced || phase % GLIMMER_PROBE_IDLE_PERIOD == 0u)
        {
            const bool isIdle = !GlimmerHasSolidNextToProbe(uint(slot.w), gridPosition, spacing);

            if (isIdle != (probeState == GLIMMER_PROBE_STATE_IDLE))
            {
                probeState = select(isIdle, GLIMMER_PROBE_STATE_IDLE, GLIMMER_PROBE_STATE_ACTIVE);

                state.x = GlimmerPackProbeFlags(probeState, GlimmerProbeRelocations(state), 0u, 0u);

                OutStates[probeIndex] = state;
            }
        }
    }

    if (probeState == GLIMMER_PROBE_STATE_IDLE)
    {
        return;
    }

    if (probeState == GLIMMER_PROBE_STATE_ACTIVE)
    {
        InterlockedAdd(OutCounters[1], 1u);
    }

    const uint probesPerFrame = max(constants.budget.x, 1u);
    const uint period = max((OutCounters[2] + probesPerFrame - 1u) / probesPerFrame, 1u);

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

    // a probe stuck inside is checked again from where it last was clear (or its grid point) and stays out of the lighting until it's
    // clear there; what moved it in may be gone, and from inside the solid it could never tell
    if (probeState == GLIMMER_PROBE_STATE_INSIDE)
    {
        float3 retryOffset;

        if (!GlimmerUnpackGoodOffset(state.w, retryOffset))
        {
            retryOffset = (float3)0.0;
        }

        retryOffset.y = max(retryOffset.y, groundLift);

        state.y = GlimmerPackProbeOffset(retryOffset);

        OutStates[probeIndex] = state;
    }

    uint listIndex;
    InterlockedAdd(OutCounters[0], 1u, listIndex);

    if (listIndex < constants.budget.x)
    {
        OutUpdateList[listIndex] = probeIndex;
    }
}

#endif
