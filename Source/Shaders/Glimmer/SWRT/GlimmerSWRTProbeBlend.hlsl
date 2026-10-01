#include "../../Include/Defines.hlsli"

#include "GlimmerSWRTCommon.hlsli"

#include "GlimmerProbeTypes.hlsli"

struct GlimmerProbeBlendConstants
{
    GlimmerProbeVolume volume;
    uint4 dispatch; // x = probes traced per frame at most, y = moves a probe gets to get out of a solid
    float4 params;  // x = seconds for the history to fade while the light holds, y = while it changes, z = how far past a back face a probe moves (m)
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
DECLARE_UAV(GlimmerProbeBlend, OutTrend) RWStructuredBuffer<float4> OutTrend;           // x = mean luminance of recent estimates, y = mean of its square

#define GLIMMER_PROBES_NO_SAMPLING
#include "GlimmerProbes.hlsli"

//////////CONSTANTS//////////

// a quarter of a probe's rays on back faces puts it inside a solid
#define GLIMMER_PROBE_INSIDE_FRACTION 0.25
// closer than this (in spacings) to a front face, a probe is nudged off it
#define GLIMMER_PROBE_MIN_CLEARANCE 0.1
// how quickly the luminance statistics follow the estimates
#define GLIMMER_PROBE_TREND_RATE 0.125
// the history counts as changed when an estimate is this many standard deviations off it...
#define GLIMMER_PROBE_CHANGE_SIGMAS 3.0
// ...or when this many estimates in a row fall on the same side of it, which is how slow drift (bounce light building up) shows
#define GLIMMER_PROBE_DRIFT_UPDATES 6
// once settled, an estimate can't be brighter than this many times the history
#define GLIMMER_PROBE_FIREFLY_RATIO 4.0
// how narrowly a visibility texel takes the rays around its direction (the power of their cosine); a few rays per update have to
// reach every texel, so it's far wider than DDGI's
#define GLIMMER_PROBE_VISIBILITY_SHARPNESS 12.0

#define GLIMMER_PROBE_MAX_RAYS 32
/////////////////////////////

float GlimmerLuminance(float3 color)
{
    return dot(color, float3(0.2126, 0.7152, 0.0722));
}

float3 GlimmerClampOffset(float3 offset)
{
    return clamp(offset, -GLIMMER_PROBE_MAX_OFFSET, GLIMMER_PROBE_MAX_OFFSET);
}

[numthreads(64, 1, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint listIndex = dispatchThreadId.x;

    if (listIndex >= min(glimmerProbeCounters[0], constants.dispatch.x))
    {
        return;
    }

    const uint probeIndex = glimmerProbeUpdateList[listIndex];
    const int4 slot = glimmerProbeSlots[probeIndex / GLIMMER_PROBES_PER_BLOCK];

    uint4 state = OutStates[probeIndex];
    uint probeState = GlimmerProbeStateOf(state);

    if (slot.w < 0 || probeState == GLIMMER_PROBE_STATE_FREE || probeState == GLIMMER_PROBE_STATE_BURIED || probeState == GLIMMER_PROBE_STATE_IDLE)
    {
        return;
    }

    const float spacing = constants.volume.levels[slot.w].params.x;
    const float invSpacing = constants.volume.levels[slot.w].params.y;

    const uint numRays = min(constants.volume.info.y, GLIMMER_PROBE_MAX_RAYS);

    float3 e0 = (float3)0.0;
    float3 e1R = (float3)0.0;
    float3 e1G = (float3)0.0;
    float3 e1B = (float3)0.0;

    float3 rayDirections[GLIMMER_PROBE_MAX_RAYS];
    float rayDepths[GLIMMER_PROBE_MAX_RAYS];

    uint numBackfaces = 0u;
    uint numStartsInside = 0u;

    float nearestBackface = 1e30;
    float3 nearestBackfaceDirection = (float3)0.0;

    float nearestFrontface = 1e30;
    float3 nearestFrontfaceDirection = (float3)0.0;

    for (uint rayIndex = 0; rayIndex < numRays; rayIndex++)
    {
        const float4 ray = rays[listIndex * numRays + rayIndex];
        const float3 direction = GlimmerProbeRayDirection(constants.volume, probeIndex, rayIndex);

        e0 += ray.rgb;
        e1R += ray.r * direction;
        e1G += ray.g * direction;
        e1B += ray.b * direction;

        // negative distances are back faces, 0 is a ray that started under the ground
        const float hitDistance = abs(ray.w);

        if (ray.w <= 0.0)
        {
            if (ray.w < 0.0)
            {
                numBackfaces++;
            }
            else
            {
                numStartsInside++;
            }

            if (hitDistance < nearestBackface)
            {
                nearestBackface = hitDistance;
                nearestBackfaceDirection = direction;
            }
        }
        else if (hitDistance < nearestFrontface)
        {
            nearestFrontface = hitDistance;
            nearestFrontfaceDirection = direction;
        }

        rayDirections[rayIndex] = direction;
        rayDepths[rayIndex] = min(hitDistance * invSpacing, GLIMMER_PROBE_DEPTH_RANGE);
    }

    uint relocations = GlimmerProbeRelocations(state);
    uint updates = GlimmerProbeUpdates(state);
    float3 offset = GlimmerUnpackProbeOffset(state.y);

    const float time = constants.volume.params.x;

    // inside a solid: move past the nearest back face and start over, a few times, before giving up on the probe
    if (float(numBackfaces + numStartsInside) >= GLIMMER_PROBE_INSIDE_FRACTION * float(numRays))
    {
        if (relocations < constants.dispatch.y)
        {
            offset = GlimmerClampOffset(offset + nearestBackfaceDirection * ((nearestBackface + constants.params.z) * invSpacing));

            OutStates[probeIndex] = uint4(GlimmerPackProbeFlags(GLIMMER_PROBE_STATE_ACTIVE, relocations + 1u, 0u, numBackfaces, numStartsInside), GlimmerPackProbeOffset(offset), asuint(time), 0u);
        }
        else
        {
            OutStates[probeIndex] = uint4(GlimmerPackProbeFlags(GLIMMER_PROBE_STATE_INSIDE, relocations, 0u, numBackfaces, numStartsInside), state.y, asuint(time), 0u);
        }

        return;
    }

    // a probe that was inside got out (the solid moved, or streamed out) and starts over
    if (probeState == GLIMMER_PROBE_STATE_INSIDE)
    {
        probeState = GLIMMER_PROBE_STATE_ACTIVE;
        updates = 0u;
    }

    // right up against a front face, half the probe's view is that face; nudge it off
    if (nearestFrontface * invSpacing < GLIMMER_PROBE_MIN_CLEARANCE)
    {
        offset = GlimmerClampOffset(offset - nearestFrontfaceDirection * (GLIMMER_PROBE_MIN_CLEARANCE - nearestFrontface * invSpacing));
    }

    const float invNumRays = 1.0 / float(max(numRays, 1u));

    float4 shR = float4(e0.r * invNumRays, e1R * (2.0 * invNumRays));
    float4 shG = float4(e0.g * invNumRays, e1G * (2.0 * invNumRays));
    float4 shB = float4(e0.b * invNumRays, e1B * (2.0 * invNumRays));

    float estimateLuminance = GlimmerLuminance(float3(shR.x, shG.x, shB.x));

    float4 trend = OutTrend[probeIndex];
    int drift = int(state.w);

    float hysteresis = 0.0;

    if (updates > 0u)
    {
        const float4 previousR = OutSH[probeIndex * 3u + 0u];
        const float4 previousG = OutSH[probeIndex * 3u + 1u];
        const float4 previousB = OutSH[probeIndex * 3u + 2u];

        const float historyLuminance = GlimmerLuminance(float3(previousR.x, previousG.x, previousB.x));

        // once the history means something, a single bright estimate (a sliver of sun) can't drag it far
        if (updates > 4u && estimateLuminance > GLIMMER_PROBE_FIREFLY_RATIO * historyLuminance + 1e-3)
        {
            const float scale = (GLIMMER_PROBE_FIREFLY_RATIO * historyLuminance + 1e-3) / estimateLuminance;

            shR *= scale;
            shG *= scale;
            shB *= scale;
            estimateLuminance *= scale;
        }

        trend.xy = lerp(trend.xy, float2(estimateLuminance, estimateLuminance * estimateLuminance), GLIMMER_PROBE_TREND_RATE);

        const float sigma = sqrt(max(trend.y - trend.x * trend.x, 0.0));

        const int side = estimateLuminance >= historyLuminance ? 1 : -1;
        drift = (drift * side > 0) ? clamp(drift + side, -15, 15) : side;

        const bool isChanging = abs(estimateLuminance - historyLuminance) > GLIMMER_PROBE_CHANGE_SIGMAS * sigma + 1e-4
            || abs(drift) >= GLIMMER_PROBE_DRIFT_UPDATES;

        const float elapsed = max(time - asfloat(state.z), 0.0);
        hysteresis = exp(-elapsed / max(isChanging ? constants.params.y : constants.params.x, 1e-3));
        hysteresis = min(hysteresis, float(updates) / float(updates + 1u));

        shR = lerp(shR, previousR, hysteresis);
        shG = lerp(shG, previousG, hysteresis);
        shB = lerp(shB, previousB, hysteresis);
    }
    else
    {
        trend = float4(estimateLuminance, estimateLuminance * estimateLuminance, 0.0, 0.0);
        drift = 0;
    }

    OutSH[probeIndex * 3u + 0u] = shR;
    OutSH[probeIndex * 3u + 1u] = shG;
    OutSH[probeIndex * 3u + 2u] = shB;

    for (uint texel = 0; texel < GLIMMER_PROBE_VISIBILITY_TEXELS; texel++)
    {
        const float2 texelUV = (float2(texel % GLIMMER_PROBE_VISIBILITY_RES, texel / GLIMMER_PROBE_VISIBILITY_RES) + 0.5) / float(GLIMMER_PROBE_VISIBILITY_RES);
        const float3 texelDirection = GlimmerOctahedralDecode(texelUV);

        float3 sums = (float3)0.0; // weighted depth, weighted depth squared, weight

        for (uint rayIndex = 0; rayIndex < numRays; rayIndex++)
        {
            const float weight = pow(saturate(dot(texelDirection, rayDirections[rayIndex])), GLIMMER_PROBE_VISIBILITY_SHARPNESS);
            const float depth = rayDepths[rayIndex];

            sums += float3(depth, depth * depth, 1.0) * weight;
        }

        const uint visibilityIndex = probeIndex * GLIMMER_PROBE_VISIBILITY_TEXELS + texel;

        if (sums.z > 1e-4)
        {
            const float2 estimate = sums.xy / sums.z;
            const float texelHysteresis = updates > 0u ? lerp(1.0, hysteresis, saturate(sums.z)) : 0.0;

            OutVisibility[visibilityIndex] = GlimmerPackHalf2(lerp(estimate, GlimmerUnpackHalf2(OutVisibility[visibilityIndex]), texelHysteresis));
        }
    }

    OutTrend[probeIndex] = trend;
    OutStates[probeIndex] = uint4(GlimmerPackProbeFlags(probeState, relocations, updates + 1u, numBackfaces, numStartsInside), GlimmerPackProbeOffset(offset), asuint(time), uint(drift));
}
