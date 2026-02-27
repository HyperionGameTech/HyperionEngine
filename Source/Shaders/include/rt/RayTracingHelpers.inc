#ifndef HYP_RT_RADIANCE
#define HYP_RT_RADIANCE

struct RayTracingConstants
{
    uint num_bound_lights;
    uint ray_offset; // for lightmapper
    float min_roughness;
    float _pad0;
    int2 output_image_resolution;
    float _pad1;
    float _pad2;
    uint4 light_indices[4];
};

#if RAY_GEN_SHADER

float CheckInShadow(float3 position, float3 normal, float3 lightDir, float maxDist)
{
    float3 origin = position + normal * 0.01;
    float3 direction = lightDir;

    RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_FORCE_OPAQUE> rq;
    
    RayDesc ray;
    ray.Origin = origin;
    ray.Direction = direction;
    ray.TMin = 0.0001;
    ray.TMax = maxDist > 0.0 ? maxDist : 1000.0;
    
    rq.TraceRayInline(tlas, RAY_FLAG_NONE, 0xff, ray);
    rq.Proceed();

    return float(rq.CommittedStatus() != COMMITTED_NOTHING);
}

float CheckInShadow(float3 position, float3 normal, float3 lightDir)
{
    return CheckInShadow(position, normal, lightDir, -1.0);
}

#endif

#endif