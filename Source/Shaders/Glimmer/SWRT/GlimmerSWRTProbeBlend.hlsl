#include "../../Include/Defines.hlsli"

#include "GlimmerSWRTCommon.hlsli"

#include "GlimmerProbeTypes.hlsli"

struct GlimmerProbeBlendConstants
{
    GlimmerProbeVolume volume;
    uint4 cascadeDispatches[GLIMMER_PROBE_CASCADES]; // x = GLIMMER_PROBE_DISPATCH_*, y = slice period | slice << 16, z = probes
};

DECLARE_BUFFER_DYNAMIC(GlimmerProbeBlend, CBuffer) cbuffer CBuffer
{
    GlimmerProbeBlendConstants constants;
};

DECLARE_SRV(GlimmerProbeBlend, Rays) StructuredBuffer<float4> rays;
DECLARE_SRV(GlimmerProbeBlend, GlimmerProbeBaseTexture) Texture2DArray<float> glimmerProbeBase;

DECLARE_UAV(GlimmerProbeBlend, OutProbeSH0) RWTexture3D<float4> OutProbeSH0;
DECLARE_UAV(GlimmerProbeBlend, OutProbeSH1) RWTexture3D<float4> OutProbeSH1;
DECLARE_UAV(GlimmerProbeBlend, OutProbeSH2) RWTexture3D<float4> OutProbeSH2;
DECLARE_UAV(GlimmerProbeBlend, OutProbeState) RWTexture3D<uint2> OutProbeState;
DECLARE_UAV(GlimmerProbeBlend, OutProbeTrend) RWTexture3D<float> OutProbeTrend; // luminance over the last few updates

#define GLIMMER_PROBES_NO_SAMPLING
#include "GlimmerProbes.hlsli"

// Projects each probe's rays onto L1, already convolved with the cosine lobe and divided by pi:
// with N uniformly spread rays, e0 = mean radiance and e1 = 2 * mean(radiance * direction)
[numthreads(64, 1, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    // a row of groups per cascade
    const uint cascadeIndex = dispatchThreadId.y;
    const uint4 cascadeDispatch = constants.cascadeDispatches[cascadeIndex];
    const uint2 slice = uint2(cascadeDispatch.y & 0xFFFFu, cascadeDispatch.y >> 16);

    if (dispatchThreadId.x >= cascadeDispatch.z)
    {
        return;
    }

    const uint probeIndex = GlimmerDispatchedProbe(dispatchThreadId.x, cascadeDispatch.x, slice);
    const GlimmerProbeCascade cascade = constants.volume.cascades[cascadeIndex];

    int2 localColumn;
    uint layer;
    GlimmerProbeFromIndex(probeIndex, localColumn, layer);

    const int2 column = cascade.gridOrigin.xy + localColumn;
    const float3 position = GlimmerProbePosition(cascade, cascadeIndex, column, layer);

    const uint3 texel = GlimmerProbeTexel(cascadeIndex, column, layer);

    // a probe that was traced for another column (it scrolled in) or at another height starts over
    const uint2 state = OutProbeState[texel];

    const bool isSameProbe = GlimmerIsSameColumn(state.x, column)
        && abs(asfloat(state.y) - position.y) <= 0.25 * cascade.params.y;

    // a scroll dispatch traced just the probes that scrolled in and this frame's slice
    if (isSameProbe && cascadeDispatch.x == GLIMMER_PROBE_DISPATCH_SCROLLED && !GlimmerIsProbeInSlice(probeIndex, slice))
    {
        return;
    }

    const uint numRays = constants.volume.info.y;

    float3 e0 = (float3)0.0;
    float3 e1R = (float3)0.0;
    float3 e1G = (float3)0.0;
    float3 e1B = (float3)0.0;

    for (uint rayIndex = 0; rayIndex < numRays; rayIndex++)
    {
        const float3 radiance = rays[(cascadeIndex * GLIMMER_PROBES_PER_CASCADE + probeIndex) * numRays + rayIndex].rgb;
        const float3 direction = GlimmerProbeRayDirection(constants.volume, rayIndex);

        e0 += radiance;
        e1R += radiance.r * direction;
        e1G += radiance.g * direction;
        e1B += radiance.b * direction;
    }

    const float invNumRays = 1.0 / float(max(numRays, 1u));

    float4 shR = float4(e0.r * invNumRays, e1R * (2.0 * invNumRays));
    float4 shG = float4(e0.g * invNumRays, e1G * (2.0 * invNumRays));
    float4 shB = float4(e0.b * invNumRays, e1B * (2.0 * invNumRays));

    const float3 luminanceWeights = float3(0.2126, 0.7152, 0.0722);
    const float estimateLuminance = dot(float3(shR.x, shG.x, shB.x), luminanceWeights);

    uint updates = 1u;
    float trend = estimateLuminance;

    if (isSameProbe)
    {
        const float4 previousR = OutProbeSH0[texel];
        const float4 previousG = OutProbeSH1[texel];
        const float4 previousB = OutProbeSH2[texel];

        // a few rays per update make every estimate noisy: hold on to the history while the light only jitters around it, and
        // let it go when it has really changed (a light, a door, an occluder). One estimate can't tell the two apart, but the
        // last few updates' luminance can: noise averages out of it, a real change stays in it
        const float previousLuminance = dot(float3(previousR.x, previousG.x, previousB.x), luminanceWeights);

        trend = lerp(estimateLuminance, OutProbeTrend[texel], 0.6);

        const float relativeChange = abs(trend - previousLuminance) / max(max(trend, previousLuminance), 1e-4);

        float hysteresis = lerp(cascade.params.z, cascade.params.w, smoothstep(0.35, 0.6, relativeChange));

        // a probe that was only just placed averages its first updates evenly rather than trusting the first one
        const uint previousUpdates = GlimmerProbeUpdates(state.x);
        hysteresis = min(hysteresis, float(previousUpdates) / float(previousUpdates + 1u));

        updates = min(previousUpdates + 1u, GLIMMER_PROBE_MAX_UPDATES);

        shR = lerp(shR, previousR, hysteresis);
        shG = lerp(shG, previousG, hysteresis);
        shB = lerp(shB, previousB, hysteresis);
    }

    OutProbeSH0[texel] = shR;
    OutProbeSH1[texel] = shG;
    OutProbeSH2[texel] = shB;
    OutProbeState[texel] = uint2(GlimmerPackColumn(column) | (updates << 28), asuint(position.y));
    OutProbeTrend[texel] = trend;
}
