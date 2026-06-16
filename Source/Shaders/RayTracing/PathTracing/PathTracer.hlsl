#define PATHTRACER

#include "../../include/Defines.hlsli"
#include "../../include/Shared.hlsli"
#include "../../include/Noise.hlsli"
#include "../../include/Packing.hlsli"

DECLARE_SRV(PathTracer, GBufferAlbedoTexture) Texture2D gbuffer_albedo_texture;
DECLARE_SRV(PathTracer, GBufferNormalsTexture) Texture2D gbuffer_normals_texture;
DECLARE_SRV(PathTracer, GBufferMaterialTexture) Texture2D<uint> gbuffer_material_texture;
DECLARE_SRV(PathTracer, GBufferVelocityTexture) Texture2D gbuffer_velocity_texture;

DECLARE_SRV(PathTracer, GBufferMipChain) Texture2D gbuffer_mip_chain;
DECLARE_SRV(PathTracer, GBufferDepthTexture) Texture2D gbuffer_depth_texture;

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

DECLARE_SRV_DYNAMIC(PathTracer, CurrentEnvProbe) StructuredBuffer<EnvProbe> current_env_probe_buffer;
#define current_env_probe current_env_probe_buffer[0]

#if ENV_PROBE_CUBEMAP
DECLARE_SRV(PathTracer, EnvProbesColorTexture) TextureCubeArray envProbesColorTexture;
#else
DECLARE_SRV(PathTracer, EnvProbesColorTexture) Texture2DArray envProbesColorTexture;
#endif

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

    const float4 normalSample = SAMPLE_TEXTURE_2D_LOD(sampler_nearest, gbuffer_normals_texture, uv, 0.0);
    const float3 normal = normalize(GBufferUnpackNormal(normalSample));
    const float depth = SAMPLE_TEXTURE_2D_LOD(sampler_nearest, gbuffer_depth_texture, uv, 0.0).r;
    const float4 worldPosition = ReconstructWorldSpacePositionFromDepth(projection_inverse, view_inverse, uv, depth);

    uint2 gbufferDimensions;
    gbuffer_albedo_texture.GetDimensions(gbufferDimensions.x, gbufferDimensions.y);

    const uint2 gbufferCoord = uint2(uv * max(0, int2(gbufferDimensions) - 1));

    GBufferMaterialParams materialParams;
    GBufferUnpackMaterialParams(normalSample.x, 0 /* don't need mask */, materialParams);

    const float roughness = materialParams.roughness; // alpha (perceptualRoughness^2)
    const float perceptualRoughness = sqrt(roughness);
    const float metalness = materialParams.metalness;

    const float3 V = normalize(camera.position.xyz - worldPosition.xyz);
    const float3 R = reflect(-V, normal);

    const RAY_FLAG flags = RAY_FLAG_FORCE_OPAQUE;
    const float tmin = 0.05;
    const float tmax = 1000.0;

    float4 color = (float4)0;

    const float3 albedo = SAMPLE_TEXTURE_2D_LOD(sampler_nearest, gbuffer_albedo_texture, uv, 0.0).rgb;
    const float3 N0 = normal;

    float4 accumRadiance = (float4)0;

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
        float3 beta = (float3)1.0;

        {
            float3 L = direction;
            float NdotL = max(dot(N0, L), 0.0);
            float NdotV = max(dot(N0, V), 0.0);

            if (NdotL > 0.0 && NdotV > 0.0)
            {
                float3 H = normalize(V + L);
                float NdotH = max(dot(N0, H), 0.0);
                float LdotH = max(dot(L, H), 0.0);

                float3 F = F_Schlick(F0_init, LdotH);
                float D = DistributionGGX(NdotH, roughness);
                float G = V_SmithGGXCorrelated(roughness * roughness, NdotV, NdotL);

                float3 specularBrdf = F * D * G;
                float3 diffuseBrdf = (1.0 - F) * (1.0 - metalness) * albedo * HYP_FMATH_ONE_OVER_PI;

                float3 brdf = diffuseBrdf + specularBrdf;

                if (chooseSpecular)
                {
                    float pdf = (D * NdotH) / (4.0 * LdotH);
                    beta = brdf * NdotL / max(pdf * specProb, 1e-6);
                }
                else
                {
                    float pdf = NdotL * HYP_FMATH_ONE_OVER_PI;
                    beta = brdf * NdotL / max(pdf * (1.0 - specProb), 1e-6);
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
                if (current_env_probe.texture_index != ~0u)
                {
                    uint probe_texture_index = max(0, min(current_env_probe.texture_index, HYP_MAX_BOUND_REFLECTION_PROBES - 1));
                    float3 env = EnvProbeSample(sampler_linear, envProbesColorTexture, probe_texture_index, direction, 0.0).rgb * ENVIRONMENT_INTENSITY;
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
            float3 f0 = CalculateF0(hitAlbedo, hitMetalness);

            for (uint light_index = 0; light_index < rayTracingConstants.numBoundLights; light_index++)
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
                            float3 H = normalize(-direction + L);
                            float NdotH = max(dot(N, H), 0.0);
                            float LdotH = max(dot(L, H), 0.0);
                            float NdotV = max(dot(N, -direction), 0.0);

                            float3 F = F_Schlick(f0, LdotH);
                            float G = V_SmithGGXCorrelated(hitRoughness * hitRoughness, NdotV, NdotL);
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

                        float3 H = normalize(-direction + L);
                        float NdotH = max(dot(N, H), 0.0);
                        float LdotH = max(dot(L, H), 0.0);
                        float NdotV = max(dot(N, -direction), 0.0);

                        float3 F = F_Schlick(f0, LdotH);
                        float G = V_SmithGGXCorrelated(hitRoughness * hitRoughness, NdotV, NdotL);
                        float D = DistributionGGX(NdotH, hitRoughness);

                        radiance += beta * light_color * attenuation * shadow * NdotL * (
                            (1.0 - F) * diffuseColor * HYP_FMATH_ONE_OVER_PI +
                            F * G * D
                        );
                    }
                }
            }

            // Russian Roulette
            if (bounceIndex >= 3)
            {
                float p = clamp(max(max(beta.r, beta.g), beta.b), 0.05, 1.0);
                if (RandomFloat(ray_seed) > p) {
                    break;
                }
                beta /= float3(p, p, p);
            }

            rnd = float2(RandomFloat(ray_seed), RandomFloat(ray_seed));
            direction = normalize(SampleCosineDir(rnd, N));

            beta *= max(diffuseColor, 0.001);

            origin = hitPos + N * RAY_OFFSET;
        } // end bounces

        accumRadiance.rgb += radiance;
    } // end samples

    float3 finalColor = accumRadiance.rgb / float(NUM_SAMPLES);

    // temp debug

    //finalColor = float3(1.0f, 0.0, 1.0);

    image[storage_coord] = float4(finalColor, 1.0);
}
