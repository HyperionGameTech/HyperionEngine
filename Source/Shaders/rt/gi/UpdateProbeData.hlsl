#include "../include/defines.inc"
#include "../include/rt/probe/probe_uniforms.inc"

PERMUTE(MODE, IRRADIANCE, DEPTH);

STATIC(HYSTERESIS, 0.95);

#define CACHE_SIZE 64
#define EPS 0.00001
#define ENERGY_CONSERVATION 0.95
#define MAX_DISTANCE (ddgiConstants.probe_distance * 1.2)

#if MODE_DEPTH
    #define DDGI_PROBE_SIDE_LENGTH DDGI_PROBE_SIDE_LENGTH_DEPTH
    #define OUTPUT_IMAGE_DIMENSIONS (ddgiConstants.image_dimensions.zw)
#else
    #define DDGI_PROBE_SIDE_LENGTH DDGI_PROBE_SIDE_LENGTH_IRRADIANCE
    #define OUTPUT_IMAGE_DIMENSIONS (ddgiConstants.image_dimensions.xy)
#endif

#define GROUP_SIZE DDGI_PROBE_SIDE_LENGTH

#define DDGI_PROBE_SIDE_LENGTH_BORDER (DDGI_PROBE_SIDE_LENGTH + ddgiConstants.probe_border.x)

groupshared ProbeRayData ray_cache[CACHE_SIZE];

DECLARE_BUFFER_DYNAMIC(DDGI, CBuffer) cbuffer CBuffer
{
    DDGIConstants ddgiConstants;
};

DECLARE_SRV(DDGI, ProbeRayData) StructuredBuffer<ProbeRayData> probe_rays;

#include "../include/rt/probe/shared.inc"

#if MODE_DEPTH
DECLARE_UAV(DDGI, OutputImage) RWTexture2D<float2> outputImage;
#else // !MODE_DEPTH
DECLARE_UAV(DDGI, OutputImage) RWTexture2D<float4> outputImage;
#endif // MODE_DEPTH

float2 NormalizeOctahedralCoord(uint2 coord)
{
    int2 oct_frag_coord = int2((int(coord.x) - 2) % DDGI_PROBE_SIDE_LENGTH_BORDER, (int(coord.y) - 2) % DDGI_PROBE_SIDE_LENGTH_BORDER);
    
    return (float2(oct_frag_coord) + float2(0.5, 0.5)) * (2.0 / float(DDGI_PROBE_SIDE_LENGTH)) - float2(1.0, 1.0);
}

ProbeRayData GetProbeRayData(uint2 coord)
{
    return probe_rays[PROBE_RAY_DATA_INDEX(coord)];
}

void UpdateRayCache(int2 coord, uint offset, uint num_rays, uint groupIndex)
{
    if (groupIndex >= num_rays)
    {
        return;
    }

    int probes_per_side = int((OUTPUT_IMAGE_DIMENSIONS.x - 2) / DDGI_PROBE_SIDE_LENGTH_BORDER);
    int probe_index = int(coord.x / DDGI_PROBE_SIDE_LENGTH_BORDER) + probes_per_side * int(coord.y / DDGI_PROBE_SIDE_LENGTH_BORDER);

    ray_cache[groupIndex] = GetProbeRayData(uint2(probe_index, offset + groupIndex));
}

void GatherRays(int2 coord, uint num_rays, inout float3 result, inout float total_weight)
{
    ProbeRayData ray;

    for (uint i = 0; i < num_rays; i++)
    {
        ray = ray_cache[i];
        float3 ray_direction = ray.direction_depth.xyz;
        float3 ray_origin = ray.origin.xyz;
        float ray_depth = ray.direction_depth.w;

#if MODE_DEPTH
        float dist = ray_depth;
#else
        float4 radiance = ray.color;
        radiance.rgb *= ENERGY_CONSERVATION;
#endif

        float3 texel_direction = DecodeOctahedralCoord(NormalizeOctahedralCoord(uint2(coord)));

        float weight = max(0.0, dot(texel_direction, ray_direction));

#if MODE_DEPTH
        weight = pow(weight, 0.9 /* depth sharpness */);
#endif

        if (weight >= EPS)
        {
#if MODE_DEPTH
            result += float3(dist * weight, HYP_FMATH_SQR(dist) * weight, 0.0);
#else
            result += float3(radiance.rgb * weight);
#endif

            total_weight += weight;
        }
    }
}

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void CSMain(
    uint3 dispatchThreadID  : SV_DispatchThreadID,
    uint3 groupID           : SV_GroupID,
    uint  groupIndex        : SV_GroupIndex)
{
    int2 coord = int2(dispatchThreadID.xy) + (int2(groupID.xy) * int2(2, 2)) + int2(2, 2);

    float3 result = float3(0.0, 0.0, 0.0);
    float total_weight = 0.0;

    uint remaining_rays = ddgiConstants.num_rays_per_probe;
    uint offset = 0;

    while (remaining_rays != 0)
    {
        uint num_rays = min(CACHE_SIZE, remaining_rays);

        UpdateRayCache(coord, offset, num_rays, groupIndex);

        GroupMemoryBarrierWithGroupSync();

        GatherRays(coord, num_rays, result, total_weight);

        GroupMemoryBarrierWithGroupSync();

        remaining_rays -= num_rays;
        offset += num_rays;
    }

    if (total_weight > EPS)
    {
        result /= total_weight;
    }

    const float alpha = clamp(1.0 - HYSTERESIS, 0.0, 1.0);

#if MODE_DEPTH
    float2 existing = outputImage[coord].xy;
    float2 blended = ddgiConstants.counter == 0 ? result.xy : lerp(existing, result.xy, alpha);

    outputImage[coord] = blended;
#else
    float3 existing = outputImage[coord].rgb;
    float3 blended = ddgiConstants.counter == 0 ? result : lerp(existing, result, alpha);

    outputImage[coord] = float4(blended, 1.0);
#endif
}
