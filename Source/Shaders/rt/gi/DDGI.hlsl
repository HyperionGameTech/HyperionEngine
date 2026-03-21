#define DDGI

#include "../../include/defines.inc"
#include "../../include/shared.inc"
#include "../../include/noise.inc"
#include "../../include/packing.inc"
#include "../../include/brdf.inc"

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
#include "../../include/rt/RayTracingHelpers.inc"

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

#include "../../include/Shadows.hlsli"

#define RAY_OFFSET 0.025
#define NUM_SAMPLES 1
#define NUM_BOUNCES 4

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
    
    const float3 direction = normalize(mul(float3x3(
        ddgiConstants.rotationMatrix[0].xyz,
        ddgiConstants.rotationMatrix[1].xyz,
        ddgiConstants.rotationMatrix[2].xyz
    ), SphericalFibonacci(ray_index, ddgiConstants.num_rays_per_probe)));
    
    const float3 origin = ProbeIndexToWorldPosition(probe_index) + direction * RAY_OFFSET;
    
    RAY_FLAG flags = RAY_FLAG_FORCE_OPAQUE;
    float tmin = RAY_OFFSET;
    float tmax = 1000.0; //ddgiConstants.probe_distance;
    
    uint ray_seed = InitRandomSeed(InitRandomSeed(coord.x, coord.y), world_shader_data.frame_counter % 256);

    ProbeRayData ray_data = (ProbeRayData)0;
    ray_data.origin = float4(origin, 0.0);
    
    float4 accumRadiance = (float4)0;

    for (uint sample_index = 0; sample_index < NUM_SAMPLES; sample_index++)
    {
        float3 localOrigin = origin;
        float3 localDirection = direction;
        
        uint ray_seed = InitRandomSeed(InitRandomSeed(
            (DispatchRaysIndex().x * 2 * NUM_SAMPLES) + sample_index * 2,
            (DispatchRaysIndex().x * 2 * NUM_SAMPLES) + (sample_index * 2) + 1),
            world_shader_data.frame_counter);
        
        float2 rnd = float2(RandomFloat(ray_seed), RandomFloat(ray_seed));
        
        float3 radiance = (float3)0.0;
        float3 beta = (float3)1.0;

        RayPayload payload = (RayPayload)0;
        
        for (int bounceIndex = 0; bounceIndex < NUM_BOUNCES; ++bounceIndex)
        {
            payload.distance = -1.0;
            payload.throughput = (float4)1.0;
            payload.color = (float4)0.0;
            payload.distance = -1.0;
            payload.normal = (float3)0.0;
            payload.roughness = 0.0;
            payload.emissive = (float4)0.0;

            RayDesc ray;
            ray.Origin = localOrigin;
            ray.Direction = localDirection;
            ray.TMin = tmin;
            ray.TMax = tmax;

            TraceRay(tlas, flags, 0xff, 0, 1, 0, ray, payload);

            if (payload.distance < 0.0)
            {
#if HAS_ENV_PROBE
                if (current_env_probe.texture_index != ~0u)
                {
                    uint probe_texture_index = max(0, min(current_env_probe.texture_index, HYP_MAX_BOUND_REFLECTION_PROBES - 1));
                    float3 env = EnvProbeSample(sampler_linear, envProbesTexture, probe_texture_index, localDirection, 0.0).rgb;
                    radiance += beta * env;
                }
#endif
                break;
            }

            float3 hitPos = localOrigin + localDirection * payload.distance;
            float3 N = normalize(payload.normal);
            
            if (bounceIndex == 0)
            {
                ray_data.normal += float4(N, 0.0);
                ray_data.direction_depth += float4(localDirection, payload.distance);
            }

            if (length(payload.emissive.rgb) > 0.0)
            {
                radiance += beta * payload.emissive.rgb;
            }

            float3 hitAlbedo = saturate(payload.throughput.rgb);
            
            float hitRoughness = payload.roughness;
            float hitMetalness = saturate(payload.throughput.a);
            
            float3 diffuseColor = hitAlbedo * (1.0 - hitMetalness);
            float3 f0 = CalculateF0(hitAlbedo, hitMetalness);

            for (uint light_index = 0; light_index < ddgiConstants.num_bound_lights; light_index++)
            {
                const Light light = lights[light_index];
                float3 light_color = light.color.rgb * light.position_intensity.w;

                if (light.type == HYP_LIGHT_TYPE_DIRECTIONAL)
                {
                    float3 light_direction = normalize(light.position_intensity.xyz);
                    float3 L = light_direction;

                    float shadow = 1.0 - CheckInShadow(hitPos, N, L);
                    if (shadow > 0.0)
                    {
                        float NdotL = max(dot(N, L), 0.0);
                        
                        if (NdotL > 0.0)
                        {
                            float3 H = normalize(-localDirection + L);
                            float NdotH = max(dot(N, H), 0.0);
                            float LdotH = max(dot(L, H), 0.0);
                            float NdotV = max(dot(N, -localDirection), 0.0);
                            
                            float3 F = F_Schlick(f0, LdotH);
                            float G = V_SmithGGXCorrelated(hitRoughness, NdotV, NdotL);
                            float D = DistributionGGX(NdotH, hitRoughness);
                            
                            radiance += beta * shadow * light_color * NdotL * (
                                (1.0 - F) * diffuseColor * HYP_FMATH_ONE_OVER_PI + 
                                F * G * D
                            );
                        }
                    }
                }
                else if (light.type == HYP_LIGHT_TYPE_POINT)
                {
                    float3 toLight = light.position_intensity.xyz - hitPos;
                    float d2 = max(dot(toLight, toLight), 1e-6);
                    float d = sqrt(d2);
                    float3 L = toLight / d;
                    
                    float shadow = 1.0 - CheckInShadow(hitPos, N, L, max(0.0, d - RAY_OFFSET));
                    float NdotL = max(dot(N, L), 0.0);

                    if (shadow > 0.0 && NdotL > 0.0)
                    {
                        float attenuation = 1.0 / d2;
                        
                        float3 H = normalize(-localDirection + L);
                        float NdotH = max(dot(N, H), 0.0);
                        float LdotH = max(dot(L, H), 0.0);
                        float NdotV = max(dot(N, -localDirection), 0.0);
                        
                        float3 F = F_Schlick(f0, LdotH);
                        float G = V_SmithGGXCorrelated(hitRoughness, NdotV, NdotL);
                        float D = DistributionGGX(NdotH, hitRoughness);
                        
                        radiance += beta * light_color * attenuation * shadow * NdotL * (
                            (1.0 - F) * diffuseColor * HYP_FMATH_ONE_OVER_PI + 
                            F * G * D
                        );
                    }
                }
            }

            // Russian Roulette
            if (bounceIndex >= 2)
            {
                float p = clamp(max(max(beta.r, beta.g), beta.b), 0.05, 0.99);
                if (RandomFloat(ray_seed) > p) {
                    break;
                }
                beta /= float3(p, p, p);
            }
            
            localDirection = normalize(SampleCosineDir(rnd, N));

            beta *= diffuseColor;

            localOrigin = hitPos + N * RAY_OFFSET;
        } // end bounces

        accumRadiance.rgb += radiance;
    } // end samples
    
#if NUM_SAMPLES > 1
    ray_data.normal.xyz = normalize(ray_data.normal.xyz);
    ray_data.direction_depth.xyz = normalize(ray_data.direction_depth.xyz);
    ray_data.direction_depth.w /= NUM_SAMPLES;
#endif

    ray_data.color = accumRadiance / NUM_SAMPLES;

    SetProbeRayData(coord, ray_data);
}
