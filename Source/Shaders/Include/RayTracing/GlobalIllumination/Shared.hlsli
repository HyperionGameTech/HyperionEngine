#ifndef PROBE_COMMON_GLSL
#define PROBE_COMMON_GLSL

#include "../../Octahedron.hlsli"

#define M_PI 3.14159265359

// fraction of a cascade's half extent over which it cross fades into the next, coarser cascade
#define DDGI_CASCADE_BLEND_BAND 0.2

uint DDGIProbesPerCascade()
{
    return ddgiConstants.probeCounts.x * ddgiConstants.probeCounts.y * ddgiConstants.probeCounts.z;
}

int3 DDGIProbeCounts()
{
    return int3(ddgiConstants.probeCounts.xyz);
}

float3 DDGIProbeSpacing(uint cascadeIndex)
{
    return ddgiConstants.cascades[cascadeIndex].probeSpacing.xyz;
}

bool DDGIIsCascadeUpdating(uint cascadeIndex)
{
    return (ddgiConstants.cascadeUpdateMask & (1u << cascadeIndex)) != 0u;
}

int3 DDGIWrapCoord(int3 coord)
{
    const int3 counts = DDGIProbeCounts();

    return ((coord % counts) + counts) % counts;
}

int3 DDGIStorageIndexToStorageCoord(uint storageIndex)
{
    const int countXY = int(ddgiConstants.probeCounts.x * ddgiConstants.probeCounts.y);

    return int3(
        int(storageIndex) % int(ddgiConstants.probeCounts.x),
        (int(storageIndex) % countXY) / int(ddgiConstants.probeCounts.x),
        int(storageIndex) / countXY);
}

/* Probe index inside the atlas; cascades are stacked in rows of probeCounts.x * probeCounts.y probes. */
uint DDGIProbeIndex(uint cascadeIndex, int3 storageCoord)
{
    const int3 counts = DDGIProbeCounts();

    return cascadeIndex * DDGIProbesPerCascade()
        + uint(storageCoord.x + counts.x * storageCoord.y + counts.x * counts.y * storageCoord.z);
}

/* Storage is addressed toroidally, so a slot holds the one lattice coord inside the cascade's current
   window that is congruent to the slot index. */
int3 DDGIStorageCoordToGridCoord(uint cascadeIndex, int3 storageCoord)
{
    const int3 counts = DDGIProbeCounts();
    const int3 gridOffset = ddgiConstants.cascades[cascadeIndex].gridOffset.xyz;

    return gridOffset + ((((storageCoord - gridOffset) % counts) + counts) % counts);
}

float3 DDGIProbeWorldPosition(uint cascadeIndex, int3 gridCoord)
{
    return float3(gridCoord) * DDGIProbeSpacing(cascadeIndex);
}

/* True for probes that scrolled into the volume since the cascade was last updated, and so hold radiance
   gathered at a different world position. */
bool DDGIIsProbeStale(uint cascadeIndex, int3 gridCoord)
{
    if ((ddgiConstants.cascadeResetMask & (1u << cascadeIndex)) != 0u)
    {
        return true;
    }

    const int3 previousOffset = ddgiConstants.cascades[cascadeIndex].gridOffsetPrev.xyz;

    return any(gridCoord < previousOffset) || any(gridCoord >= previousOffset + DDGIProbeCounts());
}

/* 1 well inside the cascade, falling to 0 at its outer face. */
float DDGICascadeWeight(uint cascadeIndex, float3 P)
{
    const float3 spacing = DDGIProbeSpacing(cascadeIndex);
    const float3 volumeMin = float3(ddgiConstants.cascades[cascadeIndex].gridOffset.xyz) * spacing;
    const float3 volumeMax = volumeMin + float3(DDGIProbeCounts() - 1) * spacing;

    const float3 center = (volumeMin + volumeMax) * 0.5;
    const float3 halfExtent = max((volumeMax - volumeMin) * 0.5, float3(0.0001, 0.0001, 0.0001));

    const float3 normalized = abs(P - center) / halfExtent;
    const float edgeDistance = max(normalized.x, max(normalized.y, normalized.z));

    return saturate((1.0 - edgeDistance) / DDGI_CASCADE_BLEND_BAND);
}

float2 TextureCoordFromDirection(float3 dir, uint probe_index, uint2 image_dimensions, uint probe_side_length)
{
    float2 normalizedOctCoord = EncodeOctahedralCoord(normalize(dir));
    float2 normalizedOctCoordZeroOne = (normalizedOctCoord + float2(1.0f, 1.0f)) * 0.5f;

    // Length of a probe side, plus one pixel on each edge for the border
    uint probeWithBorderSide = probe_side_length + 2;

    // The border texels are never written, so keep the (nearest filtered) coord on a texel centre inside the probe itself.
    float2 octCoordInProbe = clamp(normalizedOctCoordZeroOne * float(probe_side_length), 0.5f, float(probe_side_length) - 0.5f);

    uint probesPerRow = (image_dimensions.x - 2) / probeWithBorderSide;

    // Add (2,2) back to texCoord within larger texture. Compensates for 1 pix
    // border around texture and further 1 pix border around top left probe.
    float2 probeTopLeftPosition = float2(
        float((probe_index % probesPerRow) * probeWithBorderSide),
        float((probe_index / probesPerRow) * probeWithBorderSide)) + float2(2.0f, 2.0f);

    return (probeTopLeftPosition + octCoordInProbe) / float2(image_dimensions);
}

float3 SphericalFibonacci(uint index, uint n)
{
    float i = float(index);

    const float PHI = sqrt(5.0) * 0.5 + 0.5;
#define madfrac(A, B) ((A) * (B)-floor((A) * (B)))
    float phi       = 2.0 * M_PI * madfrac(i, PHI - 1);
    float cos_theta = 1.0 - (2.0 * i + 1.0) * (1.0 / float(n));
    float sin_theta = sqrt(clamp(1.0 - cos_theta * cos_theta, 0.0f, 1.0f));

    return float3(cos(phi) * sin_theta, sin(phi) * sin_theta, cos_theta);
#undef madfrac
}


#endif
