#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_scalar_block_layout : require

#include "../include/rt/probe/probe_uniforms.inc"

#define CACHE_SIZE 64
#define EPS 0.00001
#define ENERGY_CONSERVATION 0.95
#define MAX_DISTANCE (ddgiConstants.probe_distance * 1.5)

#if DEPTH
    #define PROBE_SIDE_LENGTH PROBE_SIDE_LENGTH_DEPTH
    #define OUTPUT_IMAGE output_depth
    #define OUTPUT_IMAGE_DIMENSIONS (ddgiConstants.image_dimensions.zw)
#else
    #define PROBE_SIDE_LENGTH PROBE_SIDE_LENGTH_IRRADIANCE
    #define OUTPUT_IMAGE output_irradiance
    #define OUTPUT_IMAGE_DIMENSIONS (ddgiConstants.image_dimensions.xy)
#endif

#define GROUP_SIZE PROBE_SIDE_LENGTH

#define PROBE_SIDE_LENGTH_BORDER (PROBE_SIDE_LENGTH + ddgiConstants.probe_border.x)

shared ProbeRayData ray_cache[CACHE_SIZE];

layout(std140, set = 0, binding = 9) uniform DDGIConstantsBuffer
{
    DDGIConstants ddgiConstants;
};

layout(std140, set = 0, binding = 10) buffer ProbeRayDataBuffer {
    ProbeRayData probe_rays[];
};

#include "../include/rt/probe/shared.inc"

layout(local_size_x = GROUP_SIZE, local_size_y = GROUP_SIZE, local_size_z = 1) in;

layout(set = 0, binding = 11, rgba16f) uniform image2D output_irradiance;
layout(set = 0, binding = 12, rg16f)   uniform image2D output_depth;

vec2 NormalizeOctahedralCoord(uvec2 coord)
{
    ivec2 oct_frag_coord = ivec2((int(coord.x) - 2) % PROBE_SIDE_LENGTH_BORDER, (int(coord.y) - 2) % PROBE_SIDE_LENGTH_BORDER);
    
    return (vec2(oct_frag_coord) + vec2(0.5)) * (2.0 / float(PROBE_SIDE_LENGTH)) - vec2(1.0);
}

ProbeRayData GetProbeRayData(uvec2 coord)
{
    return probe_rays[PROBE_RAY_DATA_INDEX(coord)];
}

void UpdateRayCache(ivec2 coord, uint offset, uint num_rays)
{
    const uint index = uint(gl_LocalInvocationIndex);
    
    if (index >= num_rays) {
        return;
    }
    
    int probes_per_side = int((OUTPUT_IMAGE_DIMENSIONS.x - 2) / PROBE_SIDE_LENGTH_BORDER);
    int probe_index = int(coord.x / PROBE_SIDE_LENGTH_BORDER) + probes_per_side * int(coord.y / PROBE_SIDE_LENGTH_BORDER);
    
    ray_cache[index] = GetProbeRayData(uvec2(probe_index, offset + index));
}

void GatherRays(ivec2 coord, uint num_rays, inout vec3 result, inout float total_weight)
{
    ProbeRayData ray;
    
    for (uint i = 0; i < num_rays; i++) {
        ray = ray_cache[i];
        vec3 ray_direction = ray.direction_depth.xyz;
        vec3 ray_origin = ray.origin.xyz;
        float ray_depth = ray.direction_depth.w;
        
#if DEPTH
        float dist = min(MAX_DISTANCE, ray_depth - 0.01);
        
        if (dist <= -0.9999) {
            dist = MAX_DISTANCE;
        }
#else
        vec4 radiance = ray.color;
        // radiance.rgb *= ENERGY_CONSERVATION;
#endif


        vec3 texel_direction = DecodeOctahedralCoord(NormalizeOctahedralCoord(coord));
        
        float weight = max(0.0, dot(texel_direction, ray_direction));

#if DEPTH
        weight = pow(weight, 1.0 /* depth sharpness */);
#endif

        if (weight >= EPS) {
#if DEPTH
            result += vec3(dist * weight, HYP_FMATH_SQR(dist) * weight, 0.0);
#else
            result += vec3(radiance.rgb * weight);
#endif

            total_weight += weight;
        }
    }
}

void main()
{
    ivec2 coord = ivec2(gl_GlobalInvocationID.xy) + (ivec2(gl_WorkGroupID.xy) * ivec2(2)) + ivec2(2);
    
    vec3 result = vec3(0.0);
    float total_weight = 0.0;

    uint remaining_rays = ddgiConstants.num_rays_per_probe;
    uint offset = 0;

    while (remaining_rays != 0) {
        uint num_rays = min(CACHE_SIZE, remaining_rays);
        
        UpdateRayCache(coord, offset, num_rays);

        barrier();

        GatherRays(coord, num_rays, result, total_weight);

        barrier();

        remaining_rays -= num_rays;
        offset += num_rays;
    }
    
    if (total_weight > EPS) {
        result /= total_weight;
    }

    float hysteresis = 0.9;

    float alpha = 1.0 - hysteresis;
    
    imageStore(
        OUTPUT_IMAGE,
        coord,
        vec4(result.rgb, alpha)
    );
}
