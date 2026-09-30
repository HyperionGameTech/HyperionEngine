#ifndef GLIMMER_PROBE_TYPES_HLSLI
#define GLIMMER_PROBE_TYPES_HLSLI

#include "../GlimmerCommon.hlsli"

// Glimmer's near field: sparse blocks of probes around the static solids near the viewer. Probes sit on a world aligned grid, one per
// level (2 m, then 4 m), grouped into blocks of 4x4x4. Each level keeps a window of 8x8x8 blocks around the viewer, and a block of it
// only gets probes (a slot of the pool) where there are solids nearby; everywhere else lighting falls through to the SH voxels.
// Each probe holds L1 irradiance already divided by pi and convolved with the cosine lobe, so E(n) / pi = e0 + dot(e1, n), one float4
// per colour channel, plus depth moments along each axis to keep light from leaking through walls.

#define GLIMMER_PROBE_LEVELS 2
#define GLIMMER_PROBE_BLOCK 4
#define GLIMMER_PROBES_PER_BLOCK 64
#define GLIMMER_PROBE_WINDOW 8
#define GLIMMER_PROBE_WINDOW_BLOCKS 512
#define GLIMMER_PROBE_POOL_BLOCKS 128
#define GLIMMER_PROBE_POOL_PROBES (GLIMMER_PROBE_POOL_BLOCKS * GLIMMER_PROBES_PER_BLOCK)

#define GLIMMER_PROBE_NO_SLOT 0xFFFFFFFFu

// Must match GlimmerProbeState in GlimmerSWRTProbeVolume.hpp
#define GLIMMER_PROBE_STATE_FREE 0u
#define GLIMMER_PROBE_STATE_ACTIVE 1u
#define GLIMMER_PROBE_STATE_BURIED 2u  // under the ground; never traced
#define GLIMMER_PROBE_STATE_INSIDE 3u  // inside a solid even after moving; traced now and then to see if it got out

// distances the depth moments hold, in probe spacings; the lookup never asks past the far corner of a cell
#define GLIMMER_PROBE_DEPTH_RANGE 2.0

// how far a probe may be moved off its grid point, per axis, in spacings
#define GLIMMER_PROBE_MAX_OFFSET 0.45

// Must match GlimmerProbeLevelShaderData in GlimmerSWRTProbeVolume.hpp
struct GlimmerProbeLevel
{
    int4 windowOrigin; // xyz = absolute block of the window's first block, w = 1 once it has been allocated
    float4 params;     // x = probe spacing, y = 1 / spacing
};

// Must match GlimmerProbeVolumeShaderData in GlimmerSWRTProbeVolume.hpp
struct GlimmerProbeVolume
{
    GlimmerProbeLevel levels[GLIMMER_PROBE_LEVELS];
    uint4 info;       // x = number of levels, y = rays per probe, z = frame, w = 1 when the volume can be sampled
    float4 params;    // x = seconds since the volume started, y = unused, z = escape radiance clamp, w = max ray distance
    float4 nearField; // x = levels traced with SWRT, y = SWRT reach in spacings, z = SWRT instances, w = ground albedo
};

// A probe's state: x = state (bits 0-3) | relocation attempts (4-5) | updates since it was placed (8-13) | back face rays of its last
// update (16-23) | rays of its last update that started inside the ground or a solid span (24-31), y = offset from its grid point (3 x 10 bit snorm of GLIMMER_PROBE_MAX_OFFSET spacings), z = time of its last update
// (float bits), w = how many updates in a row the estimate sat on the same side of the history (signed)
#define GLIMMER_PROBE_MAX_UPDATES 63u

uint GlimmerProbeStateOf(uint4 state)
{
    return state.x & 0xFu;
}

uint GlimmerProbeRelocations(uint4 state)
{
    return (state.x >> 4) & 0x3u;
}

uint GlimmerProbeUpdates(uint4 state)
{
    return (state.x >> 8) & 0x3Fu;
}

uint GlimmerProbeBackfaces(uint4 state)
{
    return (state.x >> 16) & 0xFFu;
}

uint GlimmerProbeStartsInside(uint4 state)
{
    return (state.x >> 24) & 0xFFu;
}

uint GlimmerPackProbeFlags(uint probeState, uint relocations, uint updates, uint backfaces, uint startsInside = 0u)
{
    return (probeState & 0xFu) | ((relocations & 0x3u) << 4) | ((min(updates, GLIMMER_PROBE_MAX_UPDATES) & 0x3Fu) << 8) | ((min(backfaces, 255u) & 0xFFu) << 16)
        | ((min(startsInside, 255u) & 0xFFu) << 24);
}

// offset in spacings
uint GlimmerPackProbeOffset(float3 offset)
{
    const int3 quantized = int3(round(clamp(offset / GLIMMER_PROBE_MAX_OFFSET, -1.0, 1.0) * 511.0));

    return (uint(quantized.x) & 0x3FFu) | ((uint(quantized.y) & 0x3FFu) << 10) | ((uint(quantized.z) & 0x3FFu) << 20);
}

float3 GlimmerUnpackProbeOffset(uint packed)
{
    // sign extend each 10 bit field
    const int3 quantized = int3(int(packed << 22) >> 22, int(packed << 12) >> 22, int(packed << 2) >> 22);

    return float3(quantized) / 511.0 * GLIMMER_PROBE_MAX_OFFSET;
}

// the block table holds a slot (or GLIMMER_PROBE_NO_SLOT) per block of each level's window, addressed by the block's absolute coordinate
uint GlimmerProbeBlockTableIndex(uint levelIndex, int3 block)
{
    const uint3 wrapped = uint3(block & (GLIMMER_PROBE_WINDOW - 1));

    return levelIndex * GLIMMER_PROBE_WINDOW_BLOCKS + (wrapped.z * GLIMMER_PROBE_WINDOW + wrapped.y) * GLIMMER_PROBE_WINDOW + wrapped.x;
}

bool GlimmerIsBlockInWindow(GlimmerProbeLevel level, int3 block)
{
    const int3 local = block - level.windowOrigin.xyz;

    return level.windowOrigin.w != 0 && all(local >= 0) && all(local < GLIMMER_PROBE_WINDOW);
}

// the index of a probe of a slot, x fastest
uint GlimmerProbeIndex(uint slot, int3 localProbe)
{
    return slot * GLIMMER_PROBES_PER_BLOCK + uint((localProbe.z * GLIMMER_PROBE_BLOCK + localProbe.y) * GLIMMER_PROBE_BLOCK + localProbe.x);
}

int3 GlimmerLocalProbeOf(uint probeIndex)
{
    const uint local = probeIndex % GLIMMER_PROBES_PER_BLOCK;

    return int3(local % GLIMMER_PROBE_BLOCK, (local / GLIMMER_PROBE_BLOCK) % GLIMMER_PROBE_BLOCK, local / (GLIMMER_PROBE_BLOCK * GLIMMER_PROBE_BLOCK));
}

// absolute probe coordinate on its level's grid
int3 GlimmerProbeCoord(int3 block, int3 localProbe)
{
    return block * GLIMMER_PROBE_BLOCK + localProbe;
}

float3 GlimmerProbeGridPosition(GlimmerProbeLevel level, int3 probeCoord)
{
    return (float3(probeCoord) + 0.5) * level.params.x;
}

uint GlimmerProbeHash(uint value)
{
    value ^= value >> 16;
    value *= 0x7FEB352Du;
    value ^= value >> 15;
    value *= 0x846CA68Bu;
    value ^= value >> 16;

    return value;
}

// a uniformly random rotation per probe and frame, so neighbouring probes never make the same error in the same frame
float4 GlimmerProbeRayRotation(uint probeIndex, uint frame)
{
    const uint seed = GlimmerProbeHash(probeIndex * 0x9E3779B9u + frame * 0x85EBCA6Bu);

    const float u1 = float(GlimmerProbeHash(seed) & 0xFFFFFFu) / 16777216.0;
    const float u2 = float(GlimmerProbeHash(seed + 1u) & 0xFFFFFFu) / 16777216.0;
    const float u3 = float(GlimmerProbeHash(seed + 2u) & 0xFFFFFFu) / 16777216.0;

    const float a = sqrt(1.0 - u1);
    const float b = sqrt(u1);
    const float twoPi = 6.28318530718;

    return float4(a * sin(twoPi * u2), a * cos(twoPi * u2), b * sin(twoPi * u3), b * cos(twoPi * u3));
}

float3 GlimmerProbeRayDirection(GlimmerProbeVolume volume, uint probeIndex, uint rayIndex)
{
    return normalize(GlimmerRotateByQuaternion(GlimmerProbeRayRotation(probeIndex, volume.info.z), GlimmerSphericalFibonacci(rayIndex, volume.info.y)));
}

float3 GlimmerEvaluateL1(float4 shR, float4 shG, float4 shB, float3 N)
{
    const float4 basis = float4(1.0, N);

    return max(float3(dot(shR, basis), dot(shG, basis), dot(shB, basis)), 0.0);
}

// which of the six axis directions (+x, -x, +y, ...) a direction is closest to
uint GlimmerProbeDirectionBin(float3 direction)
{
    const float3 absDirection = abs(direction);
    const uint axis = (absDirection.x >= absDirection.y && absDirection.x >= absDirection.z) ? 0u : (absDirection.y >= absDirection.z ? 1u : 2u);

    return axis * 2u + (direction[axis] < 0.0 ? 1u : 0u);
}

#endif
