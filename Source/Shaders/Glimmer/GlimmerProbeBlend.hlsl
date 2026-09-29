#include "../Include/Defines.hlsli"

#include "GlimmerCommon.hlsli"

#include "GlimmerProbeTypes.hlsli"

struct GlimmerProbeBlendConstants
{
    GlimmerProbeVolume volume;
    uint4 dispatch; // x = cascade
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

#define GLIMMER_PROBES_NO_SAMPLING
#include "GlimmerProbes.hlsli"

// Projects each probe's rays onto L1, already convolved with the cosine lobe and divided by pi:
// with N uniformly spread rays, e0 = mean radiance and e1 = 2 * mean(radiance * direction)
[numthreads(64, 1, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint probeIndex = dispatchThreadId.x;

    if (probeIndex >= GLIMMER_PROBES_PER_CASCADE)
    {
        return;
    }

    const uint cascadeIndex = constants.dispatch.x;
    const GlimmerProbeCascade cascade = constants.volume.cascades[cascadeIndex];

    int2 localColumn;
    uint layer;
    GlimmerProbeFromIndex(probeIndex, localColumn, layer);

    const int2 column = cascade.gridOrigin.xy + localColumn;
    const float3 position = GlimmerProbePosition(cascade, cascadeIndex, column, layer);

    const uint numRays = constants.volume.info.y;

    float3 e0 = (float3)0.0;
    float3 e1R = (float3)0.0;
    float3 e1G = (float3)0.0;
    float3 e1B = (float3)0.0;

    for (uint rayIndex = 0; rayIndex < numRays; rayIndex++)
    {
        const float3 radiance = rays[probeIndex * numRays + rayIndex].rgb;
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

    const uint3 texel = GlimmerProbeTexel(cascadeIndex, column, layer);

    // a probe that was traced for another column (it scrolled in) or at another height starts over
    const uint2 state = OutProbeState[texel];

    const bool isSameProbe = state.x == GlimmerPackColumn(column)
        && abs(asfloat(state.y) - position.y) <= 0.25 * cascade.params.y;

    if (isSameProbe)
    {
        const float hysteresis = cascade.params.z;

        shR = lerp(shR, OutProbeSH0[texel], hysteresis);
        shG = lerp(shG, OutProbeSH1[texel], hysteresis);
        shB = lerp(shB, OutProbeSH2[texel], hysteresis);
    }

    OutProbeSH0[texel] = shR;
    OutProbeSH1[texel] = shG;
    OutProbeSH2[texel] = shB;
    OutProbeState[texel] = uint2(GlimmerPackColumn(column), asuint(position.y));
}
