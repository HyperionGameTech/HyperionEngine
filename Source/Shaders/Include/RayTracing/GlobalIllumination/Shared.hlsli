#ifndef PROBE_COMMON_GLSL
#define PROBE_COMMON_GLSL

#include "../../Octahedron.hlsli"

#define M_PI 3.14159265359

#define PROBE_GRID_ORIGIN (ddgiConstants.aabb_min.xyz)
#define PROBE_GRID_STEP   (float3(ddgiConstants.probe_distance, ddgiConstants.probe_distance, ddgiConstants.probe_distance))
#define PROBE_TOTAL_COUNT (ddgiConstants.probe_counts.x * ddgiConstants.probe_counts.y * ddgiConstants.probe_counts.z)
#define PROBE_NORMAL_BIAS (0.01)

int3 ProbeIndexToGridPosition(uint index)
{
    const int probe_count_xy = int(ddgiConstants.probe_counts.x) * int(ddgiConstants.probe_counts.y);
    
    return int3(
        index % int(ddgiConstants.probe_counts.x),
        (index % probe_count_xy) / int(ddgiConstants.probe_counts.x),
        index / probe_count_xy
    );
}

float3 GridPositionToWorldPosition(int3 pos)
{
    //float3 half_border = float3(ddgiConstants.probe_border) * 0.5;
    return PROBE_GRID_STEP * float3(pos) + ddgiConstants.aabb_min.xyz;
}

int GridPositionToProbeIndex(int3 pos)
{
    return pos.x
        + int(ddgiConstants.probe_counts.x) * pos.y
        + int(ddgiConstants.probe_counts.x) * int(ddgiConstants.probe_counts.y) * pos.z;
}


float3 ProbeIndexToWorldPosition(uint index)
{
    return GridPositionToWorldPosition(ProbeIndexToGridPosition(index));
}

int3 BaseGridCoord(float3 P)
{   
    return PROBE_TOTAL_COUNT == 0
        ? (int3) 0
        : clamp(int3(max((float3) 0, P - PROBE_GRID_ORIGIN) / PROBE_GRID_STEP), (int3) 0, int3(ddgiConstants.probe_counts.xyz) - 1);
}

float2 TextureCoordFromDirection(float3 dir, int probe_index, uint3 probe_counts, uint2 image_dimensions, uint probe_side_length)
{
    float2 normalizedOctCoord = EncodeOctahedralCoord(normalize(dir));
    float2 normalizedOctCoordZeroOne = (normalizedOctCoord + float2(1.0f, 1.0f)) * 0.5f;

    // Length of a probe side, plus one pixel on each edge for the border
    float probeWithBorderSide = float(probe_side_length) + 2.0f;

    float2 octCoordNormalizedToTextureDimensions = (normalizedOctCoordZeroOne * float(probe_side_length)) / float2(image_dimensions);

    uint probesPerRow = (image_dimensions.x - 2) / uint(probeWithBorderSide);

    // Add (2,2) back to texCoord within larger texture. Compensates for 1 pix 
    // border around texture and further 1 pix border around top left probe.
    float2 probeTopLeftPosition = float2(mod(probe_index, probesPerRow) * probeWithBorderSide,
        (probe_index / probesPerRow) * probeWithBorderSide) + float2(2.0f, 2.0f);

    float2 normalizedProbeTopLeftPosition = float2(probeTopLeftPosition) / float2(image_dimensions);

    return float2(normalizedProbeTopLeftPosition + octCoordNormalizedToTextureDimensions);
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
