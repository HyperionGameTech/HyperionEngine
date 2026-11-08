#include "../../Include/Defines.hlsli"

PERMUTE(MODE, REQUESTS, RESOLVE, FINISH, RAYS, HISTORY, SPREAD, WHOLE_LIST, WHOLE_RAYS)

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

DECLARE_SRV(GlimmerBrickReflections, GBufferNormalsTexture) Texture2D GBufferNormalsTexture;
DECLARE_SRV(GlimmerBrickReflections, GBufferDepthTexture) Texture2D GBufferDepthTexture;
DECLARE_SRV(GlimmerBrickReflections, GBufferMaterialTexture) Texture2D<uint> GBufferMaterialTexture;

#include "../../Include/Gbuffer.hlsli"
#include "../../Include/Scene.hlsli"

DECLARE_SRV_DYNAMIC(GlimmerBrickReflections, CamerasBuffer) StructuredBuffer<Camera> _cameras_buffer;
#define camera _cameras_buffer[0]

DECLARE_SRV(GlimmerBrickReflections, GlimmerBrickTableBuffer) StructuredBuffer<uint> glimmerBrickTable;
DECLARE_SRV(GlimmerBrickReflections, GlimmerBrickIndexBuffer) StructuredBuffer<uint> glimmerBrickIndex;
DECLARE_SRV(GlimmerBrickReflections, GlimmerBrickCrossingsBuffer) StructuredBuffer<uint> glimmerBrickCrossings;
DECLARE_SRV(GlimmerBrickReflections, GlimmerBrickMaterialBuffer) StructuredBuffer<uint> glimmerBrickMaterial;
DECLARE_SRV(GlimmerBrickReflections, GlimmerBrickRadianceBuffer) StructuredBuffer<uint> glimmerBrickRadiance;
DECLARE_SRV(GlimmerBrickReflections, GlimmerBrickCrossingLightBuffer) StructuredBuffer<uint> glimmerBrickCrossingLight;

#define GLIMMER_BRICKS_WITH_LIGHT
#include "GlimmerBricks.hlsli"

struct GlimmerBrickReflectionsConstants
{
    GlimmerBrickParams bricks;

    /// xy = the view's dimensions
    /// zw = the half resolution picture's dimensions
    uint4 dimensions;

    /// x = frame
    /// y = frames a pixel that reads slots adds up
    /// z = frames one that traces for itself adds up
    uint4 frame;

    /// x = crossings
    /// y = blocks
    /// z = requests per frame max
    /// w = 1 when there is no last frame to read.
    uint4 limits;
};

DECLARE_BUFFER_DYNAMIC(GlimmerBrickReflections, CBuffer) cbuffer CBuffer
{
    GlimmerBrickReflectionsConstants constants;
};

// ten words a block: its crossing, a hash of that crossing, then four slots of two: what the ray hit, and its key with the ray's length as a half above it
DECLARE_UAV(GlimmerBrickReflections, SlotBlocksBuffer) RWStructuredBuffer<uint> slotBlocks;
DECLARE_UAV(GlimmerBrickReflections, SlotClaimsBuffer) RWStructuredBuffer<uint> slotClaims;
// four words a request: the crossing, which bins, its brick and its column
DECLARE_UAV(GlimmerBrickReflections, SlotRequestsBuffer) RWStructuredBuffer<uint> slotRequests;
// what each resolved pixel found missing, four words as a request has them; after the finish, the pixels of the view that get a ray after the spread
DECLARE_UAV(GlimmerBrickReflections, SlotWantedBuffer) RWStructuredBuffer<uint> slotWanted;
DECLARE_UAV(GlimmerBrickReflections, SlotListBuffer) RWStructuredBuffer<uint> slotList;
#define slotWhole slotWanted
DECLARE_UAV(GlimmerBrickReflections, SlotCountsBuffer) RWStructuredBuffer<uint> slotCounts;

// rgb = radiance times a, a = how much of the pixel's reflection is the bricks'
DECLARE_UAV(GlimmerBrickReflections, OutColor) RWTexture2D<float4> OutColor;
// x = how far the pixel's rays went, less than 0 where they left the bricks, REACH_HOLE for a pixel with no ray this frame; y = frames added up
DECLARE_UAV(GlimmerBrickReflections, OutInfo) RWTexture2D<float4> OutInfo;
// xyz = position, w = roughness with GUIDE_SAMPLED or GUIDE_DITHERED added for how its light was come by, less than 0 where there is no surface
DECLARE_UAV(GlimmerBrickReflections, OutGuide) RWTexture2D<float4> OutGuide;

DECLARE_SRV(GlimmerBrickReflections, ColorTexture) Texture2D ColorTexture;
DECLARE_SRV(GlimmerBrickReflections, InfoTexture) Texture2D InfoTexture;
DECLARE_SRV(GlimmerBrickReflections, GuideTexture) Texture2D GuideTexture;
DECLARE_SRV(GlimmerBrickReflections, HistoryColorTexture) Texture2D HistoryColorTexture;
DECLARE_SRV(GlimmerBrickReflections, HistoryInfoTexture) Texture2D HistoryInfoTexture;
DECLARE_SRV(GlimmerBrickReflections, HistoryGuideTexture) Texture2D HistoryGuideTexture;

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#define COUNT_REQUESTS 0
#define COUNT_LISTED 1
#define COUNT_WHOLE 3
// NOTE must match GlimmerBrickReflections.cpp
#define NUM_COUNTS 4

// alphas a bin is wide, and that a card is read over along a drawn direction
#define LOBE_ALPHAS 3.0
#define AIM_ALPHAS 2.0
#define RAY_DISTANCE 500.0
// how many times what it has added up a sample may be
#define FIREFLIES 5.0
// columns a moving dither reaches
#define DITHER_REACH_COLUMNS 2.0

#define BLOCK_WORDS 10u
#define BLOCK_NONE 0xFFFFFFFFu
#define REQUEST_WORDS 4u
#define TRACE_ROW 256u

#define SLOT_SKY 0xFFFFFFFFu
#define SLOT_BELOW 0xFFFFFFFEu

#define WANT_NOTHING 0xFFFFFFFFu
#define WANT_RAY 0xFFFFFFFEu
#define WANT_FILL 0xFFFFFFFDu

#define WANT_RAY_SAMPLED 0xFFFFFFFCu
#define WANT_BLANK (1u << 27u)

#define LIST_SINGLE 0x80000000u
#define LIST_SAMPLED 0x40000000u

#define REACH_HOLE -2.0
#define GUIDE_SAMPLED 2.0
#define GUIDE_DITHERED 4.0

// the octahedral grid's bins are this many radians wide at level 0
#define BIN_ANGLE 3.5449

#define MIN_LEVEL 2
#define MAX_LEVEL 6

// the level from which a bin's rays go along the middle of it and not from somewhere in it of each crossing's own
#define SCATTER_BELOW_LEVEL 6
// bins beyond its crossing's own that a pixel's direction may fall and still be read from them
#define STRAY 0.5
// the cosine of how far a pixel's surface may face from its crossing and be flat
#define CURVED_FACING 0.9994
// the roughness from which a pixel that traces for itself places its ray by chance
#define SAMPLED_FROM 0.2
#define SPREAD_DISC 2.0
// close to something flat and polished the way a crossing's middle reflects the eye moves on by a bin or more a column, and which pixels
// stray from its bins goes by where they fall: a pattern of diamonds. Past NEAR_FROM of a bin a column a pixel traces for itself, all of
// them by NEAR_TO, its ray placed over the lobe as from SAMPLED_FROM up
#define NEAR_FROM 0.6
#define NEAR_TO 0.8
// the least of a pixel's view worth a read, and the least of it the cards its slots name have to cover for those to be its picture
#define LAYER_LEAST 0.02
#define LAYER_ENOUGH 0.5

#define SAME_SURFACE 0.01
#define VIRTUAL_BELOW 0.3
#define SURFACE_ABOVE 0.6
#define MISS_DISTANCE 100.0
#define ROUGHNESS_TOLERANCE 0.05
#define SLIDE_FORGETS 1.0

#define SPREAD_PLANE_TOLERANCE 0.01

#include "GlimmerBrickReflectionsSurface.hlsli"
#include "../../Include/Noise.hlsli"

uint PixelSeed(uint2 texel)
{
    return pcg_hash(texel.x + pcg_hash(texel.y));
}

float UnitFloat(uint value)
{
    return float(value >> 8u) * (1.0 / 16777216.0);
}

void MakeBasis(float3 N, out float3 T, out float3 B)
{
    const float s = N.z >= 0.0 ? 1.0 : -1.0;
    const float a = -1.0 / (s + N.z);
    const float b = N.x * N.y * a;

    T = float3(1.0 + s * N.x * N.x * a, s * b, -s * N.x);
    B = float3(b, s + N.y * N.y * a, -N.y);
}

// a direction on the octahedral grid, 0 to 1 each way, with straight up in the middle
float2 DirectionPlace(float3 direction)
{
    const float3 n = direction.xzy / (abs(direction.x) + abs(direction.y) + abs(direction.z));

    float2 e = n.xy;

    if (n.z < 0.0)
    {
        e = (1.0 - abs(n.yx)) * select(n.xy >= 0.0, 1.0, -1.0);
    }

    return e * 0.5 + 0.5;
}

float3 PlaceDirection(float2 place)
{
    const float2 e = place * 2.0 - 1.0;

    float3 n = float3(e, 1.0 - abs(e.x) - abs(e.y));

    const float t = max(-n.z, 0.0);
    n.xy += select(n.xy >= 0.0, -t, t);

    return normalize(n).xzy;
}

uint LevelOf(float alpha)
{
    return uint(clamp(round(log2(BIN_ANGLE / max(LOBE_ALPHAS * alpha, 1e-4))), float(MIN_LEVEL), float(MAX_LEVEL)));
}

// a bin's place among the bins of every level up to its own, counting from 1
uint BinKey(uint level, int2 bin)
{
    const uint bins = 1u << level;

    return 1u + (bins * bins - 1u) / 3u + uint(bin.y) * bins + uint(bin.x);
}

// the way a bin's ray goes from a crossing: somewhere in the bin of the crossing's own, the same whenever it is asked
float3 BinDirection(uint level, int2 bin, uint crossing)
{
    float2 within = float2(0.5, 0.5);

    if (level < uint(SCATTER_BELOW_LEVEL))
    {
        const uint own = pcg_hash(crossing ^ pcg_hash(BinKey(level, bin)));

        within = float2(float(own & 0xFFFFu), float(own >> 16u)) / 65536.0;
    }

    return PlaceDirection((float2(bin) + within) / float(1u << level));
}

uint CrossingHash(uint crossing)
{
    return pcg_hash(glimmerBrickCrossings[2u * crossing] ^ pcg_hash(glimmerBrickCrossings[2u * crossing + 1u] ^ crossing)) | 1u;
}

// a crossing's block is one of two, picked by its hash
void BlockChoices(uint hash, out uint outFirst, out uint outSecond)
{
    outFirst = (hash >> 1u) % constants.limits.y;
    outSecond = pcg_hash(hash) % constants.limits.y;
}

bool IsBlockOf(uint block, uint crossing, uint hash)
{
    return slotBlocks[block * BLOCK_WORDS] == crossing && slotBlocks[block * BLOCK_WORDS + 1u] == hash;
}

uint FindBlock(uint crossing, uint hash)
{
    uint first;
    uint second;
    BlockChoices(hash, first, second);

    return IsBlockOf(first, crossing, hash) ? first : (IsBlockOf(second, crossing, hash) ? second : BLOCK_NONE);
}

uint BrickSerial(uint brick)
{
    return glimmerBrickIndex[GlimmerBrickWord(brick, GLIMMER_BRICK_INDEX_SERIAL)] & GLIMMER_BRICK_SERIAL_MASK;
}

// a slot's card carries the serial its brick had, so a slot whose brick was built again since reads as empty and is traced again
uint PackSlotCard(uint brick, uint cardSlot)
{
    return (brick * uint(GLIMMER_BRICK_CARDS) + cardSlot) | (BrickSerial(brick) << GLIMMER_BRICK_CARD_BITS);
}

bool UnpackSlotCard(uint packed, out uint outCard)
{
    outCard = packed;

    if (packed == SLOT_SKY || packed == SLOT_BELOW)
    {
        return true;
    }

    outCard = packed & ((1u << GLIMMER_BRICK_CARD_BITS) - 1u);

    const uint brick = outCard / uint(GLIMMER_BRICK_CARDS);

    return glimmerBrickIndex[GlimmerBrickWord(brick, GLIMMER_BRICK_INDEX_AXIS)] <= 2u
        && BrickSerial(brick) == (packed >> GLIMMER_BRICK_CARD_BITS);
}

uint SlotWord(uint block, uint slot)
{
    return block * BLOCK_WORDS + 2u + 2u * slot;
}

// a block's slots, each left empty where its brick has been built again since: the cards as packed, or unpacked when isUnpacked
void LoadBlock(uint block, bool isUnpacked, out uint outCards[4], out uint outKeys[4])
{
    for (uint slot = 0; slot < 4u; slot++)
    {
        const uint packed = slotBlocks[SlotWord(block, slot)];

        uint card;

        outCards[slot] = 0u;
        outKeys[slot] = 0u;

        if (UnpackSlotCard(packed, card))
        {
            outCards[slot] = isUnpacked ? card : packed;
            outKeys[slot] = slotBlocks[SlotWord(block, slot) + 1u];
        }
    }
}

// which slot holds a bin's key, 4 when none does
uint HeldSlot(uint keys[4], uint key)
{
    uint held = 4u;

    for (uint slot = 0; slot < 4u; slot++)
    {
        held = (keys[slot] & 0xFFFFu) == key ? slot : held;
    }

    return held;
}

/// rgb = radiance times how much of the patch is surface, a = that share
float4 CardLight(
    int3 cell,
    uint axis,
    uint cardSlot,
    float3 position,
    float3 normal,
    float footprint)
{
    const float width = constants.bricks.params.z;

    if (footprint >= float(1u << uint(GLIMMER_BRICK_CARD_FIRST_MIP)) * width * 0.75)
    {
        const float4 light = GlimmerSampleBrickLight(constants.bricks, cell, axis, cardSlot, position, normal, footprint);

        return light.a < 1e-3 ? (float4)0.0 : light;
    }
    
    const float3 cellLow = GlimmerBrickCellLow(constants.bricks, cell);
    const float2 across = GlimmerBrickAcross(axis, position - cellLow) * constants.bricks.params.w;
    const int2 beside = int2(floor(across / float(GLIMMER_BRICK_SIDE)));
    const int3 otherCell = cell + int3(GlimmerBrickPoint(axis, float2(beside), 0.0));

    if (!GlimmerBrickCellInWindow(constants.bricks, otherCell))
    {
        return (float4)0.0;
    }

    const uint brick = glimmerBrickTable[GlimmerBrickTableIndex(otherCell, axis)];

    if (brick == GLIMMER_BRICK_NONE)
    {
        return (float4)0.0;
    }

    const int2 column = clamp(int2(floor(across)) - beside * GLIMMER_BRICK_SIDE, int2(0, 0), int2(GLIMMER_BRICK_SIDE - 1, GLIMMER_BRICK_SIDE - 1));

    uint first;
    const uint count = GlimmerLoadBrickColumn(brick, column, first);

    const float depthAt = GlimmerBrickAlong(axis, position - cellLow);

    uint nearest = GLIMMER_BRICK_NONE;
    float nearestApart = 4.0 * width;

    for (uint k = 0; k < count; k++)
    {
        const GlimmerBrickCrossing crossing = GlimmerLoadBrickCrossing(constants.bricks, first + k);

        if (GlimmerBrickCardSlot(constants.bricks, axis, crossing.normal, crossing.depth) == cardSlot && abs(crossing.depth - depthAt) < nearestApart)
        {
            nearestApart = abs(crossing.depth - depthAt);
            nearest = first + k;
        }
    }

    if (nearest == GLIMMER_BRICK_NONE)
    {
        return (float4)0.0;
    }

    const float4 light = GlimmerBrickUnpackLight(glimmerBrickCrossingLight[nearest]);

    return light.a > 0.0 ? float4(light.rgb, 1.0) : (float4)0.0;
}

float4 HitLight(GlimmerBrickHit hit, float3 position, float footprint)
{
    int3 cell = int3(0, 0, 0);
    uint axis = 0u;

    if (!GlimmerLoadBrickPlace(hit.brick, cell, axis))
    {
        return (float4)0.0;
    }

    return CardLight(cell, axis, GlimmerBrickCardSlot(constants.bricks, axis, hit.normal, hit.depth), position, hit.normal, footprint);
}

// rgb = radiance times a, a = how much of the pixel's reflection a ray found in the bricks; outT is how far it went, less than 0 where it left them
float4 TraceReflection(float3 origin, float3 direction, float angle, float roughness, out float outT)
{
    GlimmerBrickHit hit;
    float4 light = (float4)0.0;

    outT = -1.0;

    if (TraceGlimmerBricks(constants.bricks, origin, direction, 0.0, RAY_DISTANCE, hit))
    {
        outT = hit.t;

        if (dot(hit.normal, direction) < 0.0)
        {
            light = HitLight(hit, origin + direction * hit.t, angle * hit.t);
        }
    }

    const float coverage = light.a > 0.0 ? 1.0 - MatteShare(roughness) : 0.0;

    return float4(light.rgb / max(light.a, 1e-6) * coverage, coverage);
}

// the half vector of a view's visible normals (Heitz 2018), in the frame of the normal
float3 SampleVisibleNormal(float3 view, float alpha, float2 u)
{
    const float3 stretched = normalize(float3(alpha * view.x, alpha * view.y, view.z));
    const float lengthSquared = stretched.x * stretched.x + stretched.y * stretched.y;

    const float3 t1 = lengthSquared > 0.0 ? float3(-stretched.y, stretched.x, 0.0) * rsqrt(lengthSquared) : float3(1.0, 0.0, 0.0);
    const float3 t2 = cross(stretched, t1);

    const float r = sqrt(u.x);
    const float phi = 2.0 * HYP_FMATH_PI * u.y;
    const float p1 = r * cos(phi);
    const float s = 0.5 * (1.0 + stretched.z);
    const float p2 = (1.0 - s) * sqrt(1.0 - p1 * p1) + s * r * sin(phi);

    const float3 h = p1 * t1 + p2 * t2 + sqrt(max(0.0, 1.0 - p1 * p1 - p2 * p2)) * stretched;

    return normalize(float3(alpha * h.x, alpha * h.y, max(0.0, h.z)));
}

/// gets the way a pixel that reads slots looks this frame, and the angle its cards are read over along it, when it has one.
/// drawn from its lobe, from a start of its own and a step a frame
/// the same whoever asks in one frame, so that a ray traced for the pixel later goes the way its cards were read
void LobeAim(
    uint2 texel,
    ReflectionSurface surface,
    float3 surfaceNormal,
    inout float3 ioAim,
    inout float ioAngle)
{
    if (constants.frame.y == 0u)
    {
        return;
    }

    const uint own = PixelSeed(texel);
    const uint2 stepped = uint2(pcg_hash(own ^ 0x51ED270Bu), pcg_hash(own ^ 0x2545F491u)) + constants.frame.x * uint2(2700274805u, 2313257605u);

    float3 T;
    float3 B;
    MakeBasis(surface.N, T, B);

    const float3 V = ViewOf(surface);
    const float3 localView = float3(dot(V, T), dot(V, B), dot(V, surface.N));

    if (localView.z <= 0.0)
    {
        return;
    }

    const float3 h = SampleVisibleNormal(localView, surface.alpha, float2(UnitFloat(stepped.x), UnitFloat(stepped.y)));
    const float3 localDirection = reflect(-localView, h);

    if (localDirection.z <= 0.0)
    {
        return;
    }

    ioAim = AboveHorizon(T * localDirection.x + B * localDirection.y + surface.N * localDirection.z, surfaceNormal);
    ioAngle = AIM_ALPHAS * surface.alpha * max(dot(surface.N, V), 0.1);
}

// up to a square root of two either way, another each frame: a pixel then reads each of the two mips its width lies between
float BetweenMips(uint own)
{
    return exp2(UnitFloat(own + constants.frame.x * 2654435769u) - 0.5);
}

#if defined(MODE_REQUESTS)

void TraceRequest(uint index)
{
    if (index >= min(slotCounts[COUNT_REQUESTS], constants.limits.z))
    {
        return;
    }

    const uint crossingIndex = slotRequests[REQUEST_WORDS * index];
    const uint code = slotRequests[REQUEST_WORDS * index + 1u];
    const uint brick = slotRequests[REQUEST_WORDS * index + 2u];
    const uint columnIndex = slotRequests[REQUEST_WORDS * index + 3u];

    int3 cell = int3(0, 0, 0);
    uint axis = 0u;

    if (crossingIndex >= constants.limits.x || !GlimmerLoadBrickPlace(brick, cell, axis))
    {
        return;
    }

    const uint level = code >> 28u;
    const int2 base = int2(int(code & 0x1FFFu), int((code >> 13u) & 0x1FFFu));

    const int2 column = int2(columnIndex % uint(GLIMMER_BRICK_SIDE), columnIndex / uint(GLIMMER_BRICK_SIDE));
    const GlimmerBrickCrossing crossing = GlimmerLoadBrickCrossing(constants.bricks, crossingIndex);

    const float3 origin = RayStart(GlimmerBrickCrossingPoint(constants.bricks, cell, axis, column, crossing), crossing.normal);

    const uint hash = CrossingHash(crossingIndex);

    uint block = FindBlock(crossingIndex, hash);

    const bool hasBlock = block != BLOCK_NONE;

    if (!hasBlock)
    {
        uint first;
        uint second;
        BlockChoices(hash, first, second);

        // one whose crossing has changed since is free; otherwise either may go
        const uint firstOf = slotBlocks[first * BLOCK_WORDS];
        const uint secondOf = slotBlocks[second * BLOCK_WORDS];
        const bool isFirstHeld = firstOf < constants.limits.x && CrossingHash(firstOf) == slotBlocks[first * BLOCK_WORDS + 1u];
        const bool isSecondHeld = secondOf < constants.limits.x && CrossingHash(secondOf) == slotBlocks[second * BLOCK_WORDS + 1u];

        block = !isFirstHeld ? first : (!isSecondHeld ? second : ((pcg_hash(hash ^ constants.frame.x) & 1u) != 0u ? second : first));
    }

    // two requests can come to the same block in a dispatch.
    // the one that locks it writes it, the other asks again next frame.
    // the lock is even, as no hash is, and the frame's own, so one left in the buffer from before (or garbage) holds nothing
    const uint locked = (constants.frame.x << 1u) + 2u;
    const uint lockedFrom = slotBlocks[block * BLOCK_WORDS + 1u];

    if (lockedFrom == locked || (hasBlock && lockedFrom != hash))
    {
        return;
    }

    uint was;
    InterlockedCompareExchange(slotBlocks[block * BLOCK_WORDS + 1u], lockedFrom, locked, was);

    if (was != lockedFrom)
    {
        return;
    }

    uint oldCard[4] = { 0u, 0u, 0u, 0u };
    uint oldKey[4] = { 0u, 0u, 0u, 0u };

    if (hasBlock)
    {
        LoadBlock(block, false, oldCard, oldKey);
    }

    uint newCard[4] = { 0u, 0u, 0u, 0u };
    uint newKey[4] = { 0u, 0u, 0u, 0u };
    uint wanted[4] = { 0u, 0u, 0u, 0u };
    uint filled = 0u;

    for (uint tap = 0; tap < 4u; tap++)
    {
        const int2 bin = base + int2(tap & 1u, tap >> 1u);
        const uint key = BinKey(level, bin);

        wanted[tap] = key;

        const uint held = HeldSlot(oldKey, key);

        if (held < 4u)
        {
            newCard[filled] = oldCard[held];
            newKey[filled] = oldKey[held];
            filled++;

            continue;
        }

        const float3 direction = BinDirection(level, bin, crossingIndex);

        uint card = SLOT_BELOW;
        float t = 0.0;

        if (dot(direction, crossing.normal) >= HORIZON)
        {
            GlimmerBrickHit hit;

            if (!TraceGlimmerBricks(constants.bricks, origin, direction, 0.0, RAY_DISTANCE, hit))
            {
                card = SLOT_SKY;
            }
            else if (dot(hit.normal, direction) < 0.0)
            {
                // the back of something is what a ray sees from inside it: nothing to show
                card = PackSlotCard(hit.brick, GlimmerBrickCardSlot(constants.bricks, hit.axis, hit.normal, hit.depth));
                t = hit.t;
            }
        }

        newCard[filled] = card;
        newKey[filled] = key | (f32tof16(min(t, 60000.0)) << 16u);
        filled++;
    }

    // behind them, as many of what it held before as there is room for: the oldest go first
    for (uint oldSlot = 0; oldSlot < 4u && filled < 4u; oldSlot++)
    {
        const uint oldSlotKey = oldKey[oldSlot] & 0xFFFFu;

        if (oldSlotKey != 0u && HeldSlot(wanted, oldSlotKey) == 4u)
        {
            newCard[filled] = oldCard[oldSlot];
            newKey[filled] = oldKey[oldSlot];
            filled++;
        }
    }

    slotBlocks[block * BLOCK_WORDS] = crossingIndex;

    for (uint writeSlot = 0; writeSlot < 4u; writeSlot++)
    {
        slotBlocks[SlotWord(block, writeSlot)] = newCard[writeSlot];
        slotBlocks[SlotWord(block, writeSlot) + 1u] = newKey[writeSlot];
    }

    // unlocks it, once what it holds is written
#ifndef BACKEND_WEBGPU
    DeviceMemoryBarrier();
#endif

    uint unlockedFrom;
    InterlockedExchange(slotBlocks[block * BLOCK_WORDS + 1u], hash, unlockedFrom);
}

#endif

#if defined(MODE_RESOLVE)

void SetWant(uint index, uint want, uint code, uint brick, uint column)
{
    slotWanted[REQUEST_WORDS * index] = want;
    slotWanted[REQUEST_WORDS * index + 1u] = code;
    slotWanted[REQUEST_WORDS * index + 2u] = brick;
    slotWanted[REQUEST_WORDS * index + 3u] = column;
}

void Resolve(int2 pixel, uint2 texel, uint index)
{
    SetWant(index, WANT_NOTHING, 0u, 0u, 0u);

    OutColor[texel] = (float4)0.0;
    OutInfo[texel] = (float4)0.0;

    const ReflectionSurface surface = LoadSurface(pixel);

    if (!surface.isValid)
    {
        OutGuide[texel] = float4(0.0, 0.0, 0.0, -1.0);

        return;
    }

    OutGuide[texel] = float4(surface.P - camera.position.xyz, surface.roughness);

    const float matte = MatteShare(surface.roughness);

    if (matte >= 1.0)
    {
        OutColor[texel] = (float4)0.0;

        return;
    }

    const float3 surfaceNormal = LoadSurfaceNormal(pixel, surface.P, surface.N);
    const float width = constants.bricks.params.z;

    const uint level = LevelOf(surface.alpha);
    const bool isScattered = level < uint(SCATTER_BELOW_LEVEL);

    float3 T;
    float3 B;
    MakeBasis(surfaceNormal, T, B);

    // which crossing a pixel reads is moved, somewhere else every frame: over a few frames it has read each crossing around it
    // for as much as it is near it, which added up is the crossings blended
    float3 at = surface.P;
    float between = 1.0;

    if (constants.frame.y != 0u)
    {
        const uint own = PixelSeed(texel);
        const uint2 stepped = uint2(own, pcg_hash(own)) + constants.frame.x * uint2(3242174889u, 2447445414u);
        const float2 shift = float2(UnitFloat(stepped.x), UnitFloat(stepped.y)) - 0.5;

        OutGuide[texel] = float4(surface.P - camera.position.xyz, surface.roughness + GUIDE_DITHERED);

        between = BetweenMips(pcg_hash(own ^ 0x9E3779B9u));

        // as far as the rays are scattered: where they are not, the crossings around have nothing more to say than the pixel's own
        at += (T * shift.x + B * shift.y) * width * (isScattered ? 2.0 * DITHER_REACH_COLUMNS : 1.0);
    }
    else
    {
        const float2 shift = float2(InterleavedGradientNoise(float2(texel) + 0.5), InterleavedGradientNoise(float2(texel.yx) + float2(47.5, 17.5))) - 0.5;

        at += (T * shift.x + B * shift.y) * width;
    }

    GlimmerBrickReceiver receiver = GlimmerFindBrickReceiver(constants.bricks, at, surfaceNormal);

    if (!receiver.found)
    {
        receiver = GlimmerFindBrickReceiver(constants.bricks, surface.P, surfaceNormal);
    }

    if (!receiver.found)
    {
        SetWant(index, WANT_RAY, 0u, 0u, 0u);

        return;
    }

    // something curved turns a reflection further across one crossing than its four bins reach
    if (GlimmerBrickHasFlag(glimmerBrickMaterial[receiver.crossing], GLIMMER_BRICK_FLAG_CURVED) || dot(surfaceNormal, receiver.normal) < CURVED_FACING)
    {
        SetWant(index, WANT_RAY, 0u, 0u, 0u);

        return;
    }

    int3 receiverCell = (int3)0;
    uint receiverAxis = 0u;

    if (!GlimmerLoadBrickPlace(receiver.brick, receiverCell, receiverAxis))
    {
        SetWant(index, WANT_RAY, 0u, 0u, 0u);

        return;
    }

    const float3 receiverPosition = GlimmerBrickCrossingPoint(constants.bricks, receiverCell, receiverAxis, receiver.column, GlimmerLoadBrickCrossing(constants.bricks, receiver.crossing));

    const uint block = FindBlock(receiver.crossing, CrossingHash(receiver.crossing));

    uint heldCard[4] = (uint[4])0;
    uint heldKey[4] = (uint[4])0;

    if (block != BLOCK_NONE)
    {
        LoadBlock(block, true, heldCard, heldKey);
    }

    const int bins = int(1u << level);
    const float3 direction = Reflected(surface, surfaceNormal);

    // the bins are the crossing's, from where its middle sends the eye
    // every pixel on it reads the same ones
    const float3 fromMiddle = reflect(-normalize(camera.position.xyz - receiverPosition), receiver.normal);
    const float2 place = DirectionPlace(direction) * float(bins);
    const float2 middlePlace = DirectionPlace(fromMiddle) * float(bins);

    const int2 base = clamp(int2(floor(middlePlace - 0.5)), int2(0, 0), int2(bins - 2, bins - 2));
    const float2 ownPlace = place - 0.5 - float2(base);

    {
        const float3 stepU = GlimmerBrickPoint(receiverAxis, float2(width, 0.0), 0.0);
        const float3 stepV = GlimmerBrickPoint(receiverAxis, float2(0.0, width), 0.0);

        const float2 movedU = DirectionPlace(reflect(-normalize(camera.position.xyz - receiverPosition - stepU), receiver.normal)) * float(bins) - middlePlace;
        const float2 movedV = DirectionPlace(reflect(-normalize(camera.position.xyz - receiverPosition - stepV), receiver.normal)) * float(bins) - middlePlace;

        const float jump = max(max(abs(movedU.x), abs(movedU.y)), max(abs(movedV.x), abs(movedV.y)));
        const float nearness = saturate((jump - NEAR_FROM) / (NEAR_TO - NEAR_FROM));

        if (nearness > InterleavedGradientNoise(float2(texel) + 0.5))
        {
            SetWant(index, WANT_RAY_SAMPLED, 0u, 0u, 0u);

            return;
        }
    }

    if (any(ownPlace < -STRAY) || any(ownPlace > 1.0 + STRAY))
    {
        SetWant(index, WANT_RAY, 0u, 0u, 0u);

        return;
    }

    // the cards are read along a way drawn from the lobe and as wide as the lobe is.
    float3 aim = direction;
    float angle = LOBE_ALPHAS * surface.alpha;

    LobeAim(texel, surface, surfaceNormal, aim, angle);

    const float3 from = RayStart(receiverPosition, receiver.normal);

    float missing = 0.0;

    uint layerCard[4] = { 0u, 0u, 0u, 0u };
    float layerT[4] = { 0.0, 0.0, 0.0, 0.0 };
    uint numLayers = 0u;
    bool isOpen = false;

    for (uint tap = 0; tap < 4u; tap++)
    {
        const int2 bin = base + int2(tap & 1u, tap >> 1u);
        const uint held = HeldSlot(heldKey, BinKey(level, bin));

        if (held == 4u)
        {
            missing += 1.0;

            continue;
        }

        const uint card = heldCard[held];

        if (card == SLOT_BELOW)
        {
            continue;
        }

        if (card == SLOT_SKY)
        {
            isOpen = true;

            continue;
        }

        int3 cardCell = int3(0, 0, 0);
        uint cardAxis = 0u;

        if (!GlimmerLoadBrickPlace(card / uint(GLIMMER_BRICK_CARDS), cardCell, cardAxis))
        {
            missing += 1.0;

            continue;
        }

        float t = f16tof32(heldKey[held] >> 16u);

        const float3 landed = from + BinDirection(level, bin, receiver.crossing) * t;
        const float aimAlong = GlimmerBrickAlong(cardAxis, aim);
        const float ownT = (GlimmerBrickAlong(cardAxis, landed) - GlimmerBrickAlong(cardAxis, surface.P)) / (abs(aimAlong) > 0.05 ? aimAlong : 1e9);

        if (ownT > 0.5 * t && ownT < 2.0 * t)
        {
            t = ownT;
        }

        uint layerPlace = numLayers;

        for (; layerPlace > 0u && layerT[layerPlace - 1u] > t; layerPlace--)
        {
            layerT[layerPlace] = layerT[layerPlace - 1u];
            layerCard[layerPlace] = layerCard[layerPlace - 1u];
        }

        layerT[layerPlace] = t;
        layerCard[layerPlace] = card;
        numLayers++;
    }

    float3 light = (float3)0.0;
    float hits = 0.0;
    float reach = 0.0;
    float remaining = 1.0;

    float3 readAt[4] = { (float3)0.0, (float3)0.0, (float3)0.0, (float3)0.0 };
    uint readAxis[4] = { 0u, 0u, 0u, 0u };

    for (uint layer = 0; layer < numLayers; layer++)
    {
        const uint card = layerCard[layer];

        int3 cardCell = int3(0, 0, 0);
        uint cardAxis = 0u;
        GlimmerLoadBrickPlace(card / uint(GLIMMER_BRICK_CARDS), cardCell, cardAxis);

        readAt[layer] = surface.P + aim * layerT[layer];
        readAxis[layer] = cardAxis;

        // two slots that landed on one wall are one read
        bool isAgain = false;

        for (uint other = 0; other < layer; other++)
        {
            isAgain = isAgain || (card % uint(GLIMMER_BRICK_CARDS) == layerCard[other] % uint(GLIMMER_BRICK_CARDS)
                && cardAxis == readAxis[other]
                && all(abs(readAt[layer] - readAt[other]) < width));
        }

        if (isAgain || remaining < LAYER_LEAST)
        {
            continue;
        }

        const float4 arriving = CardLight(cardCell, cardAxis, card % uint(GLIMMER_BRICK_CARDS), readAt[layer], (float3)0.0, angle * layerT[layer] * between);

        if (arriving.a == 0.0)
        {
            continue;
        }

        light += arriving.rgb * remaining;
        hits += arriving.a * remaining;
        reach += layerT[layer] * arriving.a * remaining;
        remaining *= 1.0 - arriving.a;
    }

    const float beyond = isOpen ? remaining : 0.0;
    const uint blank = hits + beyond > LAYER_ENOUGH ? 0u : WANT_BLANK;

    if (missing > 0.0)
    {
        const uint code = (level << 28u) | (uint(base.y) << 13u) | uint(base.x);

        SetWant(index, receiver.crossing, code | blank, receiver.brick, uint(receiver.column.y) * uint(GLIMMER_BRICK_SIDE) + uint(receiver.column.x));
    }
    else if (blank != 0u)
    {
        SetWant(index, WANT_FILL, 0u, 0u, 0u);
    }

    if (blank != 0u)
    {
        return;
    }

    // what is beyond the bricks is left to what the deferred pass has there
    const float total = max(hits + beyond, 1e-6);

    OutColor[texel] = float4(light, hits) * ((1.0 - matte) / total);

    OutInfo[texel] = float4(hits > 0.0 ? reach / hits : -1.0, 0.0, 0.0, 0.0);
}

#endif

#if defined(MODE_FINISH)

void ListPixel(uint index, uint single)
{
    uint at;
    InterlockedAdd(slotCounts[COUNT_LISTED], 1u, at);

    slotList[at] = index | single;
}

void Finish(int2 pixel, uint2 texel, uint index)
{
    const uint want = slotWanted[REQUEST_WORDS * index];

    if (want == WANT_NOTHING)
    {
        return;
    }

    if (want == WANT_RAY || want == WANT_RAY_SAMPLED)
    {
        const bool isSampledAnyway = (want == WANT_RAY_SAMPLED);

        const ReflectionSurface surface = LoadSurface(pixel);
        const bool isSampled = constants.frame.z != 0u && (surface.roughness >= SAMPLED_FROM || isSampledAnyway);

        OutGuide[texel] = float4(surface.P - camera.position.xyz, surface.roughness + (isSampled ? GUIDE_SAMPLED : 0.0));

        // every other one, in a checker that changes sides each frame, where a history holds the rest as they were
        if (isSampled && ((texel.x + texel.y + constants.frame.x) & 1u) != 0u)
        {
            OutInfo[texel] = float4(REACH_HOLE, 0.0, 0.0, 0.0);

            return;
        }

        ListPixel(index, isSampledAnyway ? LIST_SAMPLED : 0u);

        return;
    }

    const uint code = slotWanted[REQUEST_WORDS * index + 1u];

    // a ray of its own meanwhile for a pixel with nothing to show
    if (want == WANT_FILL || (code & WANT_BLANK) != 0u)
    {
        ListPixel(index, LIST_SINGLE);
    }

    if (want == WANT_FILL)
    {
        return;
    }

    // a crossing is asked after once a frame however many pixels are on it
    const uint stamp = (want ^ (constants.frame.x * 2654435769u));

    uint was;
    InterlockedExchange(slotClaims[pcg_hash(want) & 0xFFFFFu], stamp, was);

    if (was == stamp)
    {
        return;
    }

    uint at;
    InterlockedAdd(slotCounts[COUNT_REQUESTS], 1u, at);

    if (at >= constants.limits.z)
    {
        return;
    }

    slotRequests[REQUEST_WORDS * at] = want;
    slotRequests[REQUEST_WORDS * at + 1u] = code & ~WANT_BLANK;
    slotRequests[REQUEST_WORDS * at + 2u] = slotWanted[REQUEST_WORDS * index + 2u];
    slotRequests[REQUEST_WORDS * at + 3u] = slotWanted[REQUEST_WORDS * index + 3u];
}

#endif

#if defined(MODE_RAYS)

void TraceListed(uint listed)
{
    if (listed >= slotCounts[COUNT_LISTED])
    {
        return;
    }

    const uint entry = slotList[listed];
    const uint place = entry & ~(LIST_SINGLE | LIST_SAMPLED);
    const uint2 texel = uint2(place % constants.dimensions.z, place / constants.dimensions.z);
    const int2 pixel = FullPixel(int2(texel));

    const ReflectionSurface surface = LoadSurface(pixel);
    const float3 surfaceNormal = LoadSurfaceNormal(pixel, surface.P, surface.N);

    float3 direction = Reflected(surface, surfaceNormal);
    float angle = LOBE_ALPHAS * surface.alpha;

    if ((entry & LIST_SINGLE) != 0u)
    {
        // filling in for what the pixel's slots lack: the way its cards were read along
        LobeAim(texel, surface, surfaceNormal, direction, angle);
    }
    else if (constants.frame.z != 0u && (surface.roughness >= SAMPLED_FROM || (entry & LIST_SAMPLED) != 0u))
    {
        const uint own = PixelSeed(texel);
        const uint traced = constants.frame.x >> 1u;

        const float turn = 2.0 * HYP_FMATH_PI * UnitFloat(pcg_hash(own) + traced * 3242174889u);
        const float radius = SPREAD_DISC * surface.alpha * sqrt(UnitFloat(own + traced * 2447445414u));

        float3 T;
        float3 B;
        MakeBasis(direction, T, B);

        direction = normalize(direction + T * (radius * cos(turn)) + B * (radius * sin(turn)));
        direction = AboveHorizon(direction, surfaceNormal);

        angle = 2.0 * SPREAD_DISC * surface.alpha * BetweenMips(pcg_hash(own ^ 0x9E3779B9u));
    }

    float t;
    OutColor[texel] = TraceReflection(RayStart(surface.P, surfaceNormal), direction, angle, surface.roughness, t);
    OutInfo[texel] = float4(t, 0.0, 0.0, 0.0);
}

#endif

#if defined(MODE_WHOLE_LIST)

void ListWhole(uint2 pixel)
{
    if (all((pixel & 1u) == 0u))
    {
        return;
    }

    const ReflectionSurface surface = LoadSurface(int2(pixel));

    if (!surface.isValid || surface.roughness >= SAMPLED_FROM)
    {
        return;
    }

    const float4 resolved = GuideTexture.Load(int3(min(pixel / 2u, constants.dimensions.zw - 1u), 0));

    if (resolved.a < 0.0 || abs(resolved.a - surface.roughness) > ROUGHNESS_TOLERANCE)
    {
        return;
    }

    uint at;
    InterlockedAdd(slotCounts[COUNT_WHOLE], 1u, at);

    slotWhole[at] = pixel.y * constants.dimensions.x + pixel.x;
}

#endif

#if defined(MODE_WHOLE_RAYS)

void TraceWhole(uint listed)
{
    if (listed >= slotCounts[COUNT_WHOLE])
    {
        return;
    }

    const uint place = slotWhole[listed];
    const int2 pixel = int2(place % constants.dimensions.x, place / constants.dimensions.x);

    const ReflectionSurface surface = LoadSurface(pixel);
    const float3 surfaceNormal = LoadSurfaceNormal(pixel, surface.P, surface.N);

    float t;
    OutColor[pixel] = TraceReflection(RayStart(surface.P, surfaceNormal), Reflected(surface, surfaceNormal), LOBE_ALPHAS * surface.alpha, surface.roughness, t);
}

#endif

#if defined(MODE_HISTORY)

float3 PreviousCameraPosition()
{
    const float4x4 m = camera.prevViewProjMat;

    const float3 a = m[0].xyz;
    const float3 b = m[1].xyz;
    const float3 c = m[3].xyz;

    const float3 bc = cross(b, c);

    return -(m[0].w * bc + m[1].w * cross(c, a) + m[3].w * cross(a, b)) / dot(a, bc);
}

bool PreviousScreen(float3 position, out float2 outScreen)
{
    const float4 clip = mul(camera.prevViewProjMat, float4(position, 1.0));

    outScreen = (float2)0.0;

    if (clip.w <= 0.0)
    {
        return false;
    }

    const float2 ndc = clip.xy / clip.w;

    outScreen = float2(ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5) * float2(constants.dimensions.xy);

    return all(abs(ndc) <= 1.0);
}

float4 AddedAt(
    float2 screen,
    bool isOnScreen,
    float3 position,
    float slack,
    bool isAnywhere,
    float3 plane,
    float roughness,
    out float outFrames,
    out float outFound)
{
    outFrames = 0.0;
    outFound = 0.0;

    if (!isOnScreen)
    {
        return (float4)0.0;
    }

    const float2 at = (screen - 0.5) * 0.5;
    const int2 corner = int2(floor(at));
    const float2 fraction = at - float2(corner);

    float4 sum = (float4)0.0;

    [unroll]
    for (uint tap = 0; tap < 4u; tap++)
    {
        const int2 offset = int2(tap & 1u, tap >> 1u);
        const int2 texel = corner + offset;

        if (any(texel < 0) || any(texel >= int2(constants.dimensions.zw)))
        {
            continue;
        }

        const float4 there = HistoryGuideTexture.Load(int3(texel, 0));

        if (there.a < 0.5 * GUIDE_SAMPLED)
        {
            continue;
        }

        if (isAnywhere)
        {
            const float finish = there.a - (there.a >= 1.5 * GUIDE_SAMPLED ? GUIDE_DITHERED : GUIDE_SAMPLED);

            if (abs(dot(there.xyz - position, plane)) > slack || abs(finish - roughness) > ROUGHNESS_TOLERANCE)
            {
                continue;
            }
        }
        else if (distance(there.xyz, position) > slack)
        {
            continue;
        }

        const float2 weights = lerp(1.0 - fraction, fraction, float2(offset));
        const float weight = weights.x * weights.y;

        sum += HistoryColorTexture.Load(int3(texel, 0)) * weight;
        outFrames += HistoryInfoTexture.Load(int3(texel, 0)).y * weight;
        outFound += weight;
    }

    if (outFound < 1e-3)
    {
        outFound = 0.0;
        outFrames = 0.0;

        return (float4)0.0;
    }

    outFrames /= outFound;

    return sum / outFound;
}

void AddUp(uint2 texel)
{
    const int2 halfPixel = int2(texel);

    float4 color = ColorTexture.Load(int3(halfPixel, 0));
    float reach = InfoTexture.Load(int3(halfPixel, 0)).x;

    const float4 here = GuideTexture.Load(int3(halfPixel, 0));

    // a pixel with no ray this frame keeps what it has added up, and failing that takes what the four beside it traced
    const bool isHole = reach == REACH_HOLE;

    if (isHole)
    {
        float4 beside = (float4)0.0;
        float besideWeight = 0.0;

        [unroll]
        for (int neighbour = 0; neighbour < 4; neighbour++)
        {
            const int2 offset = int2((neighbour & 1) * (1 - (neighbour & 2)), ((neighbour + 1) & 1) * (1 - (neighbour & 2)));
            const int2 tap = clamp(halfPixel + offset, int2(0, 0), int2(constants.dimensions.zw) - 1);

            if (InfoTexture.Load(int3(tap, 0)).x != REACH_HOLE)
            {
                beside += ColorTexture.Load(int3(tap, 0));
                besideWeight += 1.0;
            }
        }

        color = beside / max(besideWeight, 1.0);
        reach = 0.0;
    }

    OutColor[texel] = color;
    OutInfo[texel] = float4(reach, 0.0, 0.0, 0.0);

    if (here.a < 0.5 * GUIDE_SAMPLED || constants.limits.w != 0u)
    {
        return;
    }

    // from the depth buffer and not from the guide, whose positions are half floats
    const int2 pixel = FullPixel(halfPixel);
    const ReflectionSurface surface = LoadSurface(pixel);

    float2 screen = (float2)0.0;
    const bool isOnScreen = PreviousScreen(surface.P, screen);

    const float3 fromPreviousCamera = surface.P - PreviousCameraPosition();

    const float slack = SAME_SURFACE * distance(camera.position.xyz, surface.P) + 1e-3;
    const bool isDithered = here.a >= 1.5 * GUIDE_SAMPLED;
    const float roughness = here.a - (isDithered ? GUIDE_DITHERED : GUIDE_SAMPLED);

    // where what the pixel reflects was: along the way the pixel is seen, as far behind the surface as its rays went
    float3 plane = (float3)0.0;
    float2 image = screen;
    bool isImageOnScreen = isOnScreen;

    float reflected = 0.0;
    float forgets = 0.0;

    if (!isHole && reach != 0.0)
    {
        const float went = reach < 0.0 ? MISS_DISTANCE : reach;
        const float3 toSurface = surface.P - camera.position.xyz;
        const float away = length(toSurface);

        isImageOnScreen = PreviousScreen(camera.position.xyz + toSurface / away * (away + went), image);

        const float mirrorlike = 1.0 - smoothstep(VIRTUAL_BELOW, SURFACE_ABOVE, roughness);

        forgets = SLIDE_FORGETS * distance(image, screen) * mirrorlike;

        // only something flat has its reflection elsewhere on itself, and only where no normal map turns it by more than its lobe is wide
        if (isDithered)
        {
            plane = LoadSurfaceNormal(pixel, surface.P, surface.N);

            const float lobe = 2.0 * roughness * roughness;

            reflected = mirrorlike * (1.0 - smoothstep(0.1 * lobe, 0.5 * lobe, length(surface.N - plane)));
        }
    }

    float4 sum = (float4)0.0;
    float frames = 0.0;
    float total = 0.0;
    float bySurface = 1.0;

    if (reflected > 0.0)
    {
        sum = AddedAt(image, isImageOnScreen, fromPreviousCamera, slack, true, plane, roughness, frames, total);
        bySurface = total > 0.0 ? 1.0 - reflected : 1.0;
    }

    if (reflected < 1.0 || total == 0.0)
    {
        float ownFrames = 0.0;
        float found = 0.0;
        const float4 ownSum = AddedAt(screen, isOnScreen, fromPreviousCamera, slack, false, plane, roughness, ownFrames, found);

        if (total == 0.0)
        {
            sum = ownSum;
            frames = ownFrames;
            total = found;
        }
        else if (found > 0.0)
        {
            sum = lerp(ownSum, sum, reflected);
            frames = lerp(ownFrames, frames, reflected);
        }
        else
        {
            bySurface = 0.0;
        }
    }

    if (total == 0.0)
    {
        OutInfo[texel] = float4(reach, 1.0, 0.0, 0.0);

        return;
    }

    frames /= 1.0 + forgets * bySurface;

    if (isHole)
    {
        OutColor[texel] = sum;
        OutInfo[texel] = float4(0.0, frames, 0.0, 0.0);

        return;
    }

    const float added = min(frames + 1.0, float(isDithered ? constants.frame.y : constants.frame.z));

    // what is many times what the pixel has added up is dimmed to that: one ray that finds a window is a flash
    float4 fresh = color;

    if (frames >= 4.0)
    {
        const float most = FIREFLIES * dot(sum.rgb, float3(0.2126, 0.7152, 0.0722)) + 1e-3;
        const float luminance = dot(fresh.rgb, float3(0.2126, 0.7152, 0.0722));

        fresh.rgb *= min(1.0, most / max(luminance, 1e-6));
    }

    OutColor[texel] = lerp(sum, fresh, 1.0 / max(added, 1.0));
    OutInfo[texel] = float4(reach, added, 0.0, 0.0);
}

#endif

[numthreads(8, 8, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
#if defined(MODE_REQUESTS)
    TraceRequest(dispatchThreadId.y * TRACE_ROW + dispatchThreadId.x);
#elif defined(MODE_RAYS)
    TraceListed(dispatchThreadId.y * TRACE_ROW + dispatchThreadId.x);
#elif defined(MODE_WHOLE_LIST)
    if (all(dispatchThreadId.xy < constants.dimensions.xy))
    {
        ListWhole(dispatchThreadId.xy);
    }
#elif defined(MODE_WHOLE_RAYS)
    TraceWhole(dispatchThreadId.y * TRACE_ROW + dispatchThreadId.x);
#elif defined(MODE_SPREAD)
    const int2 pixel = int2(dispatchThreadId.xy);

    if (any(dispatchThreadId.xy >= constants.dimensions.xy))
    {
        return;
    }

    const ReflectionSurface surface = LoadSurface(pixel);

    if (!surface.isValid)
    {
        OutColor[pixel] = (float4)0.0;

        return;
    }

    const float slack = SPREAD_PLANE_TOLERANCE * distance(camera.position.xyz, surface.P) + 1e-3;
    const int2 halfDimensions = int2(constants.dimensions.zw);

    // those alike, and all of them in case none is
    const int2 base = (pixel + 2) / 2 - 1;

    float4 alike = (float4)0.0;
    float alikeWeight = 0.0;
    float4 anyOf = (float4)0.0;
    float anyWeight = 0.0;

    [unroll]
    for (uint corner = 0; corner < 4u; corner++)
    {
        const int2 block = base + int2(corner & 1u, corner >> 1u);

        if (any(block < 0) || any(block >= halfDimensions))
        {
            continue;
        }

        const int2 source = FullPixel(block);
        const float2 apart = 1.0 - min(abs(float2(pixel - source)) * 0.5, 1.0);

        float weight = apart.x * apart.y;

        if (weight <= 0.0)
        {
            continue;
        }

        const float4 traced = ColorTexture.Load(int3(block, 0));

        anyOf += traced * weight;
        anyWeight += weight;

        if (any(source != pixel))
        {
            const ReflectionSurface other = LoadSurface(source);

            if (!other.isValid)
            {
                continue;
            }

            const float off = dot(other.P - surface.P, surface.N) / slack;
            const float finish = (other.roughness - surface.roughness) / ROUGHNESS_TOLERANCE;

            weight *= pow(saturate(dot(other.N, surface.N)), 32.0) * exp2(-off * off - finish * finish);
        }

        alike += traced * weight;
        alikeWeight += weight;
    }

    OutColor[pixel] = alikeWeight > 0.05 ? alike / alikeWeight : anyOf / max(anyWeight, 1e-6);
#else
    const uint2 texel = dispatchThreadId.xy;

    if (any(texel >= constants.dimensions.zw))
    {
        return;
    }

    const int2 pixel = FullPixel(int2(texel));
    const uint index = texel.y * constants.dimensions.z + texel.x;

#if defined(MODE_RESOLVE)
    if (index == 0u)
    {
        for (uint counter = 0; counter < NUM_COUNTS; counter++)
        {
            slotCounts[counter] = 0u;
        }
    }

    Resolve(pixel, texel, index);
#elif defined(MODE_FINISH)
    Finish(pixel, texel, index);
#else
    AddUp(texel);
#endif
#endif
}
