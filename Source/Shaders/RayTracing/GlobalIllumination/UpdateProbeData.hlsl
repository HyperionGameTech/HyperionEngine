#include "../include/Defines.hlsli"
#include "../include/RayTracing/GlobalIllumination/ProbeUniforms.hlsli"

PERMUTE(MODE, IRRADIANCE, DEPTH);

#define CACHE_SIZE 64
#define EPS 0.00001
#define ENERGY_CONSERVATION 0.95

#if MODE_DEPTH
    #define DDGI_PROBE_SIDE_LENGTH DDGI_PROBE_SIDE_LENGTH_DEPTH
    #define OUTPUT_IMAGE_DIMENSIONS (ddgiConstants.imageDimensions.zw)
#else
    #define DDGI_PROBE_SIDE_LENGTH DDGI_PROBE_SIDE_LENGTH_IRRADIANCE
    #define OUTPUT_IMAGE_DIMENSIONS (ddgiConstants.imageDimensions.xy)
#endif

#define GROUP_SIZE DDGI_PROBE_SIDE_LENGTH

#define DDGI_PROBE_SIDE_LENGTH_BORDER int(DDGI_PROBE_SIDE_LENGTH + ddgiConstants.probeBorder.x)

groupshared ProbeRayData ray_cache[CACHE_SIZE];

DECLARE_BUFFER_DYNAMIC(DDGI, CBuffer) cbuffer CBuffer
{
    DDGIConstants ddgiConstants;
};

DECLARE_SRV(DDGI, ProbeRayData) StructuredBuffer<ProbeRayData> probe_rays;

#include "../include/RayTracing/GlobalIllumination/Shared.hlsli"

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

void UpdateRayCache(uint probeIndex, uint offset, uint num_rays, uint groupIndex)
{
    if (groupIndex >= num_rays)
    {
        return;
    }

    ray_cache[groupIndex] = GetProbeRayData(uint2(probeIndex, offset + groupIndex));
}

void GatherRays(int2 coord, uint num_rays, float maxDistance, inout float3 result, inout float total_weight)
{
    ProbeRayData ray;

    for (uint i = 0; i < num_rays; i++)
    {
        ray = ray_cache[i];
        float3 ray_direction = ray.direction_depth.xyz;
        float ray_depth = ray.direction_depth.w;

#if MODE_DEPTH
        float dist = min(ray_depth, maxDistance);
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

    const int probesPerRow = int(OUTPUT_IMAGE_DIMENSIONS.x - 2) / DDGI_PROBE_SIDE_LENGTH_BORDER;
    const uint probeIndex = uint((coord.x / DDGI_PROBE_SIDE_LENGTH_BORDER) + probesPerRow * (coord.y / DDGI_PROBE_SIDE_LENGTH_BORDER));

    const uint probesPerCascade = DDGIProbesPerCascade();
    const uint cascadeIndex = probeIndex / probesPerCascade;

    // whole group maps to a single probe, so this is uniform across the group
    if (cascadeIndex >= ddgiConstants.numCascades || !DDGIIsCascadeUpdating(cascadeIndex))
    {
        return;
    }

    const int3 storageCoord = DDGIStorageIndexToStorageCoord(probeIndex % probesPerCascade);
    const int3 gridCoord = DDGIStorageCoordToGridCoord(cascadeIndex, storageCoord);

    const float maxDistance = DDGIProbeSpacing(cascadeIndex).x * 1.5;

    float3 result = float3(0.0, 0.0, 0.0);
    float total_weight = 0.0;

    uint remaining_rays = ddgiConstants.numRaysPerProbe;
    uint offset = 0;

    while (remaining_rays != 0)
    {
        uint num_rays = min(CACHE_SIZE, remaining_rays);

        UpdateRayCache(probeIndex, offset, num_rays, groupIndex);

        GroupMemoryBarrierWithGroupSync();

        GatherRays(coord, num_rays, maxDistance, result, total_weight);

        GroupMemoryBarrierWithGroupSync();

        remaining_rays -= num_rays;
        offset += num_rays;
    }

    if (total_weight > EPS)
    {
        result /= total_weight;
    }

    // probes that scrolled into the volume hold radiance gathered somewhere else, so they replace rather than blend
    const bool isStale = DDGIIsProbeStale(cascadeIndex, gridCoord);
    const float alpha = isStale ? 1.0 : saturate(ddgiConstants.cascades[cascadeIndex].blendAlpha);

#if MODE_DEPTH
    float2 existing = outputImage[coord].xy;

    outputImage[coord] = lerp(existing, result.xy, alpha);
#else
    float3 existing = outputImage[coord].rgb;

    outputImage[coord] = float4(lerp(existing, result, alpha), 1.0);
#endif
}
