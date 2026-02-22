#define DDGI

#include "../../include/defines.inc"
#include "../../include/shared.inc"
#include "../../include/noise.inc"
#include "../../include/packing.inc"

DECLARE_SAMPLER(DDGI, SamplerNearest) SamplerState sampler_nearest;
DECLARE_SAMPLER(DDGI, SamplerLinear) SamplerState sampler_linear;

#define texture_sampler sampler_linear
#define HYP_SAMPLER_NEAREST sampler_nearest
#define HYP_SAMPLER_LINEAR sampler_linear

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "../../include/scene.inc"
#include "../../include/noise.inc"
#include "../../include/Octahedron.inc"
#include "../../include/env_probe.inc"

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "../../include/rt/payload.inc"
#include "../../include/rt/probe/probe_uniforms.inc"

DECLARE_SRV(DDGI, TLAS) RaytracingAccelerationStructure tlas;

DECLARE_BUFFER(DDGI, DDGIConstants) cbuffer DDGIUniformBuffer
{
    DDGIConstants ddgiConstants;
};

DECLARE_UAV(DDGI, ProbeRayData) RWStructuredBuffer<ProbeRayData> probe_rays;

DECLARE_BUFFER(DDGI, Lights) cbuffer Lights
{
    Light lights[MAX_LIGHTS];
};

#include "../../include/rt/probe/shared.inc"

DECLARE_BUFFER(DDGI, WorldsBuffer) cbuffer WorldsBuffer
{
    WorldShaderData world_shader_data;
};

#if HAS_ENV_PROBE
DECLARE_SRV_DYNAMIC(DDGI, CurrentEnvProbe) StructuredBuffer<EnvProbe> current_env_probe_buffer;
#define current_env_probe current_env_probe_buffer[0]

#if ENV_PROBE_CUBEMAP
DECLARE_SRV(DDGI, EnvProbesTexture) TextureCubeArray envProbesTexture;
#else
DECLARE_SRV(DDGI, EnvProbesTexture) Texture2DArray envProbesTexture;
#endif

#endif

DECLARE_SRV(DDGI, ShadowMapsTextureArray) Texture2DArray shadow_maps;
DECLARE_SRV(DDGI, PointLightShadowMapsTextureArray) TextureCubeArray point_shadow_maps;

#include "../../include/shadows.inc"

#define RAY_OFFSET 0.025
#define NUM_BOUNCES 3

void SetProbeRayData(uint2 coord, ProbeRayData ray_data)
{
    probe_rays[PROBE_RAY_DATA_INDEX(coord)] = ray_data;
}

struct RayBounceInfo
{
    float4 throughput;
    float4 emissive;
};

[shader("raygeneration")]
void RayGenMain()
{
    const uint2 coord = DispatchRaysIndex().xy;
    
    const uint probe_index = coord.x;
    const uint ray_index = coord.y;
    
    float3 direction = normalize(mul(float3x3(
        ddgiConstants.rotationMatrix[0].xyz,
        ddgiConstants.rotationMatrix[1].xyz,
        ddgiConstants.rotationMatrix[2].xyz
    ), SphericalFibonacci(ray_index, ddgiConstants.num_rays_per_probe)));
    
    float3 origin = ProbeIndexToWorldPosition(probe_index) + direction * RAY_OFFSET;
    
    RAY_FLAG flags = RAY_FLAG_FORCE_OPAQUE;
    float tmin = RAY_OFFSET;
    float tmax = ddgiConstants.probe_distance;
    
    RayPayload payload = (RayPayload)0;
    payload.throughput = float4(1.0, 1.0, 1.0, 1.0);
    payload.color = float4(0.0, 0.0, 0.0, 0.0);
    payload.distance = -1.0;
    payload.normal = float3(0.0, 0.0, 0.0);
    payload.roughness = 0.0;
    payload.emissive = float4(0.0, 0.0, 0.0, 0.0);
    
    uint ray_seed = InitRandomSeed(InitRandomSeed(coord.x, coord.y), world_shader_data.frame_counter % 16);

    ProbeRayData ray_data[NUM_BOUNCES];
    ray_data[0].color = float4(0.0, 0.0, 0.0, 0.0);
    
    RayBounceInfo bounces[NUM_BOUNCES];
    uint num_bounces = 0;

    for (int bounce_index = 0; bounce_index < NUM_BOUNCES; bounce_index++)
    {
        payload.distance = -1.0;

        RayDesc ray;
        ray.Origin = origin;
        ray.Direction = direction;
        ray.TMin = tmin;
        ray.TMax = tmax;

        TraceRay(tlas, flags, 0xFF, 0, 1, 0, ray, payload);

        bounces[bounce_index].throughput = payload.throughput;
        bounces[bounce_index].emissive = payload.emissive;

        ray_data[bounce_index].color = float4(0.0, 0.0, 0.0, 0.0);
        ray_data[bounce_index].origin = float4(origin, 1.0);
        ray_data[bounce_index].normal = float4(payload.normal, 0.0);
        ray_data[bounce_index].direction_depth = float4(direction, payload.distance);

        if (payload.distance < 0.0)
        {
#if HAS_ENV_PROBE
            if (current_env_probe.texture_index != ~0u)
            {
                uint probe_texture_index = max(0, min(current_env_probe.texture_index, HYP_MAX_BOUND_REFLECTION_PROBES - 1));

                bounces[bounce_index].emissive += EnvProbeSample(sampler_linear, envProbesTexture, probe_texture_index, direction, 0.0);
            }
#endif
            
            for (uint light_index = 0; light_index < ddgiConstants.num_bound_lights; light_index++)
            {
                const Light light = lights[light_index];

                if (light.type != HYP_LIGHT_TYPE_DIRECTIONAL)
                {
                    continue;
                }

                float shadow = 1.0;

                if (light.type == HYP_LIGHT_TYPE_DIRECTIONAL && bool(light.flags & LF_SHADOW))
                {
                    shadow = GetShadowStandard(light, origin);
                }

                const float cos_theta = max(dot(direction, normalize(light.position_intensity.xyz)), 0.0);
                bounces[bounce_index].emissive += light.color * light.position_intensity.w * shadow * cos_theta;
            }

            ++num_bounces;

            break;
        }
        
        float3 hit_position = origin + direction * payload.distance;

        direction = normalize(RandomInHemisphere(
            float3(RandomFloat(ray_seed), RandomFloat(ray_seed), RandomFloat(ray_seed)),
            payload.normal
        ));
        origin = hit_position + direction * RAY_OFFSET;

        ++num_bounces;
    }

    for (int bounce_index = int(num_bounces - 1); bounce_index >= 0; bounce_index--)
    {
        float4 radiance = bounces[bounce_index].emissive;

        if (bounce_index != num_bounces - 1)
        {
            radiance += ray_data[bounce_index + 1].color * bounces[bounce_index].throughput;
        }

        float p = max(radiance.r, max(radiance.g, radiance.b));

        if (RandomFloat(ray_seed) > p)
        {
            break;
        }

        radiance /= max(p, 0.0001);
        
        ray_data[bounce_index].color = radiance;
    }

    SetProbeRayData(coord, ray_data[0]);
}
