#include "../../Include/Defines.hlsli"

#include "GlimmerSWRTCommon.hlsli"

#include "GlimmerProbeTypes.hlsli"

struct GlimmerProbeBlendConstants
{
    GlimmerProbeVolume volume;
    uint4 dispatch; // x = probes traced per frame at most, y = moves a probe gets to get out of a solid, z = estimates the history averages at least while its light holds, w = ...while it changes
    float4 params;  // x = seconds the history spans while the light holds, y = while it changes, z = how far past a back face a probe moves (m)
};

DECLARE_BUFFER_DYNAMIC(GlimmerProbeBlend, CBuffer) cbuffer CBuffer
{
    GlimmerProbeBlendConstants constants;
};

DECLARE_SRV(GlimmerProbeBlend, Rays) StructuredBuffer<float4> rays;
DECLARE_SRV(GlimmerProbeBlend, GlimmerProbeUpdateListBuffer) StructuredBuffer<uint> glimmerProbeUpdateList;
DECLARE_SRV(GlimmerProbeBlend, GlimmerProbeCountersBuffer) StructuredBuffer<uint> glimmerProbeCounters;
DECLARE_SRV(GlimmerProbeBlend, GlimmerProbeSlotsBuffer) StructuredBuffer<int4> glimmerProbeSlots;

DECLARE_UAV(GlimmerProbeBlend, OutSH) RWStructuredBuffer<float4> OutSH;
DECLARE_UAV(GlimmerProbeBlend, OutStates) RWStructuredBuffer<uint4> OutStates;
DECLARE_UAV(GlimmerProbeBlend, OutVisibility) RWStructuredBuffer<uint> OutVisibility;   // GLIMMER_PROBE_VISIBILITY_TEXELS per probe (GlimmerPackHalf2)
DECLARE_UAV(GlimmerProbeBlend, OutTrend) RWStructuredBuffer<float4> OutTrend;           // x = variance of an estimate's luminance, y = recent mean of how far estimates fall off the history (in standard deviations), z = recent mean of that distance squared (unscaled), w = estimates the history averages

#define GLIMMER_PROBES_NO_SAMPLING
#include "GlimmerProbes.hlsli"
#include "../GlimmerConstants.hlsli"

#define GROUP_SIZE GLIMMER_PROBE_VISIBILITY_TEXELS

groupshared float4 gsRays[GLIMMER_PROBE_MAX_RAYS];       // rgb = radiance, w = hit distance (negative on back faces, 0 starting inside)
groupshared float3 gsDirections[GLIMMER_PROBE_MAX_RAYS];
groupshared uint gsWriteVisibility;
groupshared float gsVisibilityBlend;

float GlimmerLuminance(float3 color)
{
    return dot(color, float3(0.2126, 0.7152, 0.0722));
}

float3 GlimmerClampOffset(float3 offset)
{
    return clamp(offset, -GLIMMER_PROBE_MAX_OFFSET, GLIMMER_PROBE_MAX_OFFSET);
}

void GlimmerBlendProbe(uint probeIndex, uint4 state, uint numRays, float invSpacing)
{
    uint probeState = GlimmerProbeStateOf(state);

    float3 e0 = (float3)0.0;
    float3 e1R = (float3)0.0;
    float3 e1G = (float3)0.0;
    float3 e1B = (float3)0.0;
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
        const float4 ray = gsRays[rayIndex];
        const float3 direction = gsDirections[rayIndex];

        if (ray.w < 0.0)
        {
            numBackfaces++;

            if (-ray.w < nearestBackface)
            {
                nearestBackface = -ray.w;
                nearestBackfaceDirection = direction;
            }

            continue;
        }
        
        if (ray.w == 0.0)
        {
            numStartsInside++;

            continue;
        }

        e0 += ray.rgb;
        e1R += ray.r * direction;
        e1G += ray.g * direction;
        e1B += ray.b * direction;
        luminanceSquaredSum += GlimmerLuminance(ray.rgb) * GlimmerLuminance(ray.rgb);
        numSeen++;

        freeDirectionSum += direction;

        if (ray.w < nearestFrontface)
        {
            nearestFrontface = ray.w;
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

    // inside a solid: move out and start over, a few times, before giving up on the probe
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

    // right up against a front face, half the probe's view is that face; nudge it off
    if (nearestFrontface * invSpacing < GLIMMER_PROBE_MIN_CLEARANCE)
    {
        offset = GlimmerClampOffset(offset - nearestFrontfaceDirection * (GLIMMER_PROBE_MIN_CLEARANCE - nearestFrontface * invSpacing));
    }
    else if (numBackfaces == 0u && numStartsInside == 0u && nearestFrontface * invSpacing > GLIMMER_PROBE_RETURN_CLEARANCE)
    {
        const float offsetLength = length(offset);

        if (offsetLength > 1e-4)
        {
            offset *= max(offsetLength - GLIMMER_PROBE_RETURN_STEP, 0.0) / offsetLength;
        }
    }

    const float invNumSeen = 1.0 / float(max(numSeen, 1u));

    float4 shR = float4(e0.r * invNumSeen, e1R * (2.0 * invNumSeen));
    float4 shG = float4(e0.g * invNumSeen, e1G * (2.0 * invNumSeen));
    float4 shB = float4(e0.b * invNumSeen, e1B * (2.0 * invNumSeen));

    float estimateLuminance = GlimmerLuminance(float3(shR.x, shG.x, shB.x));
   
    const float estimateVariance = max(luminanceSquaredSum * invNumSeen - estimateLuminance * estimateLuminance, 0.0) * invNumSeen;

    const float updateInterval = max(time - asfloat(state.z), 1e-3);

    const float maxHistory = clamp(constants.params.x / updateInterval, float(max(constants.dispatch.z, 1u)), GLIMMER_PROBE_MAX_HISTORY);
    const float minHistory = min(max(constants.params.y / updateInterval, float(constants.dispatch.w)), maxHistory);

    float4 trend = OutTrend[probeIndex];

    if (updates > 0u)
    {
        const float4 previousR = OutSH[probeIndex * 3u + 0u];
        const float4 previousG = OutSH[probeIndex * 3u + 1u];
        const float4 previousB = OutSH[probeIndex * 3u + 2u];

        const float historyLuminance = GlimmerLuminance(float3(previousR.x, previousG.x, previousB.x));

        const float sigma = max(sqrt(trend.x), GLIMMER_PROBE_MIN_RELATIVE_SIGMA * historyLuminance + 1e-5);
        const float difference = estimateLuminance - historyLuminance;

        const float deviation = difference / max(sqrt(trend.z), sigma);

        const float drift = lerp(trend.y, clamp(deviation, -GLIMMER_PROBE_DRIFT_STEP_DOWN, GLIMMER_PROBE_DRIFT_STEP_UP), GLIMMER_PROBE_DRIFT_RATE);

        // a history off by b standard deviations is worth no more than (1 / b)^2 estimates
        const float bias = drift * drift - GLIMMER_PROBE_DRIFT_NOISE;

        float history = min(trend.w, maxHistory);

        if (bias > 0.0)
        {
            history = min(history, max(1.0 / bias, minHistory));
        }

        if (history >= GLIMMER_PROBE_FIREFLY_MIN_HISTORY && difference > GLIMMER_PROBE_FIREFLY_SIGMAS * sigma)
        {
            const float scale = (historyLuminance + GLIMMER_PROBE_FIREFLY_SIGMAS * sigma) / max(estimateLuminance, 1e-6);

            shR *= scale;
            shG *= scale;
            shB *= scale;
        }

        const float blend = 1.0 / (history + 1.0);

        shR = lerp(previousR, shR, blend);
        shG = lerp(previousG, shG, blend);
        shB = lerp(previousB, shB, blend);

        trend = float4(
            lerp(trend.x, estimateVariance, GLIMMER_PROBE_NOISE_RATE),
            drift,
            lerp(trend.z, difference * difference, GLIMMER_PROBE_SPREAD_RATE),
            min(history + 1.0, maxHistory));
    }
    else
    {
        trend = float4(estimateVariance, 0.0, 2.0 * estimateVariance, 1.0);
    }

    if (relocations != 0u && updates >= GLIMMER_PROBE_SETTLE_UPDATES)
    {
        relocations = 0u;
    }

    OutSH[probeIndex * 3u + 0u] = shR;
    OutSH[probeIndex * 3u + 1u] = shG;
    OutSH[probeIndex * 3u + 2u] = shB;

    OutTrend[probeIndex] = trend;
    OutStates[probeIndex] = uint4(
        GlimmerPackProbeFlags(probeState, relocations, updates + 1u, numBackfaces, numStartsInside),
        GlimmerPackProbeOffset(offset),
        asuint(time),
        GlimmerPackGoodOffset(offset));

    // the depths only change with the geometry, which resets updates when it moves the probe, so they don't follow the light's history
    gsWriteVisibility = 1u;
    gsVisibilityBlend = 1.0 / (min(float(updates), maxHistory) + 1.0);
}

[numthreads(GROUP_SIZE, 1, 1)]
void CSMain(uint3 groupId : SV_GroupID, uint groupIndex : SV_GroupIndex)
{
    const uint listIndex = groupId.x;

    // no return before the barriers: WGSL wants every thread to reach them, and can't tell these are per-group decisions
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
        gsRays[groupIndex] = rays[listIndex * numRays + groupIndex];
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
        const float depth = min(abs(gsRays[rayIndex].w) * invSpacing, GLIMMER_PROBE_DEPTH_RANGE);

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
