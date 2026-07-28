#define PATHTRACER
#define LIGHTMAPPER

#include "../../include/Defines.hlsli"
#include "../../include/Shared.hlsli"
#include "../../include/Noise.hlsli"
#include "../../include/Packing.hlsli"

PERMUTE(MODE, IRRADIANCE, RADIANCE, FULL, SHADOW, DISTANCE, BENT_NORMAL);

DECLARE_SAMPLER(LightmapPathTracer, SamplerNearest) SamplerState sampler_nearest;
DECLARE_SAMPLER(LightmapPathTracer, SamplerLinear) SamplerState sampler_linear;

#define texture_sampler sampler_linear
#define HYP_SAMPLER_NEAREST sampler_nearest
#define HYP_SAMPLER_LINEAR sampler_linear

DECLARE_SRV(LightmapPathTracer, TLAS) RaytracingAccelerationStructure tlas;

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "../../include/Scene.hlsli"
#include "../../include/Packing.hlsli"
#include "../../include/Noise.hlsli"
#include "../../include/BRDF.hlsli"

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

/// Blue noise
DECLARE_SRV(LightmapPathTracer, BlueNoiseBuffer) StructuredBuffer<int4> BlueNoiseBuffer;

DECLARE_SRV(LightmapPathTracer, EnvProbesColorTexture) TextureCubeArray envProbesColorTexture;

#include "../../include/BlueNoise.hlsli"
#include "../../include/EnvProbes.hlsli"

DECLARE_SRV(LightmapPathTracer, WorldsBuffer) StructuredBuffer<WorldShaderData> _worlds_buffer;
#define world_shader_data _worlds_buffer[0]

#include "../../include/Octahedron.hlsli"

#include "../../include/RayTracing/RayTracingHelpers.hlsli"
#include "../../include/RayTracing/Payload.hlsli"

DECLARE_UAV(LightmapPathTracer, HitsBuffer) RWStructuredBuffer<float4> hits;

struct LightmapRay
{
    float3 origin;
    float3 direction;
};

DECLARE_SRV(LightmapPathTracer, RaysBuffer) StructuredBuffer<float4> ray_data;

DECLARE_BUFFER(LightmapPathTracer, CBuffer) cbuffer CBuffer
{
    RayTracingConstants rayTracingConstants;
    Light lights[MAX_LIGHTS];
    EnvProbe envProbes[MAX_ENV_PROBES];
};

#define RAY_OFFSET 0.025

#define VSM_DEPTH_BIAS_CONSTANT 0.05
#define VSM_DEPTH_BIAS_SLOPE_SCALE 0.02
#define VSM_DEPTH_BIAS_SLOPE_MAX 8.0

#ifdef MODE_IRRADIANCE
#define NUM_BOUNCES 8
#define NUM_SAMPLES 64
#define ENVIRONMENT_INTENSITY 1.0
#elif defined(MODE_FULL)
#define NUM_BOUNCES 16
#define NUM_SAMPLES 64
#define ENVIRONMENT_INTENSITY 1.0
#elif defined(MODE_BENT_NORMAL)
#define NUM_BOUNCES 1
#define NUM_SAMPLES 32
#define ENVIRONMENT_INTENSITY 1.0
#else
#define NUM_BOUNCES 1
#define NUM_SAMPLES 1
#define ENVIRONMENT_INTENSITY 1.0
#endif

#ifdef MODE_FULL
#define MAX_SAMPLE_LUMINANCE 1.0
#define MAX_THROUGHPUT_LUMINANCE 1.0

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

#if defined(MODE_IRRADIANCE) || defined(MODE_FULL)

float3 SampleDirectLighting(in float3 hitPos, in float3 N)
{
    float3 result = float3(0.0, 0.0, 0.0);

    for (uint light_index = 0; light_index < min(rayTracingConstants.numBoundLights, 16); light_index++)
    {
        const Light light = lights[light_index];
        const float3 light_color = light.color.rgb * light.position_intensity.w;

        if (light.type == HYP_LIGHT_TYPE_DIRECTIONAL)
        {
            const float3 L = normalize(light.position_intensity.xyz);
            const float NdotL = max(dot(N, L), 0.0);

            if (NdotL <= 0.0)
            {
                continue;
            }

            const float shadow = 1.0 - CheckInShadow(hitPos, N, L);
            result += light_color * NdotL * shadow;
        }
        else if (light.type == HYP_LIGHT_TYPE_POINT || light.type == HYP_LIGHT_TYPE_SPOT)
        {
            const float3 toLight = light.position_intensity.xyz - hitPos;
            const float d2 = max(dot(toLight, toLight), 1e-6);
            const float d = sqrt(d2);
            const float3 L = toLight / d;

            const float NdotL = max(dot(N, L), 0.0);

            if (NdotL <= 0.0)
            {
                continue;
            }

            const float2 radiusFalloff = float2(f16tof32(light.radiusFalloffPacked), f16tof32(light.radiusFalloffPacked >> 16));
            const float radius = radiusFalloff.x;

            float attenuation = GetSquareFalloffAttenuation(hitPos, light.position_intensity.xyz, radius);

            if (light.type == HYP_LIGHT_TYPE_SPOT)
            {
                const float theta = max(dot(-L, normalize(light.normal.xyz)), 0.0);
                const float2 spot_angles = light.area_size.xy;

                attenuation *= saturate((theta - spot_angles[0]) / (spot_angles[1] - spot_angles[0])) * step(spot_angles[0], theta);
            }

            if (attenuation <= 0.0)
            {
                continue;
            }

            const float shadow = 1.0 - CheckInShadow(hitPos, N, L, max(0.0, d - RAY_OFFSET));
            result += light_color * NdotL * attenuation * shadow;
        }
        // TODO: area lights
    }

    return result;
}
#endif

[shader("raygeneration")]
void RayGenMain()
{
    const uint ray_index = DispatchRaysIndex().x;

    LightmapRay ray;
    ray.origin = ray_data[ray_index * 2].xyz;
    ray.direction = ray_data[ray_index * 2 + 1].xyz;

    const RAY_FLAG flags = RAY_FLAG_FORCE_OPAQUE;
    const float tmin = 0.1;
    const float tmax = 1000.0;

    const float3 firstRayDirection = normalize(ray.direction);

    float3 tangent;
    float3 bitangent;

    uint ray_seed = InitRandomSeed(((rayTracingConstants.ray_offset + ray_index) * 2), ((rayTracingConstants.ray_offset + ray_index) * 2) + 1);

    RayPayload payload = (RayPayload)0;

#ifdef MODE_IRRADIANCE
    float4 accumRadiance = float4(0.0, 0.0, 0.0, 0.0);

    for (uint sample_index = 0; sample_index < NUM_SAMPLES; sample_index++)
    {
        float2 rnd = float2(RandomFloat(ray_seed), RandomFloat(ray_seed));

        float3 direction = SampleCosineDir(rnd, firstRayDirection);
        direction = normalize(direction);

        float3 origin = ray.origin + firstRayDirection * RAY_OFFSET;

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
                if (payload.distance < -1.0)
                {
                    float4 environmentRadiance = (float4)0.0;

                    for (uint envProbeIdx = 0; envProbeIdx < min(rayTracingConstants.numBoundEnvProbes, 16) && environmentRadiance.a < 1.0; envProbeIdx++)
                    {
                        // EnvProbe currentEnvProbe = envProbes[envProbeIdx];

                        // const uint probeType = GET_ENV_PROBE_TYPE(currentEnvProbe);
                        // const bool isSky = (probeType == EPT_SKY);
                        
                        // const float4 aabbMin = currentEnvProbe.aabb_min;
                        // const float4 aabbMax = currentEnvProbe.aabb_max;
                        
                        // const float4 worldPosition = currentEnvProbe.world_position;
                        // const float3 worldPosition3 = worldPosition.xyz;
                        // const float diffuseStrength = worldPosition.w;

                        // const float weight = (isSky ? 1.0 : CalculateEnvProbeWeight(origin, aabbMin.xyz, aabbMax.xyz)) * diffuseStrength * (1.0 - environmentRadiance.a);

                        // const float3 env = EnvProbeSH(currentEnvProbe, direction, /* order */ 2);
                        // environmentRadiance += float4(env, 1.0) * weight;

                        
                        const uint envProbeTextureIndex = GET_ENV_PROBE_COLOR_TEXTURE_INDEX(envProbes[envProbeIdx]);

                        if (envProbeTextureIndex != INVALID_ENV_PROBE_TEXTURE)
                        {
                            float4 env = EnvProbeSample(sampler_linear, envProbesColorTexture, envProbeTextureIndex, direction, 0.0);
                            env *= (1.0 - environmentRadiance.a);
                            environmentRadiance += env * ENVIRONMENT_INTENSITY;
                        }
                    }

                    radiance += beta * environmentRadiance.rgb;
                }

                break;
            }

            float3 albedo = clamp(payload.throughput.rgb, float3(0.0, 0.0, 0.0), float3(1.0, 1.0, 1.0));
            float metalness = clamp(payload.throughput.a, 0.0, 1.0);

            float3 hitPos = origin + direction * payload.distance;
            float3 N = normalize(payload.normal);

            // emissive
            if (any(payload.emissive.rgb > float3(0.0, 0.0, 0.0)))
            {
                radiance += beta * payload.emissive.rgb;
            }

            float3 diffuseColor = albedo * (1.0 - metalness);
            
            radiance += beta * diffuseColor * HYP_FMATH_ONE_OVER_PI * SampleDirectLighting(hitPos, N);

            beta *= diffuseColor;

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

    float4 finalColor = float4(accumRadiance.rgb / float(NUM_SAMPLES), 1.0);
#elif defined(MODE_RADIANCE)
    // direct shading

    const float3 N = firstRayDirection;

    float3 radiance = float3(0.0, 0.0, 0.0);

    for (uint light_index = 0; light_index < min(rayTracingConstants.numBoundLights, 16); light_index++)
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

    float4 finalColor = float4(radiance, 1.0);
#elif defined(MODE_FULL)
    // path traced diffuse-only light.
    float4 accumRadiance = (float4)0.0;

#if 1 // path traced ver
    for (uint sample_index = 0; sample_index < NUM_SAMPLES; sample_index++)
    {
        float2 rnd0 = float2(RandomFloat(ray_seed), RandomFloat(ray_seed));

        float3 direction = firstRayDirection;
        float3 origin = ray.origin + firstRayDirection * RAY_OFFSET;

        float4 Li = (float4)0.0;
        float3 beta = (float3)1.0;

        bool sampleIsMiss = true;

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

            // sample environment if miss
            if (payload.distance < 0.0)
            {
                float4 environmentRadiance = (float4)0.0;

                for (uint envProbeIdx = 0; envProbeIdx < rayTracingConstants.numBoundEnvProbes && environmentRadiance.a < 1.0; envProbeIdx++)
                {
                    // if (envProbeTextureIndex != INVALID_ENV_PROBE_TEXTURE)
                    // {
                    //     float4 env = EnvProbeSample(sampler_linear, envProbesColorTexture, envProbeTextureIndex, direction, 0.0);
                    //     env *= (1.0 - environmentRadiance.a);
                    //     environmentRadiance += env * ENVIRONMENT_INTENSITY;
                    // }

                    
                        EnvProbe currentEnvProbe = envProbes[envProbeIdx];
                        
                        const uint textureIndex = GET_ENV_PROBE_COLOR_TEXTURE_INDEX(currentEnvProbe);

                        const uint probeType = GET_ENV_PROBE_TYPE(currentEnvProbe);
                        const bool isSky = (probeType == EPT_SKY);

                        if (!isSky)
                        {
                            continue;
                        }
                        
                        const float4 aabbMin = currentEnvProbe.aabb_min;
                        const float4 aabbMax = currentEnvProbe.aabb_max;
                        
                        const float4 worldPosition = currentEnvProbe.world_position;
                        const float3 worldPosition3 = worldPosition.xyz;
                        const float diffuseStrength = worldPosition.w;

                        const float weight = (isSky ? 1.0 : CalculateEnvProbeWeight(origin, aabbMin.xyz, aabbMax.xyz)) * diffuseStrength * (1.0 - environmentRadiance.a);

                        const float4 env = EnvProbeSample(sampler_linear, envProbesColorTexture, textureIndex, direction, 0.0);//EnvProbeSH(currentEnvProbe, direction, /* order */ 2);
                        environmentRadiance += env * weight;
                }

                Li += float4(beta * environmentRadiance.rgb, 1.0);

                break;
            }

            // hit something, so this sample is not a miss.
            // we can use environment probes for indirect lighting,
            // but bounces that hit e.g the sky should be left with alpha = 0 so that they can be blended with other probes.
            sampleIsMiss = false;

            // hit data
            float3 hitPos = origin + direction * payload.distance;
            float3 N = normalize(payload.normal);
            float3 baseColor = clamp(payload.throughput.rgb, float3(0.0, 0.0, 0.0), float3(1.0, 1.0, 1.0));
            float metalness = clamp(payload.throughput.a, 0.0, 1.0);

            // emissive contribution
            if (any(payload.emissive.rgb > float3(0.0, 0.0, 0.0)))
            {
                Li += float4(beta * payload.emissive.rgb, 1.0);
            }

            float3 diffuseColor = baseColor * (1.0 - metalness);

            Li += float4(beta * diffuseColor * HYP_FMATH_ONE_OVER_PI * SampleDirectLighting(hitPos, N), 1.0);

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

            float2 rnd = float2(RandomFloat(ray_seed), RandomFloat(ray_seed));
            float3 wi;

            wi = normalize(SampleCosineDir(rnd, N));
            beta *= diffuseColor;

            origin = hitPos + N * RAY_OFFSET;
            direction = wi;
        }

        // if the ray never hit anything, set alpha to 0 so that probes can blend between each other. If it hit something, set alpha to 1 so that the result is not blended with other probes.
        Li.a = sampleIsMiss ? 0.0 : 1.0;
        //Li.a = 1.0;

        accumRadiance += Li;
    }
#else // !old ver

    // this version is less about tracing bounces of rays and more just a way to render a cubemap
    // but uses the RT shaders rather than rasterization.

    for (uint sample_index = 0; sample_index < NUM_SAMPLES; sample_index++)
    {
        float2 rnd0 = float2(RandomFloat(ray_seed), RandomFloat(ray_seed));

        float3 direction = firstRayDirection;
        float3 origin = ray.origin + firstRayDirection * RAY_OFFSET;

        float4 radiance = (float4)0.0;

        bool sampleIsMiss = true;

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

        float4 environmentRadiance = (float4)0.0;

        for (uint envProbeIdx = 0; envProbeIdx < rayTracingConstants.numBoundEnvProbes && environmentRadiance.a < 1.0; envProbeIdx++)
        {
            const uint envProbeTextureIndex = GET_ENV_PROBE_COLOR_TEXTURE_INDEX(currentEnvProbe);

            if (envProbeTextureIndex != INVALID_ENV_PROBE_TEXTURE)
            {
                float4 env = EnvProbeSample(sampler_linear, envProbesColorTexture, envProbeTextureIndex, direction, 6.0);
                env *= (1.0 - environmentRadiance.a);
                environmentRadiance += env * ENVIRONMENT_INTENSITY;
            }
        }

        radiance += float4(environmentRadiance.rgb, 1.0);

        // miss
        if (payload.distance < 0.0)
        {
            accumRadiance += radiance;

            break;
        }

        // hit something, so this sample is not a miss.
        // we can use environment probes for indirect lighting,
        // but bounces that hit e.g the sky should be left with alpha = 0 so that they can be blended with other probes.
        sampleIsMiss = false;

        // hit data
        float3 hitPos = origin + direction * payload.distance;
        float3 N = normalize(payload.normal);
        float3 baseColor = clamp(payload.throughput.rgb, float3(0.0, 0.0, 0.0), float3(1.0, 1.0, 1.0));
        float metalness = clamp(payload.throughput.a, 0.0, 1.0);
        float roughness = clamp(payload.roughness, 0.0, 1.0);

        // emissive contribution
        if (any(payload.emissive.rgb > float3(0.0, 0.0, 0.0)))
        {
            radiance += float4(payload.emissive.rgb, 1.0);
        }

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

                    radiance += float4(light_color * visibility * NdotL, 1.0);
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

                    radiance += float4(light_color * attenuation * visibility * NdotL, 1.0);
                }
            }
            else
            {
                // TODO: area/spotlights
            }
        }

        float3 irradiance = (float3)0.0;
        //use environment irradiance (SH) for diffuse lighting
        for (uint envProbeIdx = 0; envProbeIdx < rayTracingConstants.numBoundEnvProbes; envProbeIdx++)
        {
            const EnvProbe envProbe = envProbes[envProbeIdx];

            irradiance += EnvProbeSH(envProbe, N, /* order */ 2);
        }

        radiance.a = sampleIsMiss ? 0.0 : 1.0;

        float3 diffuseColor = baseColor * (1.0 - metalness);
        accumRadiance += float4(diffuseColor * irradiance, 0.0);
        accumRadiance += radiance * float4(diffuseColor * HYP_FMATH_ONE_OVER_PI, 1.0);
    }
#endif

    float4 finalColor = accumRadiance / float(NUM_SAMPLES);

#elif defined(MODE_SHADOW)
    // We will need to write the shadowmap depth in a way that can be consumed by the rasterizer
    payload.distance = -1.0;
    payload.throughput = float4(1.0, 1.0, 1.0, 1.0);
    payload.emissive = float4(0.0, 0.0, 0.0, 0.0);
    payload.normal = float3(0.0, 0.0, 0.0);

    RayDesc rayDesc;
    rayDesc.Origin = ray.origin + ray.direction * RAY_OFFSET;
    rayDesc.Direction = ray.direction;
    rayDesc.TMin = 0.1;
    rayDesc.TMax = 1500.0;

    TraceRay(tlas, flags, 0xff, 0, 1, 0, rayDesc, payload);

    float4 finalColor;

    if (payload.distance > 0.0)
    {
        // encode depth in red channel
        float dist = payload.distance;
        // get perspective divide factor for this fragment so that we can reconstruct linear depth in the shader that reads the shadowmap
        // float4 clipPos = mul(float4(ray.origin + ray.direction * dist, 1.0), world_shader_data.viewProjMat);

        finalColor = float4(dist, 0.0, 0.0, 1.0);
    }
    else
    {
        // if the ray misses, write a depth value to indicate that the fragment is in light
        finalColor = float4(0.0, 0.0, 0.0, 1.0);
    }
#elif defined(MODE_DISTANCE)
    payload.distance = -1.0;
    payload.throughput = float4(1.0, 1.0, 1.0, 1.0);
    payload.emissive = float4(0.0, 0.0, 0.0, 0.0);
    payload.normal = float3(0.0, 0.0, 0.0);

    RayDesc rayDesc;
    rayDesc.Origin = ray.origin + ray.direction * RAY_OFFSET;
    rayDesc.Direction = ray.direction;
    rayDesc.TMin = 0.1;
    rayDesc.TMax = 1500.0;

    TraceRay(tlas, flags, 0xff, 0, 1, 0, rayDesc, payload);

    float4 finalColor;

    if (payload.distance > 0.0)
    {
        const float3 hitNormal = normalize(payload.normal);
        const float cosTheta = saturate(-dot(hitNormal, ray.direction));
        const float slope = sqrt(saturate(1.0 - cosTheta * cosTheta)) / max(cosTheta, 1e-3);
        const float bias = VSM_DEPTH_BIAS_CONSTANT + VSM_DEPTH_BIAS_SLOPE_SCALE * min(slope, VSM_DEPTH_BIAS_SLOPE_MAX);

        float dist = payload.distance + bias;
        finalColor = float4(dist, dist * dist, 0.0, 1.0);
    }
    else
    {
        // Miss: no occluder in this direction. Use a large distance so VSM
        // visibility resolves to 1.0 (fully visible).
        static const float missDistance = 10000.0;
        finalColor = float4(missDistance, missDistance * missDistance, 0.0, 1.0);
    }
#elif defined(MODE_BENT_NORMAL)
    const float3 N = firstRayDirection;
    const float3 origin = ray.origin + N * RAY_OFFSET;

    float3 accumDirection = float3(0.0, 0.0, 0.0);
    uint numUnoccluded = 0;

    for (uint sample_index = 0; sample_index < NUM_SAMPLES; sample_index++)
    {
        float2 rnd = float2(RandomFloat(ray_seed), RandomFloat(ray_seed));
        float3 direction = normalize(SampleCosineDir(rnd, N));

        payload.distance = -1.0;

        RayDesc rayDesc;
        rayDesc.Origin = origin;
        rayDesc.Direction = direction;
        rayDesc.TMin = tmin;
        rayDesc.TMax = tmax;

        TraceRay(tlas, flags, 0xff, 0, 1, 0, rayDesc, payload);

        if (payload.distance < 0.0)
        {
            accumDirection += direction;
            numUnoccluded++;
        }
    }

    float3 bentNormal = numUnoccluded > 0 ? normalize(accumDirection / float(numUnoccluded)) : N;

    float4 finalColor = float4(bentNormal, 1.0);
#else
    // shouldn't get here; output green so it's really obvious
    float4 finalColor = float4(0.0, 1.0, 0.0, 1.0);
#endif

    finalColor.a = saturate(finalColor.a);
    hits[ray_index] = finalColor;
}
