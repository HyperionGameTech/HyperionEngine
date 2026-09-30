#ifndef GLIMMER_PROBE_TYPES_HLSLI
#define GLIMMER_PROBE_TYPES_HLSLI

#include "../GlimmerCommon.hlsli"

// Terrain following probe clipmap: GLIMMER_PROBE_CASCADES cascades of GRID x GRID columns with LAYERS probes each,
// stored toroidally in Texture3Ds of GRID x (CASCADES * LAYERS) x GRID.
// Each probe holds L1 irradiance already divided by pi and convolved with the cosine lobe, so E(n) / pi = e0 + dot(e1, n).

#define GLIMMER_PROBE_CASCADES 6
#define GLIMMER_PROBE_GRID 32
#define GLIMMER_PROBE_LAYERS 4
#define GLIMMER_PROBES_PER_CASCADE (GLIMMER_PROBE_GRID * GLIMMER_PROBE_GRID * GLIMMER_PROBE_LAYERS)

// Must match GlimmerProbeCascadeShaderData in GlimmerSWRTProbeVolume.hpp
struct GlimmerProbeCascade
{
    int4 gridOrigin; // xy = absolute column of the grid's first column, z = 1 once the cascade has been traced
    float4 params;   // x = column spacing, y = layer scale, z = hysteresis
};

// Must match GlimmerProbeVolumeShaderData in GlimmerSWRTProbeVolume.hpp
struct GlimmerProbeVolume
{
    GlimmerProbeCascade cascades[GLIMMER_PROBE_CASCADES];
    uint4 info;          // x = number of cascades, y = rays per probe, z = frame, w = 1 when the volume can be sampled
    float4 rayRotation;  // quaternion applied to this frame's ray directions
    float4 params;       // x = base height where there's no ground, y = unused, z = escape radiance clamp, w = max ray distance
    float4 nearField;    // x = cascades traced with SWRT, y = SWRT reach in spacings, z = SWRT instances, w = ground albedo
};

static const float GlimmerProbeLayerHeights[GLIMMER_PROBE_LAYERS] = { 1.0, 3.0, 9.0, 27.0 };

uint GlimmerPackColumn(int2 column)
{
    return (uint(column.x) & 0xFFFFu) | ((uint(column.y) & 0xFFFFu) << 16);
}

uint2 GlimmerWrapProbeColumn(int2 column)
{
    return uint2(column & (GLIMMER_PROBE_GRID - 1));
}

uint3 GlimmerProbeTexel(uint cascadeIndex, int2 column, uint layer)
{
    const uint2 wrapped = GlimmerWrapProbeColumn(column);

    return uint3(wrapped.x, cascadeIndex * GLIMMER_PROBE_LAYERS + layer, wrapped.y);
}

float3 GlimmerProbeRayDirection(GlimmerProbeVolume volume, uint rayIndex)
{
    return normalize(GlimmerRotateByQuaternion(volume.rayRotation, GlimmerSphericalFibonacci(rayIndex, volume.info.y)));
}

// probes of a cascade are dispatched in this order: layer major, then z, then x
void GlimmerProbeFromIndex(uint probeIndex, out int2 outLocalColumn, out uint outLayer)
{
    outLayer = probeIndex / (GLIMMER_PROBE_GRID * GLIMMER_PROBE_GRID);

    const uint columnIndex = probeIndex % (GLIMMER_PROBE_GRID * GLIMMER_PROBE_GRID);
    outLocalColumn = int2(columnIndex % GLIMMER_PROBE_GRID, columnIndex / GLIMMER_PROBE_GRID);
}

float3 GlimmerEvaluateL1(float4 shR, float4 shG, float4 shB, float3 N)
{
    const float4 basis = float4(1.0, N);

    return max(float3(dot(shR, basis), dot(shG, basis), dot(shB, basis)), 0.0);
}

#endif
