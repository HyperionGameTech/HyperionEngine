#include "../../Include/Defines.hlsli"

PERMUTE(MODE, BLEND, COMPOSE)

#include "../GlimmerCommon.hlsli"

#include "GlimmerProbeTypes.hlsli"
#include "../GlimmerLight.hlsli"
#include "../SH/GlimmerSHCommon.hlsli"

struct GlimmerProbeBlendConstants
{
    GlimmerProbeVolume volume;

    /// x = probes traced per frame at most
    /// y = moves a probe gets to get out of a solid
    /// z = estimates the history averages at least while its light holds
    /// w = same but while it changes
    uint4 dispatch;

    /// x = seconds the history spans while the light holds
    /// y = same but while it changes
    /// z = how far past a back face a probe moves (m)
    /// w = longest sleep interval
    float4 params;

    GlimmerLightParams light;

    GlimmerSHVolume sh;
};

DECLARE_BUFFER_DYNAMIC(GlimmerProbeBlend, CBuffer) cbuffer CBuffer
{
    GlimmerProbeBlendConstants constants;
};

DECLARE_UAV(GlimmerProbeBlend, OutLight) RWStructuredBuffer<float4> OutLight;

/// 3 per probe
DECLARE_UAV(GlimmerProbeBlend, OutSH) RWStructuredBuffer<float4> OutSH;

#define GROUP_SIZE GLIMMER_PROBE_VISIBILITY_TEXELS

void GlimmerWriteComposed(uint probeIndex, float4 entries[GLIMMER_LIGHT_ENTRIES])
{
    float4 composed[3];
    GlimmerComposeStoredLight(constants.light, entries, composed);

    OutSH[probeIndex * 3u + 0u] = composed[0];
    OutSH[probeIndex * 3u + 1u] = composed[1];
    OutSH[probeIndex * 3u + 2u] = composed[2];
}

#if defined(MODE_COMPOSE)

[numthreads(GROUP_SIZE, 1, 1)]
void CSMain(uint3 groupId : SV_GroupID, uint groupIndex : SV_GroupIndex)
{
    const uint probeIndex = groupId.x * GROUP_SIZE + groupIndex;

    if (probeIndex >= GLIMMER_PROBE_POOL_PROBES)
    {
        return;
    }

    float4 light[GLIMMER_LIGHT_ENTRIES];

    [unroll]
    for (uint entry = 0; entry < GLIMMER_LIGHT_ENTRIES; entry++)
    {
        light[entry] = OutLight[probeIndex * GLIMMER_LIGHT_ENTRIES + entry];
    }

    GlimmerWriteComposed(probeIndex, light);
}

#else

DECLARE_SRV(GlimmerProbeBlend, Rays) StructuredBuffer<uint> rays;
DECLARE_UAV(GlimmerProbeBlend, GlimmerProbeCountersBuffer) RWStructuredBuffer<uint> glimmerProbeCounters;
DECLARE_SRV(GlimmerProbeBlend, GlimmerProbeSlotsBuffer) StructuredBuffer<int4> glimmerProbeSlots;

DECLARE_UAV(GlimmerProbeBlend, OutStates) RWStructuredBuffer<uint4> OutStates;
DECLARE_UAV(GlimmerProbeBlend, OutVisibility) RWStructuredBuffer<uint> OutVisibility;   // GLIMMER_PROBE_VISIBILITY_TEXELS per probe (GlimmerPackHalf2)
DECLARE_UAV(GlimmerProbeBlend, OutSlotTimes) RWStructuredBuffer<uint2> OutSlotTimes;

/// x = variance of an estimate's luminance, relative to the luminance squared
/// y = recent mean of how far estimates fall off the history (in standard deviations)
/// z = recent mean of that distance squared (unscaled), relative like x
/// w = estimates the history averages
DECLARE_UAV(GlimmerProbeBlend, OutTrend) RWStructuredBuffer<float4> OutTrend;

#define GLIMMER_PROBES_NO_SAMPLING
#include "GlimmerProbes.hlsli"
#include "../GlimmerConstants.hlsli"
#include "../GlimmerCommon.hlsli"

DECLARE_SRV(GlimmerProbeBlend, GlimmerSHStateTexture) Texture3D<uint4> glimmerSHState;
DECLARE_SRV(GlimmerProbeBlend, GlimmerSHLightBuffer) StructuredBuffer<uint> glimmerSHLight;

#define GLIMMER_SH_LOAD_DATA(texel) ((float4)0.0)
#define GLIMMER_SH_LOAD_STATE(texel) glimmerSHState.Load(int4(texel, 0))
#define GLIMMER_SH_LOAD_RADIANCE(texel) ((float4)0.0)
#define GLIMMER_SH_LOAD_LIGHT(texel, entry) GLIMMER_SH_LOAD_LIGHT_FROM(glimmerSHLight, texel, entry)

#include "../SH/GlimmerSHSample.hlsli"

// what the relative trend values are relative to at least
#define GLIMMER_PROBE_MIN_TREND_LUMINANCE 1e-3

#define GLIMMER_PROBE_SEED_UPDATES 3.0

#define GLIMMER_PROBE_RAY_SUMS (GLIMMER_LIGHT_PARTS * 3 + GLIMMER_LIGHT_TERMS)

#if GLIMMER_PROBE_MAX_RAYS + GLIMMER_LIGHT_ENTRIES > GROUP_SIZE || GLIMMER_PROBE_RAY_SUMS > GROUP_SIZE
#error "uhoh"
#endif

float GlimmerSampleSHEntry(float3 P, uint entry, out float4 outValue)
{
    outValue = (float4)0.0;

    if (constants.sh.info.w == 0u)
    {
        return 0.0;
    }

    float remaining = 1.0;

    [loop]
    for (uint cascadeIndex = 0; cascadeIndex < GLIMMER_SH_CASCADES && remaining > 1e-3; cascadeIndex++)
    {
        GlimmerSHTaps taps;
        const float cascadeWeight = GlimmerSHGatherTaps(constants.sh, cascadeIndex, P, (float3)0.0, taps);

        if (cascadeWeight <= 0.0)
        {
            continue;
        }

        float4 value = (float4)0.0;

        [unroll]
        for (uint tap = 0; tap < 8; tap++)
        {
            [branch]
            if (taps.weights[tap] > 0.0)
            {
                value += GLIMMER_SH_LOAD_LIGHT(taps.texels[tap], entry) * taps.weights[tap];
            }
        }

        outValue += value * (cascadeWeight * remaining / taps.weightSum);
        remaining *= 1.0 - cascadeWeight;
    }

    const float coverage = 1.0 - remaining;

    if (coverage > 1e-4)
    {
        outValue /= coverage;
    }

    return coverage;
}

/// hit distance (negative on back faces, 0 starting inside)
groupshared float gsRayDistances[GLIMMER_PROBE_MAX_RAYS];
groupshared float gsRayLuminances[GLIMMER_PROBE_MAX_RAYS];
groupshared float3 gsRayLight[GLIMMER_PROBE_MAX_RAYS * GLIMMER_LIGHT_PARTS];
groupshared float gsRayBasis[GLIMMER_PROBE_MAX_RAYS * GLIMMER_LIGHT_TERMS];

groupshared float3 gsDirections[GLIMMER_PROBE_MAX_RAYS];
groupshared uint gsWriteVisibility;
groupshared float gsVisibilityBlend;

groupshared float4 gsSums[GLIMMER_PROBE_RAY_SUMS];
groupshared float4 gsEntries[GLIMMER_LIGHT_ENTRIES];

groupshared uint gsSeed;
groupshared float3 gsSeedPosition;
groupshared float4 gsSeedTrend;

float3 GlimmerClampOffset(float3 offset)
{
    return clamp(offset, -GLIMMER_PROBE_MAX_OFFSET, GLIMMER_PROBE_MAX_OFFSET);
}

float GlimmerComposedLuminance(float4 entries[GLIMMER_LIGHT_ENTRIES])
{
    float4 composed[3];
    GlimmerComposeStoredLight(constants.light, entries, composed);

    return GlimmerLuminance(float3(composed[0].x, composed[1].x, composed[2].x));
}

void GlimmerBlendProbe(uint probeIndex, int4 slot, uint4 state, uint numRays, float invSpacing)
{
    uint probeState = GlimmerProbeStateOf(state);

    float luminanceSquaredSum = 0.0;

    uint numBackfaces = 0u;
    uint numStartsInside = 0u;

    float nearestBackface = 1e30;
    float3 nearestBackfaceDirection = (float3)0.0;

    float nearestFrontface = 1e30;
    float farthestFrontface = 0.0;
    float3 nearestFrontfaceDirection = (float3)0.0;

    float3 freeDirectionSum = (float3)0.0;

    for (uint rayIndex = 0; rayIndex < numRays; rayIndex++)
    {
        const float rayDistance = gsRayDistances[rayIndex];
        const float3 direction = gsDirections[rayIndex];

        if (rayDistance < 0.0)
        {
            numBackfaces++;

            if (-rayDistance < nearestBackface)
            {
                nearestBackface = -rayDistance;
                nearestBackfaceDirection = direction;
            }

            continue;
        }

        if (rayDistance == 0.0)
        {
            numStartsInside++;

            continue;
        }

        const float rayLuminance = gsRayLuminances[rayIndex];

        luminanceSquaredSum += rayLuminance * rayLuminance;

        freeDirectionSum += direction;

        if (rayDistance < nearestFrontface)
        {
            nearestFrontface = rayDistance;
            nearestFrontfaceDirection = direction;
        }

        farthestFrontface = max(farthestFrontface, rayDistance);
    }

    uint relocations = GlimmerProbeRelocations(state);
    uint updates = GlimmerProbeUpdates(state);
    float3 offset = GlimmerUnpackProbeOffset(state.y);

    const bool isSealed = any(offset != 0.0) && farthestFrontface < GLIMMER_PROBE_SEALED_DISTANCE;

    const float time = constants.volume.params.x;

    const float insideFraction = select(
        probeState == GLIMMER_PROBE_STATE_INSIDE,
        GLIMMER_PROBE_OUTSIDE_FRACTION,
        GLIMMER_PROBE_INSIDE_FRACTION);

    // inside a solid.
    // move out and start over, a few times before giving up.
    if (isSealed || float(numBackfaces + numStartsInside) >= insideFraction * float(numRays))
    {
        const uint insideStreak = GlimmerProbeInsideStreak(state) + 1u;

        if (probeState == GLIMMER_PROBE_STATE_ACTIVE && updates > 0u && insideStreak < GLIMMER_PROBE_INSIDE_STREAK)
        {
            OutStates[probeIndex] = uint4(GlimmerWithInsideStreak(state.x, insideStreak), state.y, asuint(time), state.w);

            gsWriteVisibility = 0u;

            return;
        }

        if (relocations < constants.dispatch.y)
        {
            if (numBackfaces != 0u)
            {
                // past the nearest back face
                offset += nearestBackfaceDirection * ((nearestBackface + constants.params.z) * invSpacing);
            }
            else if (dot(freeDirectionSum, freeDirectionSum) > 1e-6)
            {
                // toward the rays that got out
                offset += normalize(freeDirectionSum) * GLIMMER_PROBE_FREE_STEP;
            }

            OutStates[probeIndex] = uint4(
                GlimmerPackProbeFlags(GLIMMER_PROBE_STATE_ACTIVE, relocations + 1u, 0u, numBackfaces, numStartsInside),
                GlimmerPackProbeOffset(GlimmerClampOffset(offset)),
                asuint(time),
                state.w);
        }
        else
        {
            OutStates[probeIndex] = uint4(GlimmerPackProbeFlags(GLIMMER_PROBE_STATE_INSIDE, relocations, 0u, numBackfaces, numStartsInside), state.y, asuint(time), state.w);
        }

        gsWriteVisibility = 0u;

        return;
    }

    if (probeState == GLIMMER_PROBE_STATE_INSIDE)
    {
        probeState = GLIMMER_PROBE_STATE_ACTIVE;
        updates = 0u;
        relocations = 0u;
    }

    const bool isClear = numBackfaces == 0u && numStartsInside == 0u;
    const uint tracedOffset = GlimmerPackProbeOffset(offset);

    float3 goodOffset;
    const bool hasGoodOffset = GlimmerUnpackGoodOffset(state.w, goodOffset);
    const bool isAtGoodOffset = hasGoodOffset && (state.w & 0x3FFFFFFFu) == tracedOffset;
    
    bool moved = false;
    bool isNudgedFromClear = false;
    
    uint goodOffsetWord = isClear
            ? (GlimmerPackGoodOffset(offset) | select(isAtGoodOffset, state.w & GLIMMER_PROBE_GOOD_OFFSET_HELD, 0u))
            : state.w;

    if (!isClear && hasGoodOffset && !isAtGoodOffset && (state.y & GLIMMER_PROBE_OFFSET_NUDGED) != 0u)
    {
        offset = goodOffset;
        goodOffsetWord = state.w | GLIMMER_PROBE_GOOD_OFFSET_HELD;
        moved = true;
    }
    // right up against a front face, half the probe's view is that face; nudge it off
    else if (nearestFrontface * invSpacing < GLIMMER_PROBE_MIN_CLEARANCE)
    {
        if ((goodOffsetWord & GLIMMER_PROBE_GOOD_OFFSET_HELD) == 0u)
        {
            const float3 nudged = GlimmerClampOffset(offset - nearestFrontfaceDirection * (GLIMMER_PROBE_MIN_CLEARANCE - nearestFrontface * invSpacing));

            isNudgedFromClear = isClear;
            moved = isClear && isAtGoodOffset && length(nudged - offset) > GLIMMER_PROBE_NUDGE_WAKE;
            offset = nudged;
        }
    }
    else if (isClear && nearestFrontface * invSpacing > GLIMMER_PROBE_RETURN_CLEARANCE)
    {
        const float offsetLength = length(offset);

        if (offsetLength > 1e-4)
        {
            offset *= max(offsetLength - GLIMMER_PROBE_RETURN_STEP, 0.0) / offsetLength;
        }
    }

    // back face rays and rays that start inside carry no light; averaging over them would darken probes near walls
    const uint numOpenRays = numRays - min(numBackfaces + numStartsInside, numRays);
    const float invNumRays = 1.0 / float(max(numOpenRays, 1u));

    float4 light[GLIMMER_LIGHT_ENTRIES] = (float4[GLIMMER_LIGHT_ENTRIES])0.0;

    [unroll]
    for (uint projectPart = 0; projectPart < GLIMMER_LIGHT_PARTS; projectPart++)
    {
        [unroll]
        for (uint channel = 0; channel < 3; channel++)
        {
            const float4 sum = gsSums[projectPart * 3u + channel];

            GlimmerSetBounceEntry(light, projectPart, channel, float4(sum.x * invNumRays, sum.yzw * (2.0 * invNumRays)));
        }
    }

    [unroll]
    for (uint projectTerm = 0; projectTerm < GLIMMER_LIGHT_TERMS; projectTerm++)
    {
        const float4 sum = gsSums[GLIMMER_LIGHT_PARTS * 3u + projectTerm];

        light[GLIMMER_LIGHT_DIRECT_ENTRY + projectTerm] = float4(sum.x * invNumRays, sum.yzw * (2.0 * invNumRays));
    }

    const float estimateLuminance = GlimmerComposedLuminance(light);

    const float estimateVariance = max(luminanceSquaredSum * invNumRays - estimateLuminance * estimateLuminance, 0.0) * invNumRays;

    const float updateInterval = max(time - asfloat(state.z), 1e-3);

    const float maxHistory = clamp(constants.params.x / updateInterval, float(max(constants.dispatch.z, 1u)), GLIMMER_PROBE_MAX_HISTORY);
    const float minHistory = min(max(constants.params.y / updateInterval, float(constants.dispatch.w)), maxHistory);

    float4 trend = OutTrend[probeIndex];

    bool idle = false;

    if (updates > 0u)
    {
        float4 previous[GLIMMER_LIGHT_ENTRIES];

        [unroll]
        for (uint loadEntry = 0; loadEntry < GLIMMER_LIGHT_ENTRIES; loadEntry++)
        {
            previous[loadEntry] = gsEntries[loadEntry];
        }

        const float historyLuminance = GlimmerComposedLuminance(previous);

        const float trendScale = max(abs(historyLuminance), GLIMMER_PROBE_MIN_TREND_LUMINANCE);
        const float trendScaleSquared = trendScale * trendScale;

        const float sigma = max(sqrt(trend.x) * trendScale, GLIMMER_PROBE_MIN_RELATIVE_SIGMA * abs(historyLuminance) + 1e-5);
        const float difference = estimateLuminance - historyLuminance;

        const float deviation = difference / max(sqrt(trend.z) * trendScale, sigma);

        const float drift = lerp(trend.y, clamp(deviation, -GLIMMER_PROBE_DRIFT_STEP_DOWN, GLIMMER_PROBE_DRIFT_STEP_UP), GLIMMER_PROBE_DRIFT_RATE);
        const float bias = drift * drift - GLIMMER_PROBE_DRIFT_NOISE;

        float history = min(trend.w, maxHistory);

        if (bias > 0.0)
        {
            history = min(history, max(1.0 / bias, minHistory));
        }

        idle = bias <= 0.0 && history + 1.0 >= maxHistory;

        if (bias > GLIMMER_PROBE_DRIFT_NOISE * GLIMMER_PROBE_WAKE_BLOCK_BIAS)
        {
            OutSlotTimes[probeIndex / GLIMMER_PROBES_PER_BLOCK].y = constants.volume.info.z;
        }

        float fireflyScale = 1.0;

        if (history >= GLIMMER_PROBE_FIREFLY_MIN_HISTORY && difference > GLIMMER_PROBE_FIREFLY_SIGMAS * sigma)
        {
            fireflyScale = (historyLuminance + GLIMMER_PROBE_FIREFLY_SIGMAS * sigma) / max(estimateLuminance, 1e-6);
        }

        const float blend = 1.0 / (history + 1.0);

        [unroll]
        for (uint blendEntry = 0; blendEntry < GLIMMER_LIGHT_ENTRIES; blendEntry++)
        {
            light[blendEntry] = lerp(previous[blendEntry], light[blendEntry] * fireflyScale, blend);
        }

        const float blendedScale = max(abs(GlimmerComposedLuminance(light)), GLIMMER_PROBE_MIN_TREND_LUMINANCE);
        const float invBlendedScaleSquared = 1.0 / (blendedScale * blendedScale);

        trend = float4(
            lerp(trend.x * trendScaleSquared, estimateVariance, GLIMMER_PROBE_NOISE_RATE) * invBlendedScaleSquared,
            drift,
            lerp(trend.z * trendScaleSquared, difference * difference, GLIMMER_PROBE_SPREAD_RATE) * invBlendedScaleSquared,
            min(history + 1.0, maxHistory));
    }
    else
    {
        const float estimateScale = max(abs(estimateLuminance), GLIMMER_PROBE_MIN_TREND_LUMINANCE);
        const float relativeVariance = estimateVariance / (estimateScale * estimateScale);

        // starting from the far field, so a new probe eases in from what was shown there rather than from one noisy estimate
        gsSeedPosition = GlimmerProbeGridPosition(
            constants.volume.levels[slot.w],
            GlimmerProbeCoord(slot.xyz, GlimmerLocalProbeOf(probeIndex))) + offset / invSpacing;

        trend = float4(relativeVariance, 0.0, 2.0 * relativeVariance, 1.0 + GLIMMER_PROBE_SEED_UPDATES);
    }

    if (relocations != 0u && updates >= GLIMMER_PROBE_SETTLE_UPDATES)
    {
        relocations = 0u;
    }
    
    trend.w = select(moved, min(trend.w, GLIMMER_PROBE_MOVED_HISTORY), trend.w);

    if (updates > 0u)
    {
        [unroll]
        for (uint storeEntry = 0; storeEntry < GLIMMER_LIGHT_ENTRIES; storeEntry++)
        {
            OutLight[probeIndex * GLIMMER_LIGHT_ENTRIES + storeEntry] = light[storeEntry];
        }

        GlimmerWriteComposed(probeIndex, light);

        OutTrend[probeIndex] = trend;
    }
    else
    {
        [unroll]
        for (uint seedEntry = 0; seedEntry < GLIMMER_LIGHT_ENTRIES; seedEntry++)
        {
            gsEntries[seedEntry] = light[seedEntry];
        }

        gsSeedTrend = trend;
        gsSeed = 1u;
    }

    const uint sleepInterval = idle && relocations == 0u && !moved
        ? min(GlimmerProbeSleepInterval(state) + 1u, uint(constants.params.w))
        : 0u;

    if (sleepInterval == 0u && GlimmerProbeSleepInterval(state) != 0u)
    {
        InterlockedAdd(glimmerProbeCounters[GLIMMER_PROBE_COUNTER_WOKEN_BY_LIGHT], 1u);
    }

    OutStates[probeIndex] = uint4(
        GlimmerWithSleepInterval(GlimmerPackProbeFlags(probeState, relocations, updates + 1u, numBackfaces, numStartsInside), sleepInterval),
        GlimmerPackProbeOffset(offset) | select(isNudgedFromClear, GLIMMER_PROBE_OFFSET_NUDGED, 0u),
        asuint(time),
        goodOffsetWord);

    // the depths only change with the geometry so they don't follow the light's history
    gsWriteVisibility = 1u;
    gsVisibilityBlend = 1.0 / (min(float(updates), maxHistory) + 1.0);
}

[numthreads(GROUP_SIZE, 1, 1)]
void CSMain(uint3 groupId : SV_GroupID, uint groupIndex : SV_GroupIndex)
{
    const uint listIndex = groupId.x;

    // NOTE: no return before the barriers.
    // WGSL wants every thread to reach them, and can't tell these are per-group decisions
    const bool isListed = listIndex < min(glimmerProbeCounters[0], constants.dispatch.x);

    const uint probeIndex = isListed ? glimmerProbeCounters[GLIMMER_PROBE_COUNTERS + listIndex] : 0u;
    const int4 slot = glimmerProbeSlots[probeIndex / GLIMMER_PROBES_PER_BLOCK];

    const uint4 state = OutStates[probeIndex];
    const uint probeState = GlimmerProbeStateOf(state);

    const bool isUpdated = isListed && slot.w >= 0 && (probeState == GLIMMER_PROBE_STATE_ACTIVE || probeState == GLIMMER_PROBE_STATE_INSIDE);

    const float invSpacing = constants.volume.levels[max(slot.w, 0)].params.y;
    const uint numRays = min(constants.volume.info.y, GLIMMER_PROBE_MAX_RAYS);

    if (isUpdated && groupIndex < numRays)
    {
        const uint base = (listIndex * numRays + groupIndex) * GLIMMER_PROBE_RAY_RECORD_UINTS;

        const float rayDistance = asfloat(rays[base]);
        const float3 direction = GlimmerProbeRayDirection(constants.volume, probeIndex, groupIndex);

        GlimmerLight rayLight = GlimmerNoLight();
        float escape = 0.0;
        float rayLuminance = 0.0;
        
        if (rayDistance > 0.0)
        {
            escape = asfloat(rays[base + 1u]);

            [unroll]
            for (uint part = 0; part < GLIMMER_LIGHT_PARTS; part++)
            {
                rayLight.parts[part] = GlimmerUnpackLightPart(uint2(rays[base + 2u + part * 2u], rays[base + 3u + part * 2u]));
            }

            rayLuminance = GlimmerLuminance(GlimmerComposeRayLight(constants.light, rayLight, direction, escape));
        }
        
        float basis[GLIMMER_LIGHT_TERMS];
        GlimmerLightBasis(direction, basis);

        [unroll]
        for (uint part = 0; part < GLIMMER_LIGHT_PARTS; part++)
        {
            gsRayLight[groupIndex * GLIMMER_LIGHT_PARTS + part] = rayLight.parts[part];
        }

        [unroll]
        for (uint term = 0; term < GLIMMER_LIGHT_TERMS; term++)
        {
            gsRayBasis[groupIndex * GLIMMER_LIGHT_TERMS + term] = basis[term] * escape;
        }

        gsRayDistances[groupIndex] = rayDistance;
        gsRayLuminances[groupIndex] = rayLuminance;
        gsDirections[groupIndex] = direction;
    }

    if (isUpdated && groupIndex >= GROUP_SIZE - GLIMMER_LIGHT_ENTRIES && GlimmerProbeUpdates(state) > 0u)
    {
        const uint entry = groupIndex - (GROUP_SIZE - GLIMMER_LIGHT_ENTRIES);

        gsEntries[entry] = OutLight[probeIndex * GLIMMER_LIGHT_ENTRIES + entry];
    }

    GroupMemoryBarrierWithGroupSync();

    if (isUpdated && groupIndex < GLIMMER_PROBE_RAY_SUMS)
    {
        float4 sum = (float4)0.0;

        if (groupIndex < GLIMMER_LIGHT_PARTS * 3u)
        {
            const uint part = groupIndex / 3u;
            const uint channel = groupIndex % 3u;

            for (uint rayIndex = 0; rayIndex < numRays; rayIndex++)
            {
                const float value = gsRayLight[rayIndex * GLIMMER_LIGHT_PARTS + part][channel];

                sum += float4(value, value * gsDirections[rayIndex]);
            }
        }
        else
        {
            const uint term = groupIndex - GLIMMER_LIGHT_PARTS * 3u;

            for (uint rayIndex = 0; rayIndex < numRays; rayIndex++)
            {
                sum += float4(1.0, gsDirections[rayIndex]) * gsRayBasis[rayIndex * GLIMMER_LIGHT_TERMS + term];
            }
        }

        gsSums[groupIndex] = sum;
    }

    GroupMemoryBarrierWithGroupSync();

    if (isUpdated && groupIndex == 0u)
    {
        gsSeed = 0u;

        GlimmerBlendProbe(probeIndex, slot, state, numRays, invSpacing);
    }

    GroupMemoryBarrierWithGroupSync();

    float seedCoverage = 0.0;

    if (isUpdated && gsSeed != 0u && groupIndex < GLIMMER_LIGHT_ENTRIES)
    {
        float4 seed;
        seedCoverage = GlimmerSampleSHEntry(gsSeedPosition, groupIndex, seed);

        const float seedUpdates = GLIMMER_PROBE_SEED_UPDATES * saturate(seedCoverage);
        const float4 seeded = lerp(gsEntries[groupIndex], seed, seedUpdates / (seedUpdates + 1.0));

        OutLight[probeIndex * GLIMMER_LIGHT_ENTRIES + groupIndex] = seeded;
        gsEntries[groupIndex] = seeded;
    }

    if (isUpdated && gsWriteVisibility != 0u)
    {
        const uint texel = groupIndex;
        const float2 texelUV = (float2(texel % GLIMMER_PROBE_VISIBILITY_RES, texel / GLIMMER_PROBE_VISIBILITY_RES) + 0.5) / float(GLIMMER_PROBE_VISIBILITY_RES);
        const float3 texelDirection = GlimmerOctahedralDecode(texelUV);

        float3 sums = (float3)0.0; // weighted depth, weighted depth squared, weight

        for (uint rayIndex = 0; rayIndex < numRays; rayIndex++)
        {
            const float weight = pow(saturate(dot(texelDirection, gsDirections[rayIndex])), GLIMMER_PROBE_VISIBILITY_SHARPNESS);
            const float depth = min(abs(gsRayDistances[rayIndex]) * invSpacing, GLIMMER_PROBE_DEPTH_RANGE);

            sums += float3(depth, depth * depth, 1.0) * weight;
        }

        const uint visibilityIndex = probeIndex * GLIMMER_PROBE_VISIBILITY_TEXELS + texel;

        if (gsVisibilityBlend >= 1.0)
        {
            const float2 estimate = sums.z > 1e-4
                ? sums.xy / sums.z
                : float2(GLIMMER_PROBE_DEPTH_RANGE, GLIMMER_PROBE_DEPTH_RANGE * GLIMMER_PROBE_DEPTH_RANGE);

            OutVisibility[visibilityIndex] = GlimmerPackHalf2(estimate);
        }
        else if (sums.z > 1e-4)
        {
            const float2 estimate = sums.xy / sums.z;

            OutVisibility[visibilityIndex] = GlimmerPackHalf2(lerp(GlimmerUnpackHalf2(OutVisibility[visibilityIndex]), estimate, gsVisibilityBlend * saturate(sums.z)));
        }
    }

    GroupMemoryBarrierWithGroupSync();

    if (isUpdated && gsSeed != 0u && groupIndex == 0u)
    {
        float4 light[GLIMMER_LIGHT_ENTRIES];

        [unroll]
        for (uint lightEntry = 0; lightEntry < GLIMMER_LIGHT_ENTRIES; lightEntry++)
        {
            light[lightEntry] = gsEntries[lightEntry];
        }

        GlimmerWriteComposed(probeIndex, light);

        float4 trend = gsSeedTrend;
        trend.w = min(trend.w, 1.0 + GLIMMER_PROBE_SEED_UPDATES * saturate(seedCoverage));

        OutTrend[probeIndex] = trend;
    }
}

#endif
