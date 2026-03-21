#define PATHTRACER
#define LIGHTMAPPER

#include "../../include/defines.inc"
#include "../../include/shared.inc"
#include "../../include/noise.inc"
#include "../../include/packing.inc"

DECLARE_SAMPLER(LightmapPathTracer, SamplerNearest) SamplerState sampler_nearest;
DECLARE_SAMPLER(LightmapPathTracer, SamplerLinear) SamplerState sampler_linear;

#define texture_sampler sampler_linear
#define HYP_SAMPLER_NEAREST sampler_nearest
#define HYP_SAMPLER_LINEAR sampler_linear

DECLARE_SRV(LightmapPathTracer, TLAS) RaytracingAccelerationStructure tlas;

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "../../include/scene.inc"
#include "../../include/packing.inc"
#include "../../include/noise.inc"
#include "../../include/brdf.inc"

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

/// Blue noise
DECLARE_SRV(LightmapPathTracer, BlueNoiseBuffer) StructuredBuffer<int4> BlueNoiseBuffer;

#include "../../include/BlueNoise.inc"
#include "../../include/env_probe.inc"

#if ENV_PROBE_CUBEMAP
DECLARE_SRV(LightmapPathTracer, EnvProbesTexture) TextureCubeArray envProbesTexture;
#else
DECLARE_SRV(LightmapPathTracer, EnvProbesTexture) Texture2DArray envProbesTexture;
#endif

DECLARE_BUFFER(LightmapPathTracer, WorldsBuffer) cbuffer WorldsBuffer
{
    WorldShaderData world_shader_data;
};

#include "../../include/Octahedron.inc"

#include "../../include/rt/RayTracingHelpers.inc"
#include "../../include/rt/payload.inc"

DECLARE_UAV(LightmapPathTracer, HitsBuffer) RWStructuredBuffer<float4> hits;

struct LightmapRay
{
    float3 origin;
    float3 direction;
};

DECLARE_SRV(LightmapPathTracer, RaysBuffer) StructuredBuffer<float4> ray_data;

DECLARE_BUFFER_DYNAMIC(LightmapPathTracer, CBuffer) cbuffer CBuffer
{
    RayTracingConstants rayTracingConstants;
    Light lights[MAX_LIGHTS];
    EnvProbe envProbes[MAX_ENV_PROBES];
};

#ifdef MODE_IRRADIANCE

#define RAY_OFFSET 0.005
#define NUM_BOUNCES 8
#define NUM_SAMPLES 1
#elif defined(MODE_FULL)
#define RAY_OFFSET 0.005
#define NUM_BOUNCES 8
#define NUM_SAMPLES 1
#else
#define RAY_OFFSET 0.01
#define NUM_BOUNCES 1
#define NUM_SAMPLES 1
#endif

#ifdef MODE_FULL
#define MAX_SAMPLE_LUMINANCE 20.0
#define MAX_THROUGHPUT_LUMINANCE 10.0
#define ROUGHNESS_FLOOR 0.001

float3 ClampLuminance(in float3 c, float max_lum)
{
    float lum = Luminance(c);
    if (lum > max_lum)
    {
        return c * (max_lum / max(lum, 1e-6));
    }
    return c;
}
#endif

float3 DebugTest_Albedo(in float3 position, in float3 normal, inout RayPayload payload)
{
    float4 saved_throughput = payload.throughput;
    float4 saved_emissive = payload.emissive;
    float saved_distance = payload.distance;
    float3 saved_normal = payload.normal;
    float3 saved_bary = payload.barycentric_coords;

    float3 origin = position + normal * 0.001;
    float3 direction = -normal;

    payload.distance = -1.0;
    payload.throughput = float4(0.0, 0.0, 0.0, 0.0);
    payload.emissive = float4(0.0, 0.0, 0.0, 0.0);
    payload.normal = float3(0.0, 0.0, 0.0);

    RayDesc ray;
    ray.Origin = origin;
    ray.Direction = direction;
    ray.TMin = 0.0;
    ray.TMax = 1.0;

    TraceRay(tlas, RAY_FLAG_FORCE_OPAQUE, 0xff, 0, 1, 0, ray, payload);

    float3 albedo = float3(0.0, 0.0, 0.0);
    if (payload.distance >= 0.0)
    {
        albedo = payload.throughput.rgb;
    }

    payload.throughput = saved_throughput;
    payload.emissive = saved_emissive;
    payload.distance = saved_distance;
    payload.normal = saved_normal;
    payload.barycentric_coords = saved_bary;

    return albedo;
}

[shader("raygeneration")]
void RayGenMain()
{
    const uint ray_index = DispatchRaysIndex().x;

    LightmapRay ray;
    ray.origin = ray_data[ray_index * 2].xyz;
    ray.direction = ray_data[ray_index * 2 + 1].xyz;

    const RAY_FLAG flags = RAY_FLAG_FORCE_OPAQUE;
    const float tmin = 0.01;
    const float tmax = 1000.0;

    const float3 N0 = normalize(ray.direction);

    float3 tangent;
    float3 bitangent;

    uint ray_seed = InitRandomSeed(((rayTracingConstants.ray_offset + ray_index) * 2), ((rayTracingConstants.ray_offset + ray_index) * 2) + 1);

    RayPayload payload = (RayPayload)0;

#ifdef MODE_IRRADIANCE
    float4 accumRadiance = float4(0.0, 0.0, 0.0, 0.0);

    for (uint sample_index = 0; sample_index < NUM_SAMPLES; sample_index++)
    {
        float2 rnd = float2(RandomFloat(ray_seed), RandomFloat(ray_seed));

        float3 direction = SampleCosineDir(rnd, N0);
        direction = normalize(direction);

        float3 origin = ray.origin + N0 * RAY_OFFSET;

        float3 radiance = float3(0.0, 0.0, 0.0);
        float3 beta = float3(1.0, 1.0, 1.0);

        for (int bounceIndex = 0; bounceIndex < NUM_BOUNCES; ++bounceIndex)
        {
            // perform scene trace using global payload
            payload.distance = -1.0;
            payload.throughput = float4(1.0, 1.0, 1.0, 1.0);
            payload.emissive = float4(0.0, 0.0, 0.0, 0.0);
            payload.normal = float3(0.0, 0.0, 0.0);

            RayDesc rayDesc;
            rayDesc.Origin = origin;
            rayDesc.Direction = direction;
            rayDesc.TMin = tmin;
            rayDesc.TMax = tmax;

            TraceRay(tlas, flags, 0xff, 0, 1, 0, rayDesc, payload);

            if (payload.distance < 0.0)
            {
                float4 environmentRadiance = (float4)0.0;

                for (uint envProbeIdx = 0; envProbeIdx < rayTracingConstants.numBoundEnvProbes && environmentRadiance.a < 1.0; envProbeIdx++)
                {
                    const EnvProbe envProbe = envProbes[envProbeIdx];
                    float4 env = EnvProbeSample(sampler_linear, envProbesTexture, envProbe.texture_index, direction, 0.0) * (1.0 - environmentRadiance.a);
                    environmentRadiance += env;
                }
                
                radiance += beta * environmentRadiance.rgb;
                    
                break;
            }

            float3 albedo = clamp(payload.throughput.rgb, float3(0.0, 0.0, 0.0), float3(1.0, 1.0, 1.0));
            float metalness = payload.throughput.a;

            float3 hitPos = origin + direction * payload.distance;
            float3 N = normalize(payload.normal);

            // emissive
            if (length(payload.emissive.rgb) > 0.0)
            {
                radiance += beta * payload.emissive.rgb;
            }

            beta *= albedo * (1.0 - metalness) * HYP_FMATH_ONE_OVER_PI;

            for (uint light_index = 0; light_index < rayTracingConstants.numBoundLights; light_index++)
            {
                const Light light = lights[light_index];
                float3 light_color = light.color.rgb * light.position_intensity.w;

                if (light.type == HYP_LIGHT_TYPE_DIRECTIONAL)
                {
                    float3 light_direction = normalize(light.position_intensity.xyz);
                    float3 L = light_direction;

                    // shadow check
                    float shadow = 1.0 - CheckInShadow(hitPos, N, L);

                    float NdotL = max(dot(N, L), 0.0);
                    radiance += beta * NdotL * shadow * light_color;
                }
                else if (light.type == HYP_LIGHT_TYPE_POINT)
                {
                    float3 L = normalize(light.position_intensity.xyz - hitPos);
                    float d = length(light.position_intensity.xyz - hitPos);
                    float attenuation = 1.0 / (d * d);

                    float NdotL = max(dot(N, L), 0.0);
                    radiance += beta * NdotL * light_color * attenuation;
                }
                else
                {
                    /// ... TODO
                }
            }

            if (bounceIndex >= 3)
            {
                float p = clamp(max(max(beta.r, beta.g), beta.b), 0.05, 0.99);
                if (RandomFloat(ray_seed) > p)
                {
                    break;
                }
                beta /= float3(p, p, p);
            }

            rnd = float2(RandomFloat(ray_seed), RandomFloat(ray_seed));

            direction = normalize(SampleCosineDir(rnd, N));
            origin = hitPos + N * RAY_OFFSET;
        } // end bounces

        accumRadiance.rgb += radiance;
    } // end samples

    float3 finalColor = accumRadiance.rgb / float(NUM_SAMPLES);
#elif defined(MODE_RADIANCE)
    // direct shading

    const float3 N = N0;

    float3 radiance = float3(0.0, 0.0, 0.0);

    for (uint light_index = 0; light_index < rayTracingConstants.numBoundLights; light_index++)
    {
        if (lights[light_index].type == HYP_LIGHT_TYPE_DIRECTIONAL)
        {
            float3 light_direction = normalize(lights[light_index].position_intensity.xyz);
            float3 L = light_direction;

            // shadow check
            float shadow = 1.0 - CheckInShadow(ray.origin, N, L);

            float NdotL = max(dot(N, L), 0.0);
            radiance += lights[light_index].color.rgb * lights[light_index].position_intensity.w * NdotL * shadow;
        }
        else if (lights[light_index].type == HYP_LIGHT_TYPE_POINT)
        {
            float3 L = normalize(lights[light_index].position_intensity.xyz - ray.origin);
            float d = length(lights[light_index].position_intensity.xyz - ray.origin);
            float attenuation = 1.0 / (d * d);

            float NdotL = max(dot(N, L), 0.0);
            radiance += lights[light_index].color.rgb * lights[light_index].position_intensity.w * NdotL * attenuation;
        }
        else
        {
            /// ... TODO
        }
    }

    float3 finalColor = radiance;
#elif defined(MODE_FULL)
    // full path tracing with diffuse/specular bounces
    float4 accumRadiance = (float4)0.0;

    for (uint sample_index = 0; sample_index < NUM_SAMPLES; sample_index++)
    {
        float2 rnd0 = float2(RandomFloat(ray_seed), RandomFloat(ray_seed));

        float3 direction = N0;
        float3 origin = ray.origin + N0 * RAY_OFFSET;

        float4 Li = (float4)0.0;
        float3 beta = (float3)1.0;

        for (int bounceIndex = 0; bounceIndex < NUM_BOUNCES; ++bounceIndex)
        {
            // prepare payload and trace
            payload.distance = -1.0;
            payload.throughput = float4(1.0, 1.0, 1.0, 1.0);
            payload.emissive = float4(0.0, 0.0, 0.0, 0.0);
            payload.normal = float3(0.0, 0.0, 0.0);

            RayDesc rayDesc;
            rayDesc.Origin = origin;
            rayDesc.Direction = direction;
            rayDesc.TMin = tmin;
            rayDesc.TMax = tmax;

            TraceRay(tlas, flags, 0xff, 0, 1, 0, rayDesc, payload);

            // environment if miss
            if (payload.distance < 0.0)
            {
                float4 environmentRadiance = (float4)0.0;

                for (uint envProbeIdx = 0; envProbeIdx < rayTracingConstants.numBoundEnvProbes && environmentRadiance.a < 1.0; envProbeIdx++)
                {
                    const EnvProbe envProbe = envProbes[envProbeIdx];
                    float4 env = EnvProbeSample(sampler_linear, envProbesTexture, envProbe.texture_index, direction, 0.0) * (1.0 - environmentRadiance.a);
                    environmentRadiance += env;
                }

                // we use 0.0 so that probes can blend between each other.
                // we still want to keep the environment contribution, as we can use it as indirect lighting
                // (plus we can keep the data of the env radiance around in the color channels so it can be used)
                Li += float4(beta * environmentRadiance.rgb, 0.0);

                break;
            }

            // mark sample valid for color.
            Li.a = 1.0;

            // hit data
            float3 hitPos = origin + direction * payload.distance;
            float3 N = normalize(payload.normal);
            float3 baseColor = clamp(payload.throughput.rgb, float3(0.0, 0.0, 0.0), float3(1.0, 1.0, 1.0));
            float metalness = clamp(payload.throughput.a, 0.0, 1.0);
            float roughness = clamp(payload.roughness, ROUGHNESS_FLOOR, 1.0);

            // emissive contribution
            if (any(payload.emissive.rgb > float3(0.0, 0.0, 0.0)))
            {
                Li += float4(beta * payload.emissive.rgb, 1.0);
            }

            float3 diffuseColor = baseColor * (1.0 - metalness);
            float3 V = normalize(-direction);
            float NdotV = max(dot(N, V), 0.0);
            float3 F0 = lerp(float3(0.04, 0.04, 0.04), baseColor, metalness);
            if (dot(diffuseColor, diffuseColor) > 0.0)
            {
                for (uint light_index = 0; light_index < rayTracingConstants.numBoundLights; light_index++)
                {
                    const Light light = lights[light_index];

                    if (light.type == HYP_LIGHT_TYPE_DIRECTIONAL)
                    {
                        float3 L = normalize(light.position_intensity.xyz);
                        float visibility = 1.0 - CheckInShadow(hitPos, N, L);
                        float NdotL = max(dot(N, L), 0.0);
                        if (NdotL > 0.0 && visibility > 0.0)
                        {
                            float3 light_color = light.color.rgb * light.position_intensity.w;
                            // Diffuse
                            float3 Lo_d = diffuseColor * (NdotL * HYP_FMATH_ONE_OVER_PI);
                            // Specular GGX
                            float3 H = normalize(V + L);
                            float NdotH = max(dot(N, H), 0.0);
                            float HdotV = max(dot(H, V), 0.0);
                            float D = DistributionGGX(NdotH, roughness);
                            float G = G_Smith(NdotV, NdotL, roughness);
                            float3 F = F_Schlick(F0, HdotV);
                            float3 Lo_s = (D * G) * F / max(4.0 * NdotL * NdotV, 1e-6);
                            Li += float4(beta * (Lo_d + Lo_s * NdotL) * light_color * visibility, 1.0);
                        }
                    }
                    else if (light.type == HYP_LIGHT_TYPE_POINT)
                    {
                        float3 toLight = light.position_intensity.xyz - hitPos;
                        float d2 = max(dot(toLight, toLight), 1e-6);
                        float d = sqrt(d2);
                        float3 L = toLight / d;
                        float visibility = 1.0 - CheckInShadow(hitPos, N, L, max(0.0, d - RAY_OFFSET));
                        float NdotL = max(dot(N, L), 0.0);
                        if (NdotL > 0.0 && visibility > 0.0)
                        {
                            float3 light_color = light.color.rgb * light.position_intensity.w;
                            float attenuation = 1.0 / d2;
                            // Diffuse
                            float3 Lo_d = diffuseColor * (NdotL * HYP_FMATH_ONE_OVER_PI);
                            // Specular GGX
                            float3 H = normalize(V + L);
                            float NdotH = max(dot(N, H), 0.0);
                            float HdotV = max(dot(H, V), 0.0);
                            float D = DistributionGGX(NdotH, roughness);
                            float G = G_Smith(NdotV, NdotL, roughness);
                            float3 F = F_Schlick(F0, HdotV);
                            float3 Lo_s = (D * G) * F / max(4.0 * NdotL * NdotV, 1e-6);
                            Li += float4(beta * (Lo_d + Lo_s * NdotL) * light_color * attenuation * visibility, 1.0);
                        }
                    }
                    else
                    {
                        // TODO: area/spotlights
                    }
                }
            }

            // Russian roulette
            if (bounceIndex >= 3)
            {
                float p = clamp(max(max(beta.r, beta.g), beta.b), 0.05, 0.99);
                if (RandomFloat(ray_seed) > p)
                {
                    break;
                }
                beta /= float3(p, p, p);
            }

            float Fv = max(max(F_Schlick(F0, NdotV).r, F_Schlick(F0, NdotV).g), F_Schlick(F0, NdotV).b);
            float specProb = clamp(lerp(0.2, 1.0, metalness) * Fv, 0.02, 0.98);
            float diffProb = 1.0 - specProb;

            float2 rnd = float2(RandomFloat(ray_seed), RandomFloat(ray_seed));
            float choose = RandomFloat(ray_seed);
            float3 wi;

            if (choose < specProb)
            {
                // Specular GGX sample
                float3 H = SampleGGX(rnd, roughness, N);
                wi = normalize(reflect(-V, H));
                float NdotL = max(dot(N, wi), 0.0);
                if (NdotL <= 0.0)
                {
                    break;
                }
                float NdotH = max(dot(N, H), 0.0);
                float HdotV = max(dot(H, V), 0.0);
                float G = G_Smith(NdotV, NdotL, roughness);
                float3 F = F_Schlick(F0, HdotV);
                float pdf_spec = max(GGX_PDF(NdotH, HdotV, roughness), 1e-6);
                float3 weight = (F * G) * (HdotV / max(NdotV * NdotH, 1e-6));
                beta *= weight / specProb;
            }
            else
            {
                // Diffuse cosine-weighted sample
                wi = normalize(SampleCosineDir(rnd, N));
                beta *= diffuseColor / max(diffProb, 1e-6);
            }

            origin = hitPos + N * RAY_OFFSET;
            direction = wi;
        }

        accumRadiance += Li;
    }

    float4 finalColor = accumRadiance / float(NUM_SAMPLES);

    // make sure alpha is in [0, 1], it is used for blending between probes, so we need to make sure this won't
    // cause lerp() to get borked.
    finalColor.a = saturate(finalColor.a);

#else
    // shouldn't get here; output green so it's really obvious
    float4 finalColor = float3(0.0, 1.0, 0.0, 1.0);
#endif

    hits[ray_index] = finalColor;
}
