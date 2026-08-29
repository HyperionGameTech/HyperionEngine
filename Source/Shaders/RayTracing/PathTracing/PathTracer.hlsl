STATIC(MAX_LIGHTS, 4)

#define PATHTRACER

#include "../../include/Defines.hlsli"
#include "../../include/Shared.hlsli"
#include "../../include/Noise.hlsli"
#include "../../include/Packing.hlsli"

DECLARE_SRV(PathTracer, GBufferAlbedoTexture) Texture2D GBufferAlbedoTexture;
DECLARE_SRV(PathTracer, GBufferNormalsTexture) Texture2D GBufferNormalsTexture;
DECLARE_SRV(PathTracer, GBufferMaterialTexture) Texture2D<uint> GBufferMaterialTexture;
DECLARE_SRV(PathTracer, GBufferVelocityTexture) Texture2D GBufferVelocityTexture;

DECLARE_SRV(PathTracer, GBufferMipChain) Texture2D GBufferMipChain;
DECLARE_SRV(PathTracer, GBufferDepthTexture) Texture2D GBufferDepthTexture;

DECLARE_SRV(PathTracer, PointLightShadowMapsTextureArray) TextureCubeArray point_shadow_maps;

DECLARE_SAMPLER(PathTracer, SamplerNearest) SamplerState sampler_nearest;
DECLARE_SAMPLER(PathTracer, SamplerLinear) SamplerState sampler_linear;

#define texture_sampler sampler_linear
#define HYP_SAMPLER_NEAREST sampler_nearest
#define HYP_SAMPLER_LINEAR sampler_linear

DECLARE_SRV(PathTracer, TLAS) RaytracingAccelerationStructure tlas;
DECLARE_UAV(PathTracer, OutputImage) RWTexture2D<float4> image;

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "../../include/Gbuffer.hlsli"
#include "../../include/Scene.hlsli"
#include "../../include/Packing.hlsli"
#include "../../include/Noise.hlsli"
#include "../../include/BRDF.hlsli"

/// Blue noise
DECLARE_SRV(PathTracer, BlueNoiseBuffer) StructuredBuffer<int4> BlueNoiseBuffer;

#include "../../include/BlueNoise.hlsli"

#include "../../include/EnvProbes.hlsli"

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

DECLARE_SRV(PathTracer, WorldsBuffer) StructuredBuffer<WorldShaderData> _worlds_buffer;
#define world_shader_data _worlds_buffer[0]

DECLARE_SRV_DYNAMIC(PathTracer, CurrentEnvProbe) StructuredBuffer<EnvProbe> EnvProbesBuffer;
#define currentEnvProbe EnvProbesBuffer[0]

DECLARE_SRV(PathTracer, EnvProbesColorTexture) TextureCubeArray envProbesColorTexture;

#include "../../include/Octahedron.hlsli"

#include "../../include/RayTracing/RayTracingHelpers.hlsli"
#include "../../include/RayTracing/Payload.hlsli"

DECLARE_BUFFER_DYNAMIC(PathTracer, CBuffer) cbuffer CBuffer
{
    RayTracingConstants rayTracingConstants;
    Camera camera;
    Light lights[MAX_LIGHTS];
    ShadowMap shadowMaps[MAX_LIGHTS];
};

#define RAY_OFFSET 0.01
#define NUM_SAMPLES 1
#define NUM_BOUNCES 4
#define ENVIRONMENT_INTENSITY 1.0

float3 EvaluateDirectLighting(float3 position, float3 N, float3 view_dir, float3 albedo, float roughness, float metalness)
{
    const float3 diffuse_color = albedo * (1.0 - metalness);
    const float3 f0 = CalculateF0(albedo, metalness);

    float3 result = (float3)0;

    for (uint light_index = 0; light_index < rayTracingConstants.numBoundLights; light_index++)
    {
        const Light light = lights[light_index];
        const float3 light_color = light.color.rgb * light.position_intensity.w;

        float3 L = (float3)0;
        float attenuation = 1.0;
        float shadow_max_dist = -1.0;

        if (light.type == HYP_LIGHT_TYPE_DIRECTIONAL)
        {
            L = normalize(light.position_intensity.xyz);
        }
        else if (light.type == HYP_LIGHT_TYPE_POINT || light.type == HYP_LIGHT_TYPE_SPOT)
        {
            const float3 to_light = light.position_intensity.xyz - position;
            const float d2 = max(dot(to_light, to_light), 1e-6);
            const float d = sqrt(d2);
            L = to_light / d;

            const float2 radius_falloff = float2(f16tof32(light.radiusFalloffPacked), f16tof32(light.radiusFalloffPacked >> 16));

            attenuation = GetSquareFalloffAttenuation(position, light.position_intensity.xyz, radius_falloff.x);

            if (light.type == HYP_LIGHT_TYPE_SPOT)
            {
                const float theta = max(dot(-L, normalize(light.normal.xyz)), 0.0);
                const float2 spot_angles = light.area_size.xy;

                attenuation *= saturate((theta - spot_angles[0]) / (spot_angles[1] - spot_angles[0])) * step(spot_angles[0], theta);
            }

            shadow_max_dist = max(0.0, d - RAY_OFFSET);
        }
        else
        {
            continue;
        }

        const float NdotL = max(dot(N, L), 0.0);

        if (NdotL <= 0.0 || attenuation <= 0.0)
        {
            continue;
        }

        const float shadow = 1.0 - CheckInShadow(position, N, L, shadow_max_dist);

        if (shadow <= 0.0)
        {
            continue;
        }

        const float3 H = normalize(view_dir + L);
        const float NdotH = max(dot(N, H), 0.0);
        const float LdotH = max(dot(L, H), 0.0);
        const float NdotV = max(dot(N, view_dir), 0.0);

        const float3 F = F_Schlick(f0, LdotH);
        const float G = V_SmithGGXCorrelated(roughness * roughness, NdotV, NdotL);
        const float D = DistributionGGX(NdotH, roughness);

        result += attenuation * shadow * light_color * NdotL * (
            (1.0 - F) * diffuse_color * HYP_FMATH_ONE_OVER_PI +
            F * G * D
        );
    }

    return result;
}

[shader("raygeneration")]
void RayGenMain()
{
    const int2 resolution = rayTracingConstants.output_image_resolution;

    const int pixel_index = int(DispatchRaysIndex().x);
    const int2 storage_coord = int2(
        pixel_index % resolution.x,
        pixel_index / resolution.x
    );

    if (pixel_index >= resolution.x * resolution.y)
    {
        return;
    }

    const float2 uv = (float2(storage_coord) + 0.5) / float2(resolution);

    const float4x4 view_inverse = camera.invViewMat;
    const float4x4 projection_inverse = camera.invProjMat;

    const float4 normalSample = SAMPLE_TEXTURE_2D_LOD(sampler_nearest, GBufferNormalsTexture, uv, 0.0);
    const float3 normal = normalize(GBufferUnpackNormal(normalSample));
    const float depth = SAMPLE_TEXTURE_2D_LOD(sampler_nearest, GBufferDepthTexture, uv, 0.0).r;
    const float4 worldPosition = ReconstructWorldSpacePositionFromDepth(projection_inverse, view_inverse, uv, depth);

    GBufferMaterialParams materialParams;
    GBufferUnpackMaterialParams(normalSample.x, 0 /* don't need mask */, materialParams);

    const float roughness = materialParams.roughness; // alpha (perceptualRoughness^2)
    const float perceptualRoughness = sqrt(roughness);
    const float metalness = materialParams.metalness;

    const float3 V = normalize(camera.position.xyz - worldPosition.xyz);

    const RAY_FLAG flags = RAY_FLAG_FORCE_OPAQUE;
    const float tmin = 0.05;
    const float tmax = 1000.0;

    const float3 albedo = SAMPLE_TEXTURE_2D_LOD(sampler_nearest, GBufferAlbedoTexture, uv, 0.0).rgb;
    const float3 N0 = normal;

    const float3 primaryDirectLighting = EvaluateDirectLighting(worldPosition.xyz, N0, V, albedo, roughness, metalness);

    float3 accumRadiance = (float3)0;

    // float noise = InterleavedGradientNoiseAnimated(float2(DispatchRaysIndex().xy), world_shader_data.frame_counter % 256);

    for (uint sample_index = 0; sample_index < NUM_SAMPLES; sample_index++)
    {
        float3 origin = worldPosition.xyz + N0 * RAY_OFFSET;
        float3 direction;

        uint ray_seed = pcg_hash((DispatchRaysIndex().x * NUM_SAMPLES) + sample_index + (world_shader_data.frame_counter * 997));

        float2 rnd = float2(RandomFloat(ray_seed), RandomFloat(ray_seed));

        float3 F0_init = CalculateF0(albedo, metalness);
        float3 F_init = F_Schlick(F0_init, max(dot(N0, V), 0.0));
        float specProb = clamp(max(max(F_init.r, F_init.g), F_init.b), 0.05, 0.95);

        bool chooseSpecular = (RandomFloat(ray_seed) < specProb);

        if (chooseSpecular)
        {
            float3 H = SampleGGX(rnd, perceptualRoughness, N0);
            direction = reflect(-V, H);
        }
        else
        {
            direction = normalize(SampleCosineDir(rnd, N0));
        }

        if (dot(N0, direction) <= 0.0)
        {
            continue;
        }

        float3 radiance = (float3)0;

        float3 beta = (float3)0;

        {
            const float NdotL = max(dot(N0, direction), 0.0);
            const float NdotV = max(dot(N0, V), 0.0);

            if (NdotL > 0.0 && NdotV > 0.0)
            {
                const float3 H = normalize(V + direction);
                const float NdotH = max(dot(N0, H), 0.0);
                const float LdotH = max(dot(direction, H), 0.0);

                const float3 F = F_Schlick(F0_init, LdotH);

                if (chooseSpecular)
                {
                    const float D = DistributionGGX(NdotH, roughness);
                    const float G = V_SmithGGXCorrelated(roughness * roughness, NdotV, NdotL);
                    const float pdf = (D * NdotH) / (4.0 * LdotH);

                    beta = F * D * G * NdotL / max(pdf * specProb, 1e-6);
                }
                else
                {
                    beta = (1.0 - F) * (1.0 - metalness) * albedo / (1.0 - specProb);
                }
            }
        }

        RayPayload payload = (RayPayload)0;

        for (int bounceIndex = 0; bounceIndex < NUM_BOUNCES; ++bounceIndex)
        {
            payload.distance = -1.0;
            payload.throughput = float4(1.0, 1.0, 1.0, 1.0);
            payload.emissive = float4(0.0, 0.0, 0.0, 0.0);
            payload.normal = float3(0.0, 0.0, 0.0);

            RayDesc ray;
            ray.Origin = origin;
            ray.Direction = direction;
            ray.TMin = tmin;
            ray.TMax = tmax;

            TraceRay(tlas, flags, 0xff, 0, 1, 0, ray, payload);

            if (payload.distance < 0.0)
            {
                const uint envProbeTextureIndex = GET_ENV_PROBE_COLOR_TEXTURE_INDEX(currentEnvProbe);

                if (envProbeTextureIndex != INVALID_ENV_PROBE_TEXTURE)
                {
                    float3 env = EnvProbeSample(sampler_linear, envProbesColorTexture, envProbeTextureIndex, direction, 0.0).rgb * ENVIRONMENT_INTENSITY;
                    radiance += beta * env;
                }
                break;
            }

            float3 hitPos = origin + direction * payload.distance;
            float3 N = payload.normal;

            if (length(payload.emissive.rgb) > 0.0)
            {
                radiance += beta * payload.emissive.rgb;
            }

            float3 hitAlbedo = payload.throughput.rgb;

            float hitRoughness = clamp(payload.roughness, 0.05, 0.95); // alpha
            float hitMetalness = clamp(payload.throughput.w, 0.0, 1.0);

            float3 diffuseColor = hitAlbedo * (1.0 - hitMetalness);

            if (bounceIndex > 0)
            {
                radiance += beta * EvaluateDirectLighting(hitPos, N, -direction, hitAlbedo, hitRoughness, hitMetalness);
            }

            // RR
            if (bounceIndex >= 2)
            {
                float p = clamp(max(max(beta.r, beta.g), beta.b), 0.05, 1.0);
                if (RandomFloat(ray_seed) > p) {
                    break;
                }
                beta /= float3(p, p, p);
            }
            
            const float3 hitF0 = CalculateF0(hitAlbedo, hitMetalness);
            const float hitNdotV = max(dot(N, -direction), 0.0);
            const float3 hitF = F_Schlick(hitF0, hitNdotV);
            const float contSpecProb = clamp(max(max(hitF.r, hitF.g), hitF.b), 0.05, 0.95);

            float3 newDirection;
            bool absorbed = false;

            if (RandomFloat(ray_seed) < contSpecProb)
            {
                const float2 rndSpec = float2(RandomFloat(ray_seed), RandomFloat(ray_seed));
                const float3 H = SampleGGX(rndSpec, sqrt(hitRoughness), N);

                newDirection = reflect(direction, H);

                if (dot(N, newDirection) <= 0.0)
                {
                    absorbed = true;
                }
                else
                {
                    const float NdotL = max(dot(N, newDirection), 0.0);
                    const float NdotH = max(dot(N, H), 0.0);
                    const float LdotH = max(dot(newDirection, H), 0.0);

                    const float3 F = F_Schlick(hitF0, LdotH);
                    const float D = DistributionGGX(NdotH, hitRoughness);
                    const float G = V_SmithGGXCorrelated(hitRoughness * hitRoughness, hitNdotV, NdotL);
                    const float pdf = (D * NdotH) / (4.0 * LdotH);

                    beta *= F * D * G * NdotL / max(pdf * contSpecProb, 1e-6);
                }
            }
            else
            {
                const float2 rndDiff = float2(RandomFloat(ray_seed), RandomFloat(ray_seed));

                newDirection = normalize(SampleCosineDir(rndDiff, N));

                const float3 H = normalize(-direction + newDirection);
                const float LdotH = max(dot(newDirection, H), 0.0);
                const float3 F = F_Schlick(hitF0, LdotH);

                beta *= (1.0 - F) * diffuseColor / (1.0 - contSpecProb);
            }

            if (absorbed)
            {
                break;
            }

            direction = newDirection;

            origin = hitPos + N * RAY_OFFSET;
        } // end bounces

        accumRadiance += radiance;
    } // end samples

    float3 finalColor = accumRadiance / float(NUM_SAMPLES) + primaryDirectLighting;

    image[storage_coord] = float4(finalColor, 1.0);
}
