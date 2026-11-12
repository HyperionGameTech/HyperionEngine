#include "../../Include/Defines.hlsli"

PERMUTE(MODE, BLEND, COMPOSE)

#include "GlimmerSurfelTypes.hlsli"

struct GlimmerSurfelBlendConstants
{
    GlimmerSurfels surfels;

    /// x = first ray record of the surfels'
    /// y = most updates the history averages
    /// z = the wake epoch: a surfel last updated in an older one keeps little of its history
    uint4 dispatch;

    GlimmerLightParams light;
};

#include "../../Include/Clouds.hlsli"

DECLARE_BUFFER_DYNAMIC(GlimmerSurfelBlend, CBuffer) cbuffer CBuffer
{
    GlimmerSurfelBlendConstants constants;
    CloudVolume cloudVolume;
    CloudWeatherMap cloudWeatherMap;
    CloudShadowMap cloudShadowMap;
};

DECLARE_SRV(GlimmerSurfelBlend, GlimmerSurfelsBuffer) StructuredBuffer<uint4> glimmerSurfels;

DECLARE_SAMPLER(GlimmerSurfelBlend, SamplerLinearMipmap) SamplerState cloudSampler;
DECLARE_SRV(GlimmerSurfelBlend, CloudWeatherMapTexture) Texture2DArray cloudWeatherMapTexture;
DECLARE_SRV(GlimmerSurfelBlend, CloudShadowMapTexture) Texture2D cloudShadowMapTexture;

/// for its counters
DECLARE_SRV(GlimmerSurfelBlend, GlimmerSurfelHashBuffer) StructuredBuffer<uint> glimmerSurfelHash;

/// GLIMMER_SURFEL_DATA_UINTS per surfel
DECLARE_UAV(GlimmerSurfelBlend, OutSurfelData) RWStructuredBuffer<uint> OutSurfelData;

/// x = updates, y = how much of it can be seen (float bits)
DECLARE_UAV(GlimmerSurfelBlend, OutSurfelStates) RWStructuredBuffer<uint2> OutSurfelStates;

/// the foliage voxels' hash, then the voxels
DECLARE_UAV(GlimmerSurfelBlend, OutFoliage) RWStructuredBuffer<uint> OutFoliage;

#define glimmerSurfelData OutSurfelData
#define glimmerFoliage OutFoliage
#include "GlimmerSurfelsGather.hlsli"

GlimmerSurfelLight LoadSurfelLight(uint surfelIndex)
{
    const uint base = GlimmerSurfelDataIndex(surfelIndex, GLIMMER_SURFEL_DATA_LIGHT);

    GlimmerSurfelLight light;

    [unroll]
    for (uint part = 0; part < GLIMMER_LIGHT_PARTS; part++)
    {
        light.parts[part] = GlimmerUnpackLightPart(uint2(OutSurfelData[base + part * 2u], OutSurfelData[base + part * 2u + 1u]));
    }

    [unroll]
    for (uint term = 0; term < GLIMMER_LIGHT_TERMS; term++)
    {
        const uint packed = OutSurfelData[base + GLIMMER_SURFEL_DIRECT_UINT + term / 2u];

        light.direct[term] = f16tof32((term & 1u) != 0u ? (packed >> 16) : (packed & 0xFFFFu));
    }

    return light;
}

void StoreSurfelLight(uint surfelIndex, GlimmerSurfelLight light)
{
    const uint base = GlimmerSurfelDataIndex(surfelIndex, GLIMMER_SURFEL_DATA_LIGHT);

    [unroll]
    for (uint part = 0; part < GLIMMER_LIGHT_PARTS; part++)
    {
        const uint2 packed = GlimmerPackLightPart(light.parts[part]);

        OutSurfelData[base + part * 2u] = packed.x;
        OutSurfelData[base + part * 2u + 1u] = packed.y;
    }

    [unroll]
    for (uint pair = 0; pair < (GLIMMER_LIGHT_TERMS + 1u) / 2u; pair++)
    {
        const float low = light.direct[pair * 2u];
        const float high = pair * 2u + 1u < GLIMMER_LIGHT_TERMS ? light.direct[pair * 2u + 1u] : 0.0;

        OutSurfelData[base + GLIMMER_SURFEL_DIRECT_UINT + pair] = f32tof16(clamp(low, -65000.0, 65000.0)) | (f32tof16(clamp(high, -65000.0, 65000.0)) << 16);
    }
}

float3 LoadSurfelSun(uint surfelIndex)
{
    const uint base = GlimmerSurfelDataIndex(surfelIndex, GLIMMER_SURFEL_DATA_SUN);

    return GlimmerUnpackLightPart(uint2(OutSurfelData[base], OutSurfelData[base + 1u]));
}

uint LoadSurfelSunState(uint surfelIndex)
{
    return OutSurfelData[GlimmerSurfelDataIndex(surfelIndex, GLIMMER_SURFEL_DATA_SUN_STATE)];
}

void StoreSurfelSun(uint surfelIndex, float3 sun, uint sunState)
{
    const uint base = GlimmerSurfelDataIndex(surfelIndex, GLIMMER_SURFEL_DATA_SUN);
    const uint2 packed = GlimmerPackLightPart(sun);

    OutSurfelData[base] = packed.x;
    OutSurfelData[base + 1u] = packed.y;
    OutSurfelData[GlimmerSurfelDataIndex(surfelIndex, GLIMMER_SURFEL_DATA_SUN_STATE)] = sunState;
}

// the sun bounce is traced without the clouds, which move on long after it settles; the cloud shadow over the surfel stands in for the one over what it sees
float3 ComposeSurfel(uint surfelIndex, GlimmerSurfelLight light, float3 sun, uint sunState)
{
    const float3 P = asfloat(glimmerSurfels[surfelIndex].xyz);
    const float cloudShadow = GetCloudShadow(cloudWeatherMapTexture, cloudShadowMapTexture, cloudSampler, cloudVolume, cloudWeatherMap, cloudShadowMap, P, constants.surfels.sun.xyz);

    float3 keyedSun;
    float keyedFreshness;
    bool hasKeys;
    GLIMMER_SURFEL_KEYED_SUN(OutSurfelData, constants.surfels, surfelIndex, GLIMMER_SURFEL_DATA_SUN_KEYS_SMOOTHED, keyedSun, keyedFreshness, hasKeys);

    if (hasKeys)
    {
        return GlimmerComposeSurfelLight(constants.light, light, keyedSun * cloudShadow, keyedFreshness);
    }

    return GlimmerComposeSurfelLight(constants.light, light, sun * cloudShadow, GlimmerSurfelSunFreshness(constants.surfels, sunState));
}

void StoreSurfelComposed(uint surfelIndex, float3 irradiance, float visible)
{
    const uint2 packed = GlimmerPackSurfelComposed(irradiance, visible);
    const uint base = GlimmerSurfelDataIndex(surfelIndex, GLIMMER_SURFEL_DATA_COMPOSED);

    OutSurfelData[base] = packed.x;
    OutSurfelData[base + 1u] = packed.y;
}

void LoadFoliageLight(uint voxelIndex, out float4 outEntries[GLIMMER_LIGHT_ENTRIES])
{
    [unroll]
    for (uint entry = 0; entry < GLIMMER_LIGHT_ENTRIES; entry++)
    {
        outEntries[entry] = GLIMMER_LOAD_FOLIAGE_ENTRY(OutFoliage, voxelIndex, entry);
    }
}

void StoreFoliageLight(uint voxelIndex, float4 entries[GLIMMER_LIGHT_ENTRIES])
{
    [unroll]
    for (uint entry = 0; entry < GLIMMER_LIGHT_ENTRIES; entry++)
    {
        const uint2 packed = GlimmerPackLightEntry(entries[entry]);

        OutFoliage[GlimmerFoliageIndex(voxelIndex, GLIMMER_FOLIAGE_LIGHT + entry * 2u)] = packed.x;
        OutFoliage[GlimmerFoliageIndex(voxelIndex, GLIMMER_FOLIAGE_LIGHT + entry * 2u + 1u)] = packed.y;
    }
}

float FoliageCloudShadow(uint voxelIndex)
{
    const int3 cell = int3(
        asint(OutFoliage[GlimmerFoliageIndex(voxelIndex, GLIMMER_FOLIAGE_CELL)]),
        asint(OutFoliage[GlimmerFoliageIndex(voxelIndex, GLIMMER_FOLIAGE_CELL + 1u)]),
        asint(OutFoliage[GlimmerFoliageIndex(voxelIndex, GLIMMER_FOLIAGE_CELL + 2u)]));

    const float3 center = (float3(cell) + 0.5) * constants.surfels.foliage.x;

    return GetCloudShadow(cloudWeatherMapTexture, cloudShadowMapTexture, cloudSampler, cloudVolume, cloudWeatherMap, cloudShadowMap, center, constants.surfels.sun.xyz);
}

void StoreFoliageComposed(uint voxelIndex, float4 entries[GLIMMER_LIGHT_ENTRIES], float4 sun[3], uint sunState)
{
    const float cloudShadow = FoliageCloudShadow(voxelIndex);

    float4 keyedSun[3];
    float keyedFreshness;
    bool hasKeys;
    GLIMMER_KEYED_SUN_L1(OutFoliage, constants.surfels, GlimmerFoliageIndex(voxelIndex, GLIMMER_FOLIAGE_SUN_KEYS), GlimmerFoliageIndex(voxelIndex, GLIMMER_FOLIAGE_SUN_KEY_STATE), GLIMMER_SURFEL_SUN_KEY_SETTLE, keyedSun, keyedFreshness, hasKeys);

    float sunFreshness = GlimmerSurfelSunFreshness(constants.surfels, sunState);

    if (hasKeys)
    {
        sun = keyedSun;
        sunFreshness = keyedFreshness;
    }

    [unroll]
    for (uint sunChannel = 0; sunChannel < 3; sunChannel++)
    {
        sun[sunChannel] *= cloudShadow;
    }

    float4 composed[3];
    GlimmerComposeStoredLightWithSun(constants.light, entries, sun, sunFreshness, composed);

    [unroll]
    for (uint channel = 0; channel < 3; channel++)
    {
        const uint2 packed = GlimmerPackLightEntry(composed[channel]);

        OutFoliage[GlimmerFoliageIndex(voxelIndex, GLIMMER_FOLIAGE_COMPOSED + channel * 2u)] = packed.x;
        OutFoliage[GlimmerFoliageIndex(voxelIndex, GLIMMER_FOLIAGE_COMPOSED + channel * 2u + 1u)] = packed.y;
    }
}

#if defined(MODE_COMPOSE)

/// a thread per surfel, then per foliage voxel from GLIMMER_SURFEL_CAPACITY on
[numthreads(64, 1, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint index = dispatchThreadId.x;

    if (index >= GLIMMER_SURFEL_CAPACITY)
    {
        const uint voxelIndex = index - GLIMMER_SURFEL_CAPACITY;

        if (voxelIndex >= GlimmerFoliageCount(glimmerSurfelHash[GLIMMER_SURFEL_COUNTER_FOLIAGE])
            || OutFoliage[GlimmerFoliageIndex(voxelIndex, GLIMMER_FOLIAGE_STATE)] == 0u)
        {
            return;
        }

        float4 entries[GLIMMER_LIGHT_ENTRIES];
        LoadFoliageLight(voxelIndex, entries);

        float4 sun[3];
        uint sunState;
        GLIMMER_LOAD_SUN_BOUNCE(OutFoliage, GlimmerFoliageIndex(voxelIndex, GLIMMER_FOLIAGE_SUN), sun, sunState);

        StoreFoliageComposed(voxelIndex, entries, sun, sunState);

        return;
    }

    if (index >= GlimmerSurfelCount(glimmerSurfelHash[GLIMMER_SURFEL_COUNTER_SURFELS]))
    {
        return;
    }

    const uint2 state = OutSurfelStates[index];

    if (state.x == 0u)
    {
        return;
    }

    StoreSurfelComposed(index, ComposeSurfel(index, LoadSurfelLight(index), LoadSurfelSun(index), LoadSurfelSunState(index)), asfloat(state.y));
}

#else

DECLARE_SRV(GlimmerSurfelBlend, Rays) StructuredBuffer<uint> rays;

#define GLIMMER_SURFEL_SUN_SUM (GLIMMER_LIGHT_PARTS * 3u + GLIMMER_LIGHT_TERMS)
#define GLIMMER_SURFEL_SUN_KEY_SUM (GLIMMER_SURFEL_SUN_SUM + 3u)
#define GLIMMER_SURFEL_SUMS (GLIMMER_SURFEL_SUN_KEY_SUM + GLIMMER_SUN_KEYS * 3u)

// share of rays starting inside or hitting a back face at which a surfel or voxel stops counting
#define GLIMMER_SURFEL_HIDDEN_FRACTION 0.4

groupshared float3 gsRayParts[GLIMMER_SURFEL_RAYS * GLIMMER_LIGHT_PARTS];
groupshared float gsRayDirect[GLIMMER_SURFEL_RAYS * GLIMMER_LIGHT_TERMS];
groupshared float3 gsRaySun[GLIMMER_SURFEL_RAYS];
groupshared float3 gsRaySunKeys[GLIMMER_SURFEL_RAYS * GLIMMER_SUN_KEYS];
groupshared float3 gsDirections[GLIMMER_SURFEL_RAYS];
groupshared float4 gsSums[GLIMMER_SURFEL_SUMS]; // the mean, then the mean times the direction (foliage voxels only)
groupshared uint gsBlocked;

// the sun bounce into shade is mostly a few bright patches seen through gaps, so a surfel's own keys are noisy; the light is composed
// with them averaged over the surfels about a cell around it that face its way and lie on its plane. It reads their own keys, not their averages,
// so the averaging doesn't spread further each time
#define GLIMMER_SURFEL_SUN_SMOOTH_FACING 4.0
#define GLIMMER_SURFEL_SUN_SMOOTH_PLANE 0.5 // of the cell size

void SmoothSurfelSunKeys(uint surfelIndex)
{
    const uint4 surfel = glimmerSurfels[surfelIndex];
    const float3 P = asfloat(surfel.xyz);
    const float3 N = GlimmerUnpackSurfelNormal(surfel.w);

    const float cellSize = constants.surfels.params.x;
    const float invCellSizeSquared = 1.0 / max(cellSize * cellSize, 1e-6);

    float3 sums[GLIMMER_SUN_KEYS];

    [unroll]
    for (uint clearKey = 0; clearKey < GLIMMER_SUN_KEYS; clearKey++)
    {
        sums[clearKey] = (float3)0.0;
    }

    float weightSum = 0.0;

    const int3 firstCell = GlimmerFirstSurfelGatherCell(constants.surfels, P);

    [loop]
    for (uint corner = 0; corner < 8; corner++)
    {
        uint neighbour = GlimmerFindSurfelCell(GlimmerSurfelGatherCell(firstCell, corner));

        [loop]
        for (uint step = 0; step < GLIMMER_SURFEL_MAX_PER_CELL && neighbour != GLIMMER_SURFEL_NONE; step++)
        {
            const uint4 other = glimmerSurfels[neighbour];
            const float3 offset = asfloat(other.xyz) - P;
            const float facing = dot(N, GlimmerUnpackSurfelNormal(other.w));

            [branch]
            if (facing > 0.0 && abs(dot(offset, N)) < GLIMMER_SURFEL_SUN_SMOOTH_PLANE * cellSize)
            {
                const uint keyUpdates = GlimmerSurfelSunKeyUpdates(constants.surfels, OutSurfelData[GlimmerSurfelDataIndex(neighbour, GLIMMER_SURFEL_DATA_SUN_KEY_STATE)]);

                const float weight = pow(facing, GLIMMER_SURFEL_SUN_SMOOTH_FACING)
                    * exp(-dot(offset, offset) * invCellSizeSquared)
                    * GlimmerSurfelSunKeyFreshness(keyUpdates, GLIMMER_SURFEL_SUN_KEY_SETTLE);

                [branch]
                if (weight > 1e-4)
                {
                    const uint keyBase = GlimmerSurfelDataIndex(neighbour, GLIMMER_SURFEL_DATA_SUN_KEYS);

                    [unroll]
                    for (uint key = 0; key < GLIMMER_SUN_KEYS; key++)
                    {
                        sums[key] += GlimmerUnpackRGB9E5(OutSurfelData[keyBase + key]) * weight;
                    }

                    weightSum += weight;
                }
            }

            neighbour = GlimmerNextSurfel(neighbour);
        }
    }

    const uint ownBase = GlimmerSurfelDataIndex(surfelIndex, GLIMMER_SURFEL_DATA_SUN_KEYS);
    const uint smoothedBase = GlimmerSurfelDataIndex(surfelIndex, GLIMMER_SURFEL_DATA_SUN_KEYS_SMOOTHED);

    [unroll]
    for (uint key = 0; key < GLIMMER_SUN_KEYS; key++)
    {
        OutSurfelData[smoothedBase + key] = weightSum > 1e-4 ? GlimmerPackRGB9E5(sums[key] / weightSum) : OutSurfelData[ownBase + key];
    }
}

// every key's sun bounce takes in the trace, which was for all of them
void BlendSurfelSunKeys(uint surfelIndex, uint updates, float invOpen)
{
    const uint keyStateIndex = GlimmerSurfelDataIndex(surfelIndex, GLIMMER_SURFEL_DATA_SUN_KEY_STATE);
    const uint keyUpdates = updates > 0u ? GlimmerSurfelSunKeyUpdates(constants.surfels, OutSurfelData[keyStateIndex]) : 0u;
    const float keyBlend = GlimmerSurfelSunKeyBlendWeight(keyUpdates, 1u);

    [loop]
    for (uint key = 0; key < GLIMMER_SUN_KEYS; key++)
    {
        const uint keyIndex = GlimmerSurfelDataIndex(surfelIndex, GLIMMER_SURFEL_DATA_SUN_KEYS + key);
        const uint sumIndex = GLIMMER_SURFEL_SUN_KEY_SUM + key * 3u;
        const float3 estimate = float3(gsSums[sumIndex].x, gsSums[sumIndex + 1u].x, gsSums[sumIndex + 2u].x) * invOpen;

        OutSurfelData[keyIndex] = GlimmerPackRGB9E5(lerp(GlimmerUnpackRGB9E5(OutSurfelData[keyIndex]), estimate, keyBlend));
    }

    OutSurfelData[keyStateIndex] = GlimmerSurfelNextSunKeyState(constants.surfels, keyUpdates);

    SmoothSurfelSunKeys(surfelIndex);
}

#define GLIMMER_SURFEL_WAKE_UPDATES 4u

/// state = updates in the low 16 bits, the wake epoch of the last update in the high 16
uint GlimmerHistoryUpdates(uint state)
{
    const uint updates = state & 0xFFFFu;

    if ((state >> 16u) != constants.dispatch.z)
    {
        return min(updates, GLIMMER_SURFEL_WAKE_UPDATES);
    }

    return updates;
}

// the sun bounce has its own updates, which a wake cuts short the same way
float GlimmerSunBlendWeightAfterWake(uint historyState, uint sunUpdates)
{
    if ((historyState >> 16u) != constants.dispatch.z)
    {
        sunUpdates = min(sunUpdates, GLIMMER_SURFEL_WAKE_UPDATES);
    }

    return GlimmerSurfelSunBlendWeight(sunUpdates);
}

uint GlimmerNextHistoryState(uint updates)
{
    return min(updates + 1u, 0xFFFFu) | (constants.dispatch.z << 16u);
}

float GlimmerBlendWeight(uint updates)
{
    return 1.0 / (float(min(updates, max(constants.dispatch.y, 1u))) + 1.0);
}

// a surfel's sun visibility is the same every time it is traced, so it is stored as it was traced last
void StoreSurfelSunVisibility(uint surfelIndex, uint firstRayRecord)
{
    [unroll]
    for (uint packedIndex = 0; packedIndex < GLIMMER_SURFEL_SUN_VISIBILITY_UINTS; packedIndex++)
    {
        uint packed = 0u;

        [unroll]
        for (uint byteIndex = 0; byteIndex < 4u; byteIndex++)
        {
            const uint slot = packedIndex * 4u + byteIndex;

            if (slot < GLIMMER_SURFEL_SUN_SLOTS)
            {
                const float visibility = asfloat(rays[(firstRayRecord + slot) * GLIMMER_SURFEL_RAY_RECORD_UINTS + GLIMMER_SURFEL_RAY_SUN_VISIBILITY_UINT]);

                packed |= uint(saturate(visibility) * 255.0 + 0.5) << (byteIndex * 8u);
            }
        }

        OutSurfelData[GlimmerSurfelDataIndex(surfelIndex, GLIMMER_SURFEL_DATA_SUN_VISIBILITY + packedIndex)] = packed;
    }
}

void BlendSurfel(uint surfelIndex, float invOpen, float visible)
{
    GlimmerSurfelLight estimate;

    [unroll]
    for (uint part = 0; part < GLIMMER_LIGHT_PARTS; part++)
    {
        estimate.parts[part] = float3(gsSums[part * 3u].x, gsSums[part * 3u + 1u].x, gsSums[part * 3u + 2u].x) * invOpen;
    }

    [unroll]
    for (uint term = 0; term < GLIMMER_LIGHT_TERMS; term++)
    {
        estimate.direct[term] = gsSums[GLIMMER_LIGHT_PARTS * 3u + term].x * invOpen;
    }

    const float3 sunEstimate = float3(gsSums[GLIMMER_SURFEL_SUN_SUM].x, gsSums[GLIMMER_SURFEL_SUN_SUM + 1u].x, gsSums[GLIMMER_SURFEL_SUN_SUM + 2u].x) * invOpen;

    const uint2 state = OutSurfelStates[surfelIndex];
    const uint updates = GlimmerHistoryUpdates(state.x);

    GlimmerSurfelLight light = estimate;
    float blendedVisible = visible;

    if (updates > 0u)
    {
        const float blend = GlimmerBlendWeight(updates);

        const GlimmerSurfelLight previous = LoadSurfelLight(surfelIndex);

        [unroll]
        for (uint blendPart = 0; blendPart < GLIMMER_LIGHT_PARTS; blendPart++)
        {
            light.parts[blendPart] = lerp(previous.parts[blendPart], estimate.parts[blendPart], blend);
        }

        [unroll]
        for (uint blendTerm = 0; blendTerm < GLIMMER_LIGHT_TERMS; blendTerm++)
        {
            light.direct[blendTerm] = lerp(previous.direct[blendTerm], estimate.direct[blendTerm], blend);
        }

        blendedVisible = lerp(asfloat(state.y), visible, blend);
    }

    StoreSurfelLight(surfelIndex, light);

    float3 sun = LoadSurfelSun(surfelIndex);
    uint sunState = LoadSurfelSunState(surfelIndex);

    if (GlimmerSurfelHasSunKeys(constants.surfels))
    {
        BlendSurfelSunKeys(surfelIndex, updates, invOpen);
    }
    else
    {
        // a new surfel's sun bounce, or one from before the sun jumped, starts over; one for a sun that only turned a little follows it
        uint sunUpdates = 0u;

        if (updates > 0u)
        {
            sunUpdates = GlimmerSurfelSunUpdatesHeld(constants.surfels, sunState);
        }

        sun = sunEstimate;

        if (sunUpdates > 0u)
        {
            sun = lerp(LoadSurfelSun(surfelIndex), sunEstimate, GlimmerSunBlendWeightAfterWake(state.x, sunUpdates));
        }

        sunState = GlimmerSurfelNextSunState(constants.surfels, sunUpdates);

        StoreSurfelSun(surfelIndex, sun, sunState);
    }

    OutSurfelStates[surfelIndex] = uint2(GlimmerNextHistoryState(updates), asuint(blendedVisible));
    StoreSurfelComposed(surfelIndex, ComposeSurfel(surfelIndex, light, sun, sunState), blendedVisible);
}

// projected to L1 the way the probes were: the mean, and twice the mean times the direction
void BlendFoliage(uint voxelIndex, float invOpen, float visible)
{
    float4 light[GLIMMER_LIGHT_ENTRIES] = (float4[GLIMMER_LIGHT_ENTRIES])0.0;

    [unroll]
    for (uint part = 0; part < GLIMMER_LIGHT_PARTS; part++)
    {
        [unroll]
        for (uint channel = 0; channel < 3; channel++)
        {
            const float4 sum = gsSums[part * 3u + channel];

            GlimmerSetBounceEntry(light, part, channel, float4(sum.x * invOpen, sum.yzw * (2.0 * invOpen)));
        }
    }

    [unroll]
    for (uint term = 0; term < GLIMMER_LIGHT_TERMS; term++)
    {
        const float4 sum = gsSums[GLIMMER_LIGHT_PARTS * 3u + term];

        light[GLIMMER_LIGHT_DIRECT_ENTRY + term] = float4(sum.x * invOpen, sum.yzw * (2.0 * invOpen));
    }

    const uint historyState = OutFoliage[GlimmerFoliageIndex(voxelIndex, GLIMMER_FOLIAGE_STATE)];
    const uint updates = GlimmerHistoryUpdates(historyState);

    float blendedVisible = visible;

    if (updates > 0u)
    {
        const float blend = GlimmerBlendWeight(updates);

        float4 previous[GLIMMER_LIGHT_ENTRIES];
        LoadFoliageLight(voxelIndex, previous);

        [unroll]
        for (uint entry = 0; entry < GLIMMER_LIGHT_ENTRIES; entry++)
        {
            light[entry] = lerp(previous[entry], light[entry], blend);
        }

        blendedVisible = lerp(asfloat(OutFoliage[GlimmerFoliageIndex(voxelIndex, GLIMMER_FOLIAGE_VISIBLE)]), visible, blend);
    }

    float4 sunEstimate[3];

    [unroll]
    for (uint sunChannel = 0; sunChannel < 3; sunChannel++)
    {
        const float4 sum = gsSums[GLIMMER_SURFEL_SUN_SUM + sunChannel];

        sunEstimate[sunChannel] = float4(sum.x * invOpen, sum.yzw * (2.0 * invOpen));
    }

    const uint sunBase = GlimmerFoliageIndex(voxelIndex, GLIMMER_FOLIAGE_SUN);

    float4 sun[3];
    uint sunState;
    GLIMMER_LOAD_SUN_BOUNCE(OutFoliage, sunBase, sun, sunState);

    if (GlimmerSurfelHasSunKeys(constants.surfels))
    {
        // the voxel's trace was for every key; each key's sun bounce takes it in
        const uint keyStateIndex = GlimmerFoliageIndex(voxelIndex, GLIMMER_FOLIAGE_SUN_KEY_STATE);
        const uint keyUpdates = updates > 0u ? GlimmerSurfelSunKeyUpdates(constants.surfels, OutFoliage[keyStateIndex]) : 0u;
        const float keyBlend = GlimmerSurfelSunKeyBlendWeight(keyUpdates, 1u);

        [loop]
        for (uint key = 0; key < GLIMMER_SUN_KEYS; key++)
        {
            const uint keyAt = GlimmerFoliageIndex(voxelIndex, GLIMMER_FOLIAGE_SUN_KEYS + key * GLIMMER_SUN_KEY_L1_UINTS);

            float4 keySun[3];
            GlimmerUnpackSunKeyL1(uint2(OutFoliage[keyAt], OutFoliage[keyAt + 1u]), keySun);

            [unroll]
            for (uint keyChannel = 0; keyChannel < 3; keyChannel++)
            {
                const float4 sum = gsSums[GLIMMER_SURFEL_SUN_KEY_SUM + key * 3u + keyChannel];

                keySun[keyChannel] = lerp(keySun[keyChannel], float4(sum.x * invOpen, sum.yzw * (2.0 * invOpen)), keyBlend);
            }

            const uint2 packedKey = GlimmerPackSunKeyL1(keySun);

            OutFoliage[keyAt] = packedKey.x;
            OutFoliage[keyAt + 1u] = packedKey.y;
        }

        OutFoliage[keyStateIndex] = GlimmerSurfelNextSunKeyState(constants.surfels, keyUpdates);
    }
    else
    {
        uint sunUpdates = 0u;

        if (updates > 0u)
        {
            sunUpdates = GlimmerSurfelSunUpdatesHeld(constants.surfels, sunState);
        }

        const float sunBlend = sunUpdates > 0u ? GlimmerSunBlendWeightAfterWake(historyState, sunUpdates) : 1.0;

        [unroll]
        for (uint blendChannel = 0; blendChannel < 3; blendChannel++)
        {
            sun[blendChannel] = lerp(sun[blendChannel], sunEstimate[blendChannel], sunBlend);
        }

        sunState = GlimmerSurfelNextSunState(constants.surfels, sunUpdates);

        GLIMMER_STORE_SUN_BOUNCE(OutFoliage, sunBase, sun, sunState);
    }

    StoreFoliageLight(voxelIndex, light);
    StoreFoliageComposed(voxelIndex, light, sun, sunState);

    OutFoliage[GlimmerFoliageIndex(voxelIndex, GLIMMER_FOLIAGE_STATE)] = GlimmerNextHistoryState(updates);
    OutFoliage[GlimmerFoliageIndex(voxelIndex, GLIMMER_FOLIAGE_VISIBLE)] = asuint(blendedVisible);
}

/// a group per surfel or foliage voxel traced this frame, a thread per ray
[numthreads(GLIMMER_SURFEL_RAYS, 1, 1)]
void CSMain(uint3 groupId : SV_GroupID, uint groupIndex : SV_GroupIndex)
{
    const uint surfelCount = GlimmerSurfelCount(glimmerSurfelHash[GLIMMER_SURFEL_COUNTER_SURFELS]);
    const uint foliageCount = GlimmerFoliageCount(glimmerSurfelHash[GLIMMER_SURFEL_COUNTER_FOLIAGE]);
    const uint listIndex = groupId.x;

    // NOTE: no return before the barriers. WGSL wants every thread to reach them, and can't tell these are per-group decisions
    const bool isListed = listIndex < min(surfelCount + foliageCount, constants.surfels.info.x);
    const uint item = GlimmerTracedItem(constants.surfels, listIndex, surfelCount, foliageCount);
    const bool isFoliage = item >= surfelCount;

    if (groupIndex == 0u)
    {
        gsBlocked = 0u;
    }

    GroupMemoryBarrierWithGroupSync();

    if (isListed)
    {
        const uint rayIndex = groupIndex;
        const uint base = (constants.dispatch.x + listIndex * GLIMMER_SURFEL_RAYS + rayIndex) * GLIMMER_SURFEL_RAY_RECORD_UINTS;

        const float rayDistance = asfloat(rays[base]);

        GlimmerLight rayLight = GlimmerNoLight();
        float3 raySun = (float3)0.0;
        float escape = 0.0;

        const bool hasSunKeys = GlimmerSurfelHasSunKeys(constants.surfels);

        [loop]
        for (uint key = 0; key < GLIMMER_SUN_KEYS; key++)
        {
            gsRaySunKeys[rayIndex * GLIMMER_SUN_KEYS + key] = hasSunKeys && rayDistance > 0.0 ? GlimmerUnpackRGB9E5(rays[base + GLIMMER_SURFEL_RAY_SUN_KEYS_UINT + key]) : (float3)0.0;
        }

        if (rayDistance > 0.0)
        {
            escape = asfloat(rays[base + 1u]);
            raySun = GlimmerUnpackLightPart(uint2(rays[base + GLIMMER_SURFEL_RAY_SUN_UINT], rays[base + GLIMMER_SURFEL_RAY_SUN_UINT + 1u]));

            [unroll]
            for (uint part = 0; part < GLIMMER_LIGHT_PARTS; part++)
            {
                rayLight.parts[part] = GlimmerUnpackLightPart(uint2(rays[base + 2u + part * 2u], rays[base + 3u + part * 2u]));
            }
        }
        else
        {
            InterlockedAdd(gsBlocked, 1u);
        }

        float3 direction;

        if (isFoliage)
        {
            direction = GlimmerFoliageRayDirection(item - surfelCount, constants.surfels.info.z, rayIndex);
        }
        else
        {
            direction = GlimmerSurfelRayDirection(GlimmerUnpackSurfelNormal(glimmerSurfels[item].w), item, constants.surfels.info.z, rayIndex);
        }

        float basis[GLIMMER_LIGHT_TERMS];
        GlimmerLightBasis(direction, basis);

        [unroll]
        for (uint part = 0; part < GLIMMER_LIGHT_PARTS; part++)
        {
            gsRayParts[rayIndex * GLIMMER_LIGHT_PARTS + part] = rayLight.parts[part];
        }

        [unroll]
        for (uint term = 0; term < GLIMMER_LIGHT_TERMS; term++)
        {
            gsRayDirect[rayIndex * GLIMMER_LIGHT_TERMS + term] = basis[term] * escape;
        }

        gsRaySun[rayIndex] = raySun;
        gsDirections[rayIndex] = direction;
    }

    GroupMemoryBarrierWithGroupSync();

    if (isListed)
    {
        for (uint sumIndex = groupIndex; sumIndex < GLIMMER_SURFEL_SUMS; sumIndex += GLIMMER_SURFEL_RAYS)
        {
            float4 sum = (float4)0.0;

            if (sumIndex < GLIMMER_LIGHT_PARTS * 3u)
            {
                const uint part = sumIndex / 3u;
                const uint channel = sumIndex % 3u;

                for (uint rayIndex = 0; rayIndex < GLIMMER_SURFEL_RAYS; rayIndex++)
                {
                    const float value = gsRayParts[rayIndex * GLIMMER_LIGHT_PARTS + part][channel];

                    sum += float4(value, value * gsDirections[rayIndex]);
                }
            }
            else if (sumIndex >= GLIMMER_SURFEL_SUN_KEY_SUM)
            {
                const uint key = (sumIndex - GLIMMER_SURFEL_SUN_KEY_SUM) / 3u;
                const uint channel = (sumIndex - GLIMMER_SURFEL_SUN_KEY_SUM) % 3u;

                for (uint rayIndex = 0; rayIndex < GLIMMER_SURFEL_RAYS; rayIndex++)
                {
                    const float value = gsRaySunKeys[rayIndex * GLIMMER_SUN_KEYS + key][channel];

                    sum += float4(value, value * gsDirections[rayIndex]);
                }
            }
            else if (sumIndex >= GLIMMER_SURFEL_SUN_SUM)
            {
                const uint channel = sumIndex - GLIMMER_SURFEL_SUN_SUM;

                for (uint rayIndex = 0; rayIndex < GLIMMER_SURFEL_RAYS; rayIndex++)
                {
                    const float value = gsRaySun[rayIndex][channel];

                    sum += float4(value, value * gsDirections[rayIndex]);
                }
            }
            else
            {
                const uint term = sumIndex - GLIMMER_LIGHT_PARTS * 3u;

                for (uint rayIndex = 0; rayIndex < GLIMMER_SURFEL_RAYS; rayIndex++)
                {
                    const float value = gsRayDirect[rayIndex * GLIMMER_LIGHT_TERMS + term];

                    sum += float4(value, value * gsDirections[rayIndex]);
                }
            }

            gsSums[sumIndex] = sum;
        }
    }

    GroupMemoryBarrierWithGroupSync();

    if (!isListed || groupIndex != 0u)
    {
        return;
    }

    const uint numOpen = GLIMMER_SURFEL_RAYS - gsBlocked;
    const float invOpen = 1.0 / float(max(numOpen, 1u));

    const float hiddenFraction = float(gsBlocked) / float(GLIMMER_SURFEL_RAYS);
    const float visible = saturate(1.0 - hiddenFraction / GLIMMER_SURFEL_HIDDEN_FRACTION);

    if (isFoliage)
    {
        BlendFoliage(item - surfelCount, invOpen, visible);
    }
    else
    {
        BlendSurfel(item, invOpen, visible);
        StoreSurfelSunVisibility(item, constants.dispatch.x + listIndex * GLIMMER_SURFEL_RAYS);
    }
}

#endif
