#ifndef PROBE_UNIFORMS
#define PROBE_UNIFORMS

#include "../../defines.inc"

#define PROBE_SYSTEM_FLAGS_NONE 0x0
#define PROBE_SYSTEM_FLAGS_FIRST_RUN 0x1

struct DDGIConstants
{
    mat4 rotationMatrix;

    vec4 aabb_max;
    vec4 aabb_min;
    
    uvec4 probe_border;
    uvec4 probe_counts;
    uvec4 grid_dimensions;
    uvec4 image_dimensions;

    float probe_distance;
    uint num_rays_per_probe;
    uint numBoundLights;
    uint flags;
};

struct ProbeRayData
{
    vec4 direction_depth;
    vec4 origin;
    vec4 normal;
    vec4 color;
};

#define PROBE_RAY_DATA_INDEX(coord) (coord.x + ddgiConstants.grid_dimensions.x * coord.y)

#endif