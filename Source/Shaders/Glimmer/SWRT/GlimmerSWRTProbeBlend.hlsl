#include "../../Include/Defines.hlsli"

PERMUTE(MODE, BLEND, COMPOSE)

#include "GlimmerSWRTCommon.hlsli"

#include "GlimmerProbeTypes.hlsli"
#include "../GlimmerLight.hlsli"

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

    for (uint entry = 0; entry < GLIMMER_LIGHT_ENTRIES; entry++)
    {
        light[entry] = OutLight[probeIndex * GLIMMER_LIGHT_ENTRIES + entry];
    }

    GlimmerWriteComposed(probeIndex, light);
}

#else

DECLARE_SRV(GlimmerProbeBlend, Rays) StructuredBuffer<uint> rays;
DECLARE_SRV(GlimmerProbeBlend, GlimmerProbeUpdateListBuffer) StructuredBuffer<uint> glimmerProbeUpdateList;
DECLARE_SRV(GlimmerProbeBlend, GlimmerProbeCountersBuffer) StructuredBuffer<uint> glimmerProbeCounters;
DECLARE_SRV(GlimmerProbeBlend, GlimmerProbeSlotsBuffer) StructuredBuffer<int4> glimmerProbeSlots;

DECLARE_UAV(GlimmerProbeBlend, OutStates) RWStructuredBuffer<uint4> OutStates;
DECLARE_UAV(GlimmerProbeBlend, OutVisibility) RWStructuredBuffer<uint> OutVisibility;   // GLIMMER_PROBE_VISIBILITY_TEXELS per probe (GlimmerPackHalf2)
DECLARE_UAV(GlimmerProbeBlend, OutBlockWake) RWStructuredBuffer<uint> OutBlockWake;

/// x = variance of an estimate's luminance, relative to the luminance squared
/// y = recent mean of how far estimates fall off the history (in standard deviations)
/// z = recent mean of that distance squared (unscaled), relative like x
/// w = estimates the history averages
DECLARE_UAV(GlimmerProbeBlend, OutTrend) RWStructuredBuffer<float4> OutTrend;

#define GLIMMER_PROBES_NO_SAMPLING
#include "GlimmerProbes.hlsli"
#include "../GlimmerConstants.hlsli"
#include "../GlimmerCommon.hlsli"

// what the relative trend values are relative to at least
#define GLIMMER_PROBE_MIN_TREND_LUMINANCE 1e-3

/// hit distance (negative on back faces, 0 starting inside)
groupshared float gsRayDistances[GLIMMER_PROBE_MAX_RAYS];
groupshared float gsRayEscapes[GLIMMER_PROBE_MAX_RAYS];
groupshared float3 gsRayLight[GLIMMER_PROBE_MAX_RAYS * GLIMMER_LIGHT_PARTS];

groupshared float3 gsDirections[GLIMMER_PROBE_MAX_RAYS];
groupshared uint gsWriteVisibility;
groupshared float gsVisibilityBlend;

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

void GlimmerBlendProbe(uint probeIndex, uint4 state, uint numRays, float invSpacing)
{
    uint probeState = GlimmerProbeStateOf(state);

    float3 e0[GLIMMER_LIGHT_PARTS] = (float3[GLIMMER_LIGHT_PARTS])0.0;
    float3 e1R[GLIMMER_LIGHT_PARTS] = (float3[GLIMMER_LIGHT_PARTS])0.0;
    float3 e1G[GLIMMER_LIGHT_PARTS] = (float3[GLIMMER_LIGHT_PARTS])0.0;
    float3 e1B[GLIMMER_LIGHT_PARTS] = (float3[GLIMMER_LIGHT_PARTS])0.0;

    float4 direct[GLIMMER_LIGHT_TERMS] = (float4[GLIMMER_LIGHT_TERMS])0.0;

    float luminanceSquaredSum = 0.0;
    uint numSeen = 0u;

    uint numBackfaces = 0u;
    uint numStartsInside = 0u;

    float nearestBackface = 1e30;
    float3 nearestBackfaceDirection = (float3)0.0;

    float nearestFrontface = 1e30;
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

        const float escape = gsRayEscapes[rayIndex];

        GlimmerLight rayLight;

        [unroll]
        for (uint part = 0; part < GLIMMER_LIGHT_PARTS; part++)
        {
            const float3 value = gsRayLight[rayIndex * GLIMMER_LIGHT_PARTS + part];

            rayLight.parts[part] = value;

            e0[part] += value;
            e1R[part] += value.r * direction;
            e1G[part] += value.g * direction;
            e1B[part] += value.b * direction;
        }

        // a ray that got out brings the distant light along it straight in
        float basis[GLIMMER_LIGHT_TERMS];
        GlimmerLightBasis(direction, basis);

        [unroll]
        for (uint term = 0; term < GLIMMER_LIGHT_TERMS; term++)
        {
            direct[term] += float4(1.0, direction) * (basis[term] * escape);
        }

        const float rayLuminance = GlimmerLuminance(GlimmerComposeRayLight(constants.light, rayLight, direction, escape));

        luminanceSquaredSum += rayLuminance * rayLuminance;
        numSeen++;

        freeDirectionSum += direction;

        if (rayDistance < nearestFrontface)
        {
            nearestFrontface = rayDistance;
            nearestFrontfaceDirection = direction;
        }
    }

    uint relocations = GlimmerProbeRelocations(state);
    uint updates = GlimmerProbeUpdates(state);
    float3 offset = GlimmerUnpackProbeOffset(state.y);

    const float time = constants.volume.params.x;

    const float insideFraction = select(
        probeState == GLIMMER_PROBE_STATE_INSIDE,
        GLIMMER_PROBE_OUTSIDE_FRACTION,
        GLIMMER_PROBE_INSIDE_FRACTION);

    // inside a solid.
    // move out and start over, a few times before giving up.
    if (float(numBackfaces + numStartsInside) >= insideFraction * float(numRays))
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

    const float invNumSeen = 1.0 / float(max(numSeen, 1u));

    float4 light[GLIMMER_LIGHT_ENTRIES] = (float4[GLIMMER_LIGHT_ENTRIES])0.0;

    [unroll]
    for (uint projectPart = 0; projectPart < GLIMMER_LIGHT_PARTS; projectPart++)
    {
        GlimmerSetBounceEntry(light, projectPart, 0u, float4(e0[projectPart].r * invNumSeen, e1R[projectPart] * (2.0 * invNumSeen)));
        GlimmerSetBounceEntry(light, projectPart, 1u, float4(e0[projectPart].g * invNumSeen, e1G[projectPart] * (2.0 * invNumSeen)));
        GlimmerSetBounceEntry(light, projectPart, 2u, float4(e0[projectPart].b * invNumSeen, e1B[projectPart] * (2.0 * invNumSeen)));
    }

    [unroll]
    for (uint projectTerm = 0; projectTerm < GLIMMER_LIGHT_TERMS; projectTerm++)
    {
        light[GLIMMER_LIGHT_DIRECT_ENTRY + projectTerm] = float4(
            direct[projectTerm].x * invNumSeen,
            direct[projectTerm].yzw * (2.0 * invNumSeen));
    }

    const float estimateLuminance = GlimmerComposedLuminance(light);

    const float estimateVariance = max(luminanceSquaredSum * invNumSeen - estimateLuminance * estimateLuminance, 0.0) * invNumSeen;

    const float updateInterval = max(time - asfloat(state.z), 1e-3);

    const float maxHistory = clamp(constants.params.x / updateInterval, float(max(constants.dispatch.z, 1u)), GLIMMER_PROBE_MAX_HISTORY);
    const float minHistory = min(max(constants.params.y / updateInterval, float(constants.dispatch.w)), maxHistory);

    float4 trend = OutTrend[probeIndex];

    bool idle = false;

    if (updates > 0u)
    {
        float4 previous[GLIMMER_LIGHT_ENTRIES];

        for (uint loadEntry = 0; loadEntry < GLIMMER_LIGHT_ENTRIES; loadEntry++)
        {
            previous[loadEntry] = OutLight[probeIndex * GLIMMER_LIGHT_ENTRIES + loadEntry];
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
            OutBlockWake[probeIndex / GLIMMER_PROBES_PER_BLOCK] = constants.volume.info.z;
        }

        float fireflyScale = 1.0;

        if (history >= GLIMMER_PROBE_FIREFLY_MIN_HISTORY && difference > GLIMMER_PROBE_FIREFLY_SIGMAS * sigma)
        {
            fireflyScale = (historyLuminance + GLIMMER_PROBE_FIREFLY_SIGMAS * sigma) / max(estimateLuminance, 1e-6);
        }

        const float blend = 1.0 / (history + 1.0);

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

        trend = float4(relativeVariance, 0.0, 2.0 * relativeVariance, 1.0);
    }

    if (relocations != 0u && updates >= GLIMMER_PROBE_SETTLE_UPDATES)
    {
        relocations = 0u;
    }

    for (uint storeEntry = 0; storeEntry < GLIMMER_LIGHT_ENTRIES; storeEntry++)
    {
        OutLight[probeIndex * GLIMMER_LIGHT_ENTRIES + storeEntry] = light[storeEntry];
    }

    GlimmerWriteComposed(probeIndex, light);

    if (moved)
    {
        trend.w = min(trend.w, GLIMMER_PROBE_MOVED_HISTORY);
    }

    const uint sleepInterval = idle && relocations == 0u && !moved
        ? min(GlimmerProbeSleepInterval(state) + 1u, uint(constants.params.w))
        : 0u;

    OutTrend[probeIndex] = trend;
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

    const uint probeIndex = isListed ? glimmerProbeUpdateList[listIndex] : 0u;
    const int4 slot = glimmerProbeSlots[probeIndex / GLIMMER_PROBES_PER_BLOCK];

    const uint4 state = OutStates[probeIndex];
    const uint probeState = GlimmerProbeStateOf(state);

    const bool isUpdated = isListed && slot.w >= 0 && (probeState == GLIMMER_PROBE_STATE_ACTIVE || probeState == GLIMMER_PROBE_STATE_INSIDE);

    const float invSpacing = constants.volume.levels[max(slot.w, 0)].params.y;
    const uint numRays = min(constants.volume.info.y, GLIMMER_PROBE_MAX_RAYS);

    if (isUpdated && groupIndex < numRays)
    {
        const uint base = (listIndex * numRays + groupIndex) * GLIMMER_RAY_UINTS;

        gsRayDistances[groupIndex] = asfloat(rays[base]);
        gsRayEscapes[groupIndex] = asfloat(rays[base + 1u]);

        [unroll]
        for (uint part = 0; part < GLIMMER_LIGHT_PARTS; part++)
        {
            gsRayLight[groupIndex * GLIMMER_LIGHT_PARTS + part] = GlimmerUnpackLightPart(uint2(rays[base + 2u + part * 2u], rays[base + 3u + part * 2u]));
        }

        gsDirections[groupIndex] = GlimmerProbeRayDirection(constants.volume, probeIndex, groupIndex);
    }

    GroupMemoryBarrierWithGroupSync();

    if (isUpdated && groupIndex == 0u)
    {
        GlimmerBlendProbe(probeIndex, state, numRays, invSpacing);
    }

    GroupMemoryBarrierWithGroupSync();

    if (!isUpdated || gsWriteVisibility == 0u)
    {
        return;
    }

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

    if (sums.z > 1e-4)
    {
        const uint visibilityIndex = probeIndex * GLIMMER_PROBE_VISIBILITY_TEXELS + texel;

        const float2 estimate = sums.xy / sums.z;
        const float texelBlend = gsVisibilityBlend < 1.0 ? gsVisibilityBlend * saturate(sums.z) : 1.0;

        OutVisibility[visibilityIndex] = GlimmerPackHalf2(lerp(GlimmerUnpackHalf2(OutVisibility[visibilityIndex]), estimate, texelBlend));
    }
}

#endif
