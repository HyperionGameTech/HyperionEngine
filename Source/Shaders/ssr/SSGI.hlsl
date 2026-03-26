#include "../include/defines.inc"
#include "../include/noise.inc"
#include "../include/env_probe.inc"

#include "./ssr_header.inc"

#include "../include/brdf.inc"

#ifndef OUTPUT_FORMAT
    #if defined(OUTPUT_RGBA8)
        #define OUTPUT_FORMAT rgba8
    #elif defined(OUTPUT_RGBA16F)
        #define OUTPUT_FORMAT rgba16f
    #elif defined(OUTPUT_RGBA32F)
        #define OUTPUT_FORMAT rgba32f
    #else
        #define OUTPUT_FORMAT rgba8
    #endif
#endif

#if defined(OUTPUT_RGBA8) || (!defined(OUTPUT_RGBA16F) && !defined(OUTPUT_RGBA32F))
    #define OUTPUT_UAV_TYPE unorm float4
#else
    #define OUTPUT_UAV_TYPE float4
#endif


struct SSGIConstants
{
    uint2 dimension;
    float rayStep;
    float maxIterations;

    float distanceBias;
    uint numSamples;
    uint numBoundLights;
    uint numBoundEnvProbes;
};

DECLARE_UAV(SSGI, OutImage) RWTexture2D<OUTPUT_UAV_TYPE> out_image;

DECLARE_BUFFER_DYNAMIC(SSGI, CBuffer) cbuffer CBuffer
{
    SSGIConstants ssgiConstants;
    Light lights[MAX_LIGHTS];
    ShadowMap shadowMaps[MAX_LIGHTS];
    EnvProbe envProbes[MAX_ENV_PROBES];
};

DECLARE_SRV(SSGI, GBufferAlbedoTexture) Texture2D gbuffer_albedo_texture;
DECLARE_SRV(SSGI, GBufferNormalsTexture) Texture2D gbuffer_normals_texture;
DECLARE_SRV(SSGI, GBufferMaterialTexture) Texture2D<uint4> gbuffer_material_texture;
DECLARE_SRV(SSGI, GBufferVelocityTexture) Texture2D gbuffer_velocity_texture;

DECLARE_SRV(SSGI, GBufferDepthTexture) Texture2D gbuffer_depth_texture;
DECLARE_SRV(SSGI, DeferredShadingTexture) Texture2D DeferredShadingTexture;

DECLARE_SAMPLER(SSGI, SamplerNearest) SamplerState sampler_nearest;
DECLARE_SAMPLER(SSGI, SamplerLinear) SamplerState sampler_linear;

DECLARE_SRV(SSGI, BlueNoiseBuffer) StructuredBuffer<int4> BlueNoiseBuffer;

DECLARE_BUFFER(SSGI, WorldsBuffer) cbuffer WorldsBuffer
{
    WorldShaderData world_shader_data;
};

DECLARE_BUFFER_DYNAMIC(SSGI, CamerasBuffer) cbuffer CamerasBuffer
{
    Camera camera;
};

DECLARE_SRV(SSGI, ShadowMapsTextureArray) Texture2DArray<float> shadow_maps;
DECLARE_SRV(SSGI, PointLightShadowMapsTextureArray) TextureCubeArray point_shadow_maps;

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../include/scene.inc"
#include "../include/gbuffer.inc"
#include "../include/BlueNoise.inc"
#include "../include/Shadows.hlsli"
#include "../include/Octahedron.inc"
#include "../include/env_probe.inc"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#if ENV_PROBE_CUBEMAP
DECLARE_SRV(SSGI, EnvProbesTexture) TextureCubeArray envProbesTexture;
#else
DECLARE_SRV(SSGI, EnvProbesTexture) Texture2DArray envProbesTexture;
#endif

#define RAY_OFFSET 0.05
#define ENVIRONMENT_INTENSITY 1.0

// amount to 'brighten up' the SSGI result
#define SSGI_INTENSITY 1.0

#if 1

bool TraceRays(
    float3 ray_origin,
    float3 ray_direction,
    out float2 hit_uv,
    out float4 hit_view_space_position,
    out float hit_depth,
    out float out_maxIterations)
{
    bool intersect = false;
    out_maxIterations = 0.0;
    hit_uv = float2(0.0, 0.0);
    hit_depth = 1.0;
    hit_view_space_position = float4(0.0, 0.0, 0.0, 0.0);

    float3 rayStep = ssgiConstants.rayStep * normalize(ray_direction);
    float3 marching_position = ray_origin;
    float step_delta = 0.0;

    int i = 0;

    for (; i < int(ssgiConstants.maxIterations); i++)
    {
        marching_position += rayStep;

        hit_uv = GetProjectedPositionFromView(camera.projection, marching_position);
        hit_depth = SAMPLE_TEXTURE_2D(sampler_nearest, gbuffer_depth_texture, hit_uv).r;
        hit_view_space_position = ReconstructViewSpacePositionFromDepth(camera.invProjMat, hit_uv, hit_depth);

        step_delta = marching_position.z - hit_view_space_position.z;

        intersect = step_delta > 0.0;
        out_maxIterations += 1.0;

        if (intersect)
        {
            break;
        }
    }

    if (intersect)
    {
        // binary search
        for (; i < int(ssgiConstants.maxIterations); i++)
        {
            rayStep *= 0.5;
            marching_position = marching_position - rayStep * sign(step_delta);

            hit_uv = GetProjectedPositionFromView(camera.projection, marching_position);
            hit_depth = SAMPLE_TEXTURE_2D(sampler_nearest, gbuffer_depth_texture, hit_uv).r;
            hit_view_space_position = ReconstructViewSpacePositionFromDepth(camera.invProjMat, hit_uv, hit_depth);

            step_delta = abs(marching_position.z) - hit_view_space_position.z;

            intersect = abs(step_delta) < ssgiConstants.distanceBias;

            if (abs(step_delta) < ssgiConstants.distanceBias)
            {
                return true;
            }
        }
    }

    if (i < int(ssgiConstants.maxIterations))
    {
        hit_depth = 1.0;
    }

    return false;
}

float CalculateAlpha(
    float maxIterations,
    float2 hit_uv,
    float3 hit_normal,
    float3 ray_direction)
{
    float alpha = 1.0;

    // Fade ray hits that approach the maximum iterations
    alpha *= 1.0 - (maxIterations / ssgiConstants.maxIterations);
    
    // Fade ray hits that approach the screen edge
    float2 uvNDC = hit_uv * 2.0 - 1.0;
    float maxDimension = saturate(max(abs(uvNDC.x), abs(uvNDC.y)));
    alpha *= 1.0 - max(0.0, maxDimension - 0.98) / 0.02;

    return alpha;
}

[numthreads(256, 1, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    const uint pixel_index = dispatchThreadID.x;
    const uint2 coord = uint2(
        pixel_index % ssgiConstants.dimension.x,
        pixel_index / ssgiConstants.dimension.x);

    if (any(coord >= ssgiConstants.dimension.xy))
    {
        return;
    }

    const float2 texcoord = saturate((float2(coord) + 0.5) / float2(ssgiConstants.dimension.xy));

    uint2 gbufferDimensions;
    gbuffer_material_texture.GetDimensions(gbufferDimensions.x, gbufferDimensions.y);

    const float4 normalSample = SAMPLE_TEXTURE_2D(sampler_nearest, gbuffer_normals_texture, texcoord);

    GBufferMaterialParams materialParams;
    GBufferUnpackMaterialParams(normalSample.x, 0, materialParams);

    const float depth = SAMPLE_TEXTURE_2D(sampler_nearest, gbuffer_depth_texture, texcoord).r;

    if (depth > 0.9999)
    {
        out_image[coord] = (float4)0.0;
        return;
    }

    const float3 P = ReconstructViewSpacePositionFromDepth(camera.invProjMat, texcoord, depth).xyz;
    const float3 V = normalize((float3)0.0 - P);
    const float3 N = GBufferUnpackNormal(normalSample);
    const float3 view_space_normal = normalize(mul(camera.view, float4(N, 0.0)).xyz);
    const float2 velocity = SAMPLE_TEXTURE_2D(sampler_linear, gbuffer_velocity_texture, texcoord).xy;

    float2 hit_uv;
    float4 hit_view_space_position;
    float hit_depth;
    float maxIterations;
    
    float4 accum_result = (float4)0.0;

    float3 tangent;
    float3 bitangent;
    ComputeOrthonormalBasis(view_space_normal, tangent, bitangent);

    float phi = InterleavedGradientNoise(float2(coord));

    const uint numRaySamples = 10; // local (per dispatch) sample count.
    const uint temporalSampleIndex = (world_shader_data.frame_counter % ssgiConstants.numSamples);
    const uint numSamplesTotal = ssgiConstants.numSamples * numRaySamples;

    for (uint rayIndex = 0; rayIndex < numRaySamples; rayIndex++)
    {
        const uint sampleIndex = temporalSampleIndex * numRaySamples + rayIndex;
        
        const float2 rnd = (float2)SampleBlueNoise(int(coord.x), int(coord.y), sampleIndex, numSamplesTotal);

        const float3 d = SampleCosineWeightedHemisphere(rnd);

        const float3 ray_direction = normalize(tangent * d.x + bitangent * d.y + view_space_normal * d.z);
        const float3 ray_origin = P + N * RAY_OFFSET;

        if (TraceRays(ray_origin, ray_direction, hit_uv, hit_view_space_position, hit_depth, maxIterations))
        {
            float3 hit_normal = GBufferUnpackNormal(SAMPLE_TEXTURE_2D(sampler_nearest, gbuffer_normals_texture, hit_uv));
            float alpha = CalculateAlpha(maxIterations, hit_uv, hit_normal, ray_direction);

            if (alpha > HYP_FMATH_EPSILON)
            {
                float2 sample_uv = saturate(hit_uv);
                float4 color = SAMPLE_TEXTURE_2D_LOD(sampler_linear, DeferredShadingTexture, sample_uv, 0.0);

                accum_result += float4(color.rgb, 1.0) * alpha;

                continue;
            }
        }

        // sample environment
        float3 rayDirWorld = normalize(mul(camera.invViewMat, float4(ray_direction, 0.0)).xyz);
        
        float4 environmentRadiance = (float4)0.0;

        for (uint envProbeIdx = 0; envProbeIdx < ssgiConstants.numBoundEnvProbes && environmentRadiance.a < 1.0; envProbeIdx++)
        {
            EnvProbe envProbe = envProbes[envProbeIdx];

            if (envProbe.texture_index == ~0u)
            {
                continue;
            }
            
            environmentRadiance += EnvProbeSample(sampler_linear, envProbesTexture, envProbe.texture_index, rayDirWorld, 6.0)
                * ENVIRONMENT_INTENSITY
                * (1.0 - environmentRadiance.a);
        }
        
        // use 0 for alpha, so we can blend with other GI if available.
        accum_result += float4(environmentRadiance.rgb, 0.0) * SSGI_INTENSITY;
    }

    out_image[coord] = accum_result / float(numRaySamples);
}

#else

/// This pass is not optimized, just meant for toying around
/// Pretty much brute force - do not use

#define NUM_BOUNCES 4

float4 SampleSky(float3 dir)
{
    float4 environmentRadiance = (float4)0.0;

    for (uint envProbeIdx = 0; envProbeIdx < ssgiConstants.numBoundEnvProbes && environmentRadiance.a < 1.0; envProbeIdx++)
    {
        EnvProbe envProbe = envProbes[envProbeIdx];

        if (envProbe.texture_index == ~0u)
        {
            continue;
        }
        
        environmentRadiance += EnvProbeSample(sampler_linear, envProbesTexture, envProbe.texture_index, dir, 6.0)
            * ENVIRONMENT_INTENSITY
            * (1.0 - environmentRadiance.a);
    }
    
    return float4(environmentRadiance.rgb, 0.0);
}

bool TraceScreenSpaceRay(
    float3 ray_origin,
    float3 ray_direction,
    out float2 hit_uv,
    out float4 hit_view_space_position,
    out float hit_depth,
    out float out_num_iterations)
{
    bool intersect = false;
    out_num_iterations = 0.0;
    hit_uv = (float2)0.0;
    hit_depth = 1.0;
    hit_view_space_position = (float4)0.0;

    float3 rayStep = ssgiConstants.rayStep * normalize(ray_direction);
    float3 marching_position = ray_origin;
    float step_delta = 0.0;

    int i = 0;

    for (; i < int(ssgiConstants.maxIterations); i++)
    {
        marching_position += rayStep;

        hit_uv = GetProjectedPositionFromView(camera.projection, marching_position);
        hit_depth = SAMPLE_TEXTURE_2D(sampler_nearest, gbuffer_depth_texture, hit_uv).r;
        hit_view_space_position = ReconstructViewSpacePositionFromDepth(camera.invProjMat, hit_uv, hit_depth);

        step_delta = marching_position.z - hit_view_space_position.z;

        intersect = step_delta > 0.0;
        out_num_iterations += 1.0;

        if (intersect)
        {
            break;
        }
    }

    if (intersect)
    {
        // binary search refinement
        for (; i < int(ssgiConstants.maxIterations); i++)
        {
            rayStep *= 0.5;
            marching_position = marching_position - rayStep * sign(step_delta);

            hit_uv = GetProjectedPositionFromView(camera.projection, marching_position);
            hit_depth = SAMPLE_TEXTURE_2D(sampler_nearest, gbuffer_depth_texture, hit_uv).r;
            hit_view_space_position = ReconstructViewSpacePositionFromDepth(camera.invProjMat, hit_uv, hit_depth);

            step_delta = abs(marching_position.z) - hit_view_space_position.z;

            if (abs(step_delta) < ssgiConstants.distanceBias)
            {
                return true;
            }
        }
    }

    hit_depth = 1.0;

    return false;
}

float CalculateHitAlpha(
    float num_iterations,
    float2 hit_uv)
{
    float alpha = 1.0;

    // Fade ray hits that approach the maximum iterations
    alpha *= 1.0 - (num_iterations / ssgiConstants.maxIterations);

    // Fade ray hits that approach the screen edge
    float2 hit_uv_ndc = hit_uv * 2.0 - 1.0;
    float max_dimension = saturate(max(abs(hit_uv_ndc.x), abs(hit_uv_ndc.y)));
    alpha *= 1.0 - max(0.0, max_dimension - 0.9) / 0.1;

    return saturate(alpha);
}

float3 CalculateDirectLighting(uint light_index, float3 albedo, float3 P, float3 N, float metalness)
{
    Light light = lights[light_index];
    ShadowMap shadowMap = shadowMaps[light_index];

    float3 light_color = light.color.rgb * light.position_intensity.w;

    float3 L = CalculateLightDirection(light, P);

    float NdotL = max(dot(N, L), 0.0);

    if (NdotL <= 0.0)
    {
        return (float3)0.0;
    }

    float shadow = 1.0;

    if ((light.flags & LF_SHADOW_CASTER) != 0)
    {
        if (light.type == HYP_LIGHT_TYPE_DIRECTIONAL)
        {
            shadow = GetShadowStandard(shadowMap, P, (float2)0.0, NdotL);
        }
        else if (light.type == HYP_LIGHT_TYPE_POINT)
        {
            shadow = GetPointShadowStandard(shadowMap.layerIndex, P - light.position_intensity.xyz, NdotL);
        }
    }

    if (shadow <= 0.0)
    {
        return (float3)0.0;
    }

    float3 diffuseColor = albedo * (1.0 - metalness);

    // don't consider specular
    return shadow * light_color * NdotL * diffuseColor * HYP_FMATH_ONE_OVER_PI;
}

[numthreads(256, 1, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    const uint pixel_index = dispatchThreadID.x;
    const uint2 coord = uint2(
        pixel_index % ssgiConstants.dimension.x,
        pixel_index / ssgiConstants.dimension.x);

    if (any(coord >= ssgiConstants.dimension.xy))
    {
        return;
    }

    const float2 texcoord = saturate((float2(coord) + 0.5) / float2(ssgiConstants.dimension.xy));

    uint2 gbufferDimensions;
    gbuffer_material_texture.GetDimensions(gbufferDimensions.x, gbufferDimensions.y);

    const float4 normalSample = SAMPLE_TEXTURE_2D(sampler_nearest, gbuffer_normals_texture, texcoord);

    GBufferMaterialParams materialParams;
    GBufferUnpackMaterialParams(normalSample.x, 0, materialParams);

    const float depth = SAMPLE_TEXTURE_2D(sampler_nearest, gbuffer_depth_texture, texcoord).r;

    if (depth >= 0.9999)
    {
        out_image[coord] = (float4)0.0;
        return;
    }

    const float3 N_world = GBufferUnpackNormal(normalSample);
    const float3 N_view = normalize(mul(camera.view, float4(N_world, 0.0)).xyz);

    const float3 P_view = ReconstructViewSpacePositionFromDepth(camera.invProjMat, texcoord, depth).xyz;
    const float3 V_view = normalize(-P_view);

    float4 P_world = mul(camera.invViewMat, float4(P_view, 1.0));
    P_world /= P_world.w;

    float3 tangent;
    float3 bitangent;
    ComputeOrthonormalBasis(N_view, tangent, bitangent);

    const uint numRaySamples = 5; // local (per dispatch) sample count.
    const uint temporalSampleIndex = (world_shader_data.frame_counter % ssgiConstants.numSamples);
    const uint numSamplesTotal = ssgiConstants.numSamples * numRaySamples;

    float4 accum_radiance = (float4)0.0;

    // float phi = InterleavedGradientNoise(float2(coord));

    for (uint rayIndex = 0; rayIndex < numRaySamples; rayIndex++)
    {
        const uint sampleIndex = temporalSampleIndex * numRaySamples + rayIndex;
        uint ray_seed = InitRandomSeed(InitRandomSeed(
            coord.x + sampleIndex * 73856093,
            coord.y + sampleIndex * 19349663), sampleIndex * 83492791);

        // float2 xi = float2(RandomFloat(ray_seed), RandomFloat(ray_seed));
        // float3 sample_dir_world = SampleCosineDir(xi, N_world);

        const float2 rnd = float2(
            SampleBlueNoise(int(coord.x), int(coord.y), sampleIndex * 2, numSamplesTotal * 2),
            SampleBlueNoise(int(coord.x), int(coord.y), sampleIndex * 2 + 1, numSamplesTotal * 2));

        const float3 d = SampleCosineWeightedHemisphere(rnd);

        const float3 ray_direction = normalize(tangent * d.x + bitangent * d.y + N_view * d.z);
        const float3 ray_origin = P_view + ray_direction * RAY_OFFSET;

        float4 radiance = (float4)0.0;
        float3 beta = (float3)1.0;

        float3 local_origin_view = P_view;
        float3 local_direction_view = ray_direction;
        float3 local_N_view = N_view;

        for (int bounce_index = 0; bounce_index < NUM_BOUNCES; bounce_index++)
        {
            float3 ray_origin = local_origin_view + local_direction_view * RAY_OFFSET;

            float2 hit_uv;
            float4 hit_view_space_position;
            float hit_depth;
            float num_march_iterations;

            bool hit = TraceScreenSpaceRay(
                ray_origin,
                local_direction_view,
                hit_uv,
                hit_view_space_position,
                hit_depth,
                num_march_iterations);

            if (hit_depth >= 0.9999)
            {
                // miss, sample environment probe
                // float3 world_dir = normalize(mul(camera.invViewMat, float4(local_direction_view, 0.0)).xyz);
                // radiance += float4(beta * SampleSky(world_dir).rgb, 1.0);
                break;
            }

            float alpha = CalculateHitAlpha(num_march_iterations, hit_uv);

            if (alpha < HYP_FMATH_EPSILON)
            {
                // float3 world_dir = normalize(mul(camera.invViewMat, float4(local_direction_view, 0.0)).xyz);
                // radiance += float4(beta * SampleSky(world_dir).rgb, 1.0);
                break;
            }

            float2 sample_uv = saturate(hit_uv);

            float4 hit_albedo = SAMPLE_TEXTURE_2D(sampler_linear, gbuffer_albedo_texture, sample_uv);
            float4 hit_normal_sample = SAMPLE_TEXTURE_2D(sampler_nearest, gbuffer_normals_texture, sample_uv);

            float3 hit_N_world = GBufferUnpackNormal(hit_normal_sample);

            GBufferMaterialParams hit_material_params;
            GBufferUnpackMaterialParams(hit_normal_sample.x, 0, hit_material_params);

            float hit_roughness = hit_material_params.roughness;
            float hit_metalness = hit_material_params.metalness;

            float4 hit_pos_world = mul(camera.invViewMat, hit_view_space_position);
            hit_pos_world /= hit_pos_world.w;

            float3 hit_V_world = normalize(P_world.xyz - hit_pos_world.xyz);

            for (uint lightIdx = 0; lightIdx < ssgiConstants.numBoundLights; lightIdx++)
            {
                radiance.rgb += beta * CalculateDirectLighting(
                    lightIdx,
                    hit_albedo.rgb,
                    hit_pos_world.xyz,
                    hit_N_world,
                    hit_metalness
                );
                radiance.w += 1.0;
            }

            // RR
            if (bounce_index >= 1)
            {
                float p = clamp(max(max(beta.r, beta.g), beta.b), 0.05, 0.99);

                if (RandomFloat(ray_seed) > p)
                {
                    break;
                }

                beta /= p;
            }
            
            float3 hit_N_view = normalize(mul(camera.view, float4(hit_N_world, 0.0)).xyz);
            float3 hit_diffuse_color = hit_albedo.rgb * (1.0 - hit_metalness);

            // scatter direction
            float2 bounce_rnd = float2(RandomFloat(ray_seed), RandomFloat(ray_seed));
            float3 next_dir_view = SampleCosineDir(bounce_rnd, hit_N_view);

            local_N_view = hit_N_view;
            local_direction_view = next_dir_view;

            beta *= hit_diffuse_color;

            local_origin_view = hit_view_space_position.xyz;
        } // end bounces

        accum_radiance += radiance;
    } // end samples

    accum_radiance /= float(numRaySamples); // Note that we do not divide by numSamples here, that is handled by temporal blending.

    out_image[coord] = accum_radiance;
}

#endif