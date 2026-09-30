#ifndef GLIMMER_PROBE_TYPES_HLSLI
#define GLIMMER_PROBE_TYPES_HLSLI

#include "../GlimmerCommon.hlsli"

// Terrain following probe clipmap, Glimmer's near field: GLIMMER_PROBE_CASCADES cascades of GRID x GRID columns with LAYERS probes each,
// stored toroidally in Texture3Ds of GRID x (CASCADES * LAYERS) x GRID. Past them the SH voxels (GlimmerSHCommon.hlsli) take over.
// Each probe holds L1 irradiance already divided by pi and convolved with the cosine lobe, so E(n) / pi = e0 + dot(e1, n): one float4
// per colour channel, in a single Texture3D of GRID x (CASCADES * LAYERS) x (3 * GRID) with a GRID deep slab per channel (GlimmerProbeSHTexel).

#define GLIMMER_PROBE_CASCADES 2
#define GLIMMER_PROBE_GRID 32
#define GLIMMER_PROBE_LAYERS 4
#define GLIMMER_PROBES_PER_CASCADE (GLIMMER_PROBE_GRID * GLIMMER_PROBE_GRID * GLIMMER_PROBE_LAYERS)

// Must match GlimmerProbeDispatchMode in GlimmerSWRTProbeVolume.cpp
#define GLIMMER_PROBE_DISPATCH_ALL 0u     // every probe
#define GLIMMER_PROBE_DISPATCH_SCROLLED 1u // every probe, but only those that scrolled in or are in the slice do anything
#define GLIMMER_PROBE_DISPATCH_SLICE 2u    // only the slice's probes, one after another

// A cascade updates a slice of its probes each frame rather than all of them every few frames, so light changes creep in
// rather than stepping. Slices are made of 2x2 column tiles (16 probes, traced together for coherence): every period-th tile
// of a scrambled order (a bijection on the 8 bit tile index: multiply, xorshift, multiply), so each slice is spread evenly
// over the cascade rather than bunched into a stripe
#define GLIMMER_PROBE_TILES_X (GLIMMER_PROBE_GRID / 2)
#define GLIMMER_PROBE_TILE_MASK (GLIMMER_PROBE_TILES_X * GLIMMER_PROBE_TILES_X - 1u)
#define GLIMMER_PROBES_PER_TILE (4u * GLIMMER_PROBE_LAYERS)

uint GlimmerProbeTileOrder(uint tile)
{
    uint x = (tile * 181u) & GLIMMER_PROBE_TILE_MASK;
    x ^= x >> 4;

    return (x * 109u) & GLIMMER_PROBE_TILE_MASK;
}

uint GlimmerProbeTileFromOrder(uint order)
{
    uint x = (order * 101u) & GLIMMER_PROBE_TILE_MASK; // 109^-1 mod 256
    x ^= x >> 4;

    return (x * 157u) & GLIMMER_PROBE_TILE_MASK; // 181^-1 mod 256
}

// slice: x = period, y = the slice updated this frame
bool GlimmerIsProbeInSlice(uint probeIndex, uint2 slice)
{
    const uint columnIndex = probeIndex % (GLIMMER_PROBE_GRID * GLIMMER_PROBE_GRID);
    const uint2 column = uint2(columnIndex % GLIMMER_PROBE_GRID, columnIndex / GLIMMER_PROBE_GRID);
    const uint tile = (column.y / 2u) * GLIMMER_PROBE_TILES_X + column.x / 2u;

    return GlimmerProbeTileOrder(tile) % slice.x == slice.y;
}

// the probe a dispatch's index-th thread (or group) handles: in a slice, a tile's probes one after another, layers first
uint GlimmerDispatchedProbe(uint index, uint mode, uint2 slice)
{
    if (mode != GLIMMER_PROBE_DISPATCH_SLICE)
    {
        return index;
    }

    const uint layer = index % GLIMMER_PROBE_LAYERS;
    const uint tileColumn = (index / GLIMMER_PROBE_LAYERS) % 4u;
    const uint tile = GlimmerProbeTileFromOrder((index / GLIMMER_PROBES_PER_TILE) * slice.x + slice.y);

    const uint2 column = uint2((tile % GLIMMER_PROBE_TILES_X) * 2u + (tileColumn & 1u), (tile / GLIMMER_PROBE_TILES_X) * 2u + (tileColumn >> 1));

    return layer * (GLIMMER_PROBE_GRID * GLIMMER_PROBE_GRID) + column.y * GLIMMER_PROBE_GRID + column.x;
}

// Must match GlimmerProbeCascadeShaderData in GlimmerSWRTProbeVolume.hpp
struct GlimmerProbeCascade
{
    int4 gridOrigin; // xy = absolute column of the grid's first column, z = 1 once the cascade has been traced
    float4 params;   // x = column spacing, y = layer scale, z = hysteresis while the light holds, w = hysteresis when it changes
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

// A probe's state: x = column (14 bits each, plenty to tell apart the columns that share a texel) | updates since it was placed
// (top 4 bits, saturating), y = height (as float bits)
#define GLIMMER_PROBE_COLUMN_MASK 0x0FFFFFFFu
#define GLIMMER_PROBE_MAX_UPDATES 15u

uint GlimmerPackColumn(int2 column)
{
    return (uint(column.x) & 0x3FFFu) | ((uint(column.y) & 0x3FFFu) << 14);
}

bool GlimmerIsSameColumn(uint stateX, int2 column)
{
    return (stateX & GLIMMER_PROBE_COLUMN_MASK) == GlimmerPackColumn(column);
}

uint GlimmerProbeUpdates(uint stateX)
{
    return stateX >> 28;
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

// where a probe's texel (GlimmerProbeTexel) is in the SH texture's slab for channel 0/1/2 = r/g/b
uint3 GlimmerProbeSHTexel(uint3 texel, uint channel)
{
    return uint3(texel.xy, channel * GLIMMER_PROBE_GRID + texel.z);
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
