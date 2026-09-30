#ifndef GLIMMER_PROBES_HLSLI
#define GLIMMER_PROBES_HLSLI

#include "GlimmerProbeTypes.hlsli"

float GlimmerProbeColumnBase(uint cascadeIndex, int2 column)
{
    return glimmerProbeBase.Load(int4(GlimmerWrapProbeColumn(column), cascadeIndex, 0));
}

float3 GlimmerProbeColumnCenter(GlimmerProbeCascade cascade, int2 column)
{
    const float2 xz = (float2(column) + 0.5) * cascade.params.x;

    return float3(xz.x, 0.0, xz.y);
}

float3 GlimmerProbePosition(GlimmerProbeCascade cascade, uint cascadeIndex, int2 column, uint layer)
{
    float3 position = GlimmerProbeColumnCenter(cascade, column);
    position.y = GlimmerProbeColumnBase(cascadeIndex, column) + cascade.params.y * GlimmerProbeLayerHeights[layer];

    return position;
}

#ifndef GLIMMER_PROBES_NO_SAMPLING

float GlimmerSampleCascade(GlimmerProbeVolume volume, uint cascadeIndex, float3 P, float3 N, out float3 outIrradiance)
{
    outIrradiance = (float3)0.0;

    const GlimmerProbeCascade cascade = volume.cascades[cascadeIndex];

    if (cascade.gridOrigin.z == 0)
    {
        return 0.0;
    }

    const float spacing = cascade.params.x;
    const float layerScale = cascade.params.y;

    const float2 gridCoord = P.xz / spacing - 0.5;
    const int2 column0 = int2(floor(gridCoord));
    const float2 columnFraction = gridCoord - float2(column0);

    const int2 local0 = column0 - cascade.gridOrigin.xy;

    if (any(local0 < 0) || any(local0 >= GLIMMER_PROBE_GRID - 1))
    {
        return 0.0;
    }

    // fade over the outer columns so the next cascade takes over smoothly
    const float2 edgeDistance = min(float2(local0) + columnFraction, float2(GLIMMER_PROBE_GRID - 2, GLIMMER_PROBE_GRID - 2) - float2(local0) - columnFraction);
    const float horizontalWeight = saturate(min(edgeDistance.x, edgeDistance.y) / 3.0);

    const float base00 = GlimmerProbeColumnBase(cascadeIndex, column0);
    const float base10 = GlimmerProbeColumnBase(cascadeIndex, column0 + int2(1, 0));
    const float base01 = GlimmerProbeColumnBase(cascadeIndex, column0 + int2(0, 1));
    const float base11 = GlimmerProbeColumnBase(cascadeIndex, column0 + int2(1, 1));

    const float base = lerp(lerp(base00, base10, columnFraction.x), lerp(base01, base11, columnFraction.x), columnFraction.y);

    const float heightAboveBase = (P.y - base) / layerScale;

    // above the top layer, hand over to a coarser cascade
    const float verticalWeight = saturate((GlimmerProbeLayerHeights[GLIMMER_PROBE_LAYERS - 1] * 1.25 - heightAboveBase) / (GlimmerProbeLayerHeights[GLIMMER_PROBE_LAYERS - 1] * 0.25));

    if (horizontalWeight * verticalWeight <= 0.0)
    {
        return 0.0;
    }

    const float layerCoord = clamp(log(max(heightAboveBase, 1.0)) / log(3.0), 0.0, float(GLIMMER_PROBE_LAYERS - 1));
    const uint layer0 = min(uint(layerCoord), GLIMMER_PROBE_LAYERS - 2);
    const float layerFraction = layerCoord - float(layer0);

    float4 sumR = (float4)0.0;
    float4 sumG = (float4)0.0;
    float4 sumB = (float4)0.0;
    float weightSum = 0.0;

    [unroll]
    for (uint corner = 0; corner < 8; corner++)
    {
        const int2 offset = int2(corner & 1u, (corner >> 1) & 1u);
        const uint layerOffset = (corner >> 2) & 1u;

        const int2 column = column0 + offset;
        const uint layer = layer0 + layerOffset;

        const uint3 texel = GlimmerProbeTexel(cascadeIndex, column, layer);

        // only probes traced for this column hold anything meaningful
        const uint2 state = glimmerProbeState.Load(int4(texel, 0));

        if (state.x != GlimmerPackColumn(column))
        {
            continue;
        }

        const float3 probePosition = float3(GlimmerProbeColumnCenter(cascade, column).x, asfloat(state.y), GlimmerProbeColumnCenter(cascade, column).z);
        const float3 toProbe = probePosition - P;
        const float toProbeLength = length(toProbe);

        const float3 trilinear = float3(
            offset.x != 0 ? columnFraction.x : 1.0 - columnFraction.x,
            offset.y != 0 ? columnFraction.y : 1.0 - columnFraction.y,
            layerOffset != 0 ? layerFraction : 1.0 - layerFraction);

        float weight = trilinear.x * trilinear.y * trilinear.z;

        // probes behind the surface see what's behind it
        const float backface = toProbeLength > 1e-4 ? (dot(toProbe / toProbeLength, N) + 1.0) * 0.5 : 1.0;
        weight *= backface * backface + 0.2;

        weight = max(weight, 1e-6);

        sumR += glimmerProbeSH0.Load(int4(texel, 0)) * weight;
        sumG += glimmerProbeSH1.Load(int4(texel, 0)) * weight;
        sumB += glimmerProbeSH2.Load(int4(texel, 0)) * weight;
        weightSum += weight;
    }

    if (weightSum <= 1e-5)
    {
        return 0.0;
    }

    outIrradiance = GlimmerEvaluateL1(sumR / weightSum, sumG / weightSum, sumB / weightSum, N);

    return horizontalWeight * verticalWeight;
}

float4 SampleGlimmerProbes(GlimmerProbeVolume volume, float3 P, float3 N)
{
    if (volume.info.w == 0u)
    {
        return (float4)0.0;
    }

    float3 irradiance = (float3)0.0;
    float remaining = 1.0;

    [loop]
    for (uint cascadeIndex = 0; cascadeIndex < volume.info.x && remaining > 1e-3; cascadeIndex++)
    {
        float3 cascadeIrradiance;
        const float cascadeWeight = GlimmerSampleCascade(volume, cascadeIndex, P, N, cascadeIrradiance);

        if (cascadeWeight <= 0.0)
        {
            continue;
        }

        irradiance += cascadeIrradiance * cascadeWeight * remaining;
        remaining *= 1.0 - cascadeWeight;
    }

    const float coverage = 1.0 - remaining;

    if (coverage <= 1e-4)
    {
        return (float4)0.0;
    }

    return float4(irradiance / coverage, coverage);
}

#endif // GLIMMER_PROBES_NO_SAMPLING


#endif
