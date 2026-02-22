#include "../include/defines.inc"
#include "../include/noise.inc"

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

struct SSGIUniforms
{
    uint4 dimension;
    float ray_step;
    float num_iterations;
    float max_ray_distance;
    float distance_bias;
    float offset;
    float eye_fade_start;
    float eye_fade_end;
    float screen_edge_fade_start;
    float screen_edge_fade_end;

    uint num_bound_lights;
    uint3 _pad0;
    uint4 light_indices[4];
};

DECLARE_UAV(SSGI, OutImage) RWTexture2D<OUTPUT_UAV_TYPE> out_image;

DECLARE_BUFFER(SSGI, UniformBuffer) cbuffer UniformBuffer
{
    SSGIUniforms ssr_params;
};

DECLARE_SRV(SSGI, GBufferAlbedoTexture) Texture2D gbuffer_albedo_texture;
DECLARE_SRV(SSGI, GBufferNormalsTexture) Texture2D gbuffer_normals_texture;
DECLARE_SRV(SSGI, GBufferMaterialTexture) Texture2D<uint4> gbuffer_material_texture;
DECLARE_SRV(SSGI, GBufferVelocityTexture) Texture2D gbuffer_velocity_texture;

DECLARE_SRV(SSGI, GBufferDepthTexture) Texture2D gbuffer_depth_texture;
DECLARE_SRV(SSGI, GBufferMipChain) Texture2D gbuffer_mip_chain;

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

DECLARE_SRV(SSGI, LightsBuffer) StructuredBuffer<Light> lights;

DECLARE_SRV(SSGI, ShadowMapsTextureArray) Texture2DArray shadow_maps;
DECLARE_SRV(SSGI, PointLightShadowMapsTextureArray) TextureCubeArray point_shadow_maps;

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../include/scene.inc"
#include "../include/gbuffer.inc"
#include "../include/BlueNoise.inc"
#include "../include/shadows.inc"
#include "../include/Octahedron.inc"
#include "../include/env_probe.inc"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#if ENV_PROBE_CUBEMAP
DECLARE_SRV(SSGI, EnvProbesTexture) TextureCubeArray envProbesTexture;
#else
DECLARE_SRV(SSGI, EnvProbesTexture) Texture2DArray envProbesTexture;
#endif

DECLARE_SRV_DYNAMIC(SSGI, CurrentEnvProbe) StructuredBuffer<EnvProbe> current_env_probe_buffer;
DECLARE_SRV(SSGI, EnvProbesBuffer) StructuredBuffer<EnvProbe> env_probes_buffer;

#define NUM_RAYS 8
// #define EVAL_LIGHTING

bool TraceRays(
    float3 ray_origin,
    float3 ray_direction,
    out float2 hit_uv,
    out float4 hit_view_space_position,
    out float hit_depth,
    out float out_num_iterations)
{
    bool intersect = false;
    out_num_iterations = 0.0;
    hit_uv = float2(0.0, 0.0);
    hit_depth = 1.0;
    hit_view_space_position = float4(0.0, 0.0, 0.0, 0.0);

    float3 ray_step = ssr_params.ray_step * normalize(ray_direction);
    float3 marching_position = ray_origin;
    float step_delta = 0.0;

    int i = 0;

    for (; i < int(ssr_params.num_iterations); i++)
    {
        marching_position += ray_step;

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
        // binary search
        for (; i < int(ssr_params.num_iterations); i++)
        {
            ray_step *= 0.5;
            marching_position = marching_position - ray_step * sign(step_delta);

            hit_uv = GetProjectedPositionFromView(camera.projection, marching_position);
            hit_depth = SAMPLE_TEXTURE_2D(sampler_nearest, gbuffer_depth_texture, hit_uv).r;
            hit_view_space_position = ReconstructViewSpacePositionFromDepth(camera.invProjMat, hit_uv, hit_depth);

            step_delta = abs(marching_position.z) - hit_view_space_position.z;

            if (abs(step_delta) < ssr_params.distance_bias)
            {
                return true;
            }
        }
    }

    return false;
}

float CalculateAlpha(
    float num_iterations,
    float2 hit_uv,
    float3 ray_direction)
{
    float alpha = 1.0;

    // Fade ray hits that approach the maximum iterations
    alpha *= 1.0 - (num_iterations / ssr_params.num_iterations);

    // Fade ray hits that approach the screen edge
    float2 hit_uv_ndc = hit_uv * 2.0 - 1.0;
    float max_dimension = saturate(max(abs(hit_uv_ndc.x), abs(hit_uv_ndc.y)));
    alpha *= 1.0 - max(0.0, max_dimension - ssr_params.screen_edge_fade_start) / (1.0 - ssr_params.screen_edge_fade_end);

    return alpha;
}

float4 CalculateDirectLighting(uint light_index, float4 albedo, float3 P, float3 N)
{
    Light light = lights[light_index];

    if (light.type != HYP_LIGHT_TYPE_DIRECTIONAL)
    {
        return float4(0.0, 0.0, 0.0, 0.0);
    }

    const float4 light_color = light.color;

    float3 L = normalize(light.position_intensity.xyz);

    float NdotL = max(0.0001, dot(N, L));

    float shadow = 1.0;

    if (light.type == HYP_LIGHT_TYPE_DIRECTIONAL && ((light.flags & LF_SHADOW) != 0))
    {
        shadow = GetShadowStandard(light, P, float2(0.0, 0.0), NdotL);
    }

    return light_color * NdotL * shadow * light.position_intensity.w;
}

float4 SampleSky(float3 dir)
{
    EnvProbe current_env_probe = current_env_probe_buffer[0];

    if (current_env_probe.texture_index != ~0u)
    {
        uint probe_texture_index = clamp(current_env_probe.texture_index, 0u, uint(HYP_MAX_BOUND_REFLECTION_PROBES - 1));

        return EnvProbeSample(sampler_linear, envProbesTexture, probe_texture_index, dir, 0.0);
    }

    return float4(0.0, 0.0, 0.0, 0.0);
}

[numthreads(256, 1, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    const uint pixel_index = dispatchThreadID.x;
    const uint2 coord = uint2(
        pixel_index % ssr_params.dimension.x,
        pixel_index / ssr_params.dimension.x);

    if (any(coord >= ssr_params.dimension.xy))
    {
        return;
    }

    const float2 texcoord = saturate((float2(coord) + 0.5) / float2(ssr_params.dimension.xy));

    uint2 gbufferDimensions;
    gbuffer_material_texture.GetDimensions(gbufferDimensions.x, gbufferDimensions.y);

    uint2 pixelCoord = uint2(texcoord * max(0, int2(gbufferDimensions) - 1));

    const uint2 materialData = gbuffer_material_texture.Load(int3(pixelCoord, 0)).xy;
    const float4 normalSample = SAMPLE_TEXTURE_2D(sampler_nearest, gbuffer_normals_texture, texcoord);

    GBufferMaterialParams materialParams;
    GBufferUnpackMaterialParams(normalSample.x, materialData.x, materialParams);

    const float depth = SAMPLE_TEXTURE_2D(sampler_nearest, gbuffer_depth_texture, texcoord).r;

    const float3 N = GBufferUnpackNormal(normalSample);
    const float3 view_space_normal = normalize(mul(camera.view, float4(N, 0.0)).xyz);

    float4 accum_result = float4(0.0, 0.0, 0.0, 0.0);

    const float blue_noise_scale = float(world_shader_data.frame_counter % 16) * 1.618;

    if (depth > 0.99999)
    {
        out_image[coord] = float4(0.0, 0.0, 0.0, 0.0);
        return;
    }

    const float3 P = ReconstructViewSpacePositionFromDepth(camera.invProjMat, texcoord, depth).xyz;
    const float3 V = normalize(float3(0.0, 0.0, 0.0) - P);

    const float2 velocity = SAMPLE_TEXTURE_2D(sampler_linear, gbuffer_velocity_texture, texcoord).xy;

    float3 tangent;
    float3 bitangent;
    ComputeOrthonormalBasis(view_space_normal, tangent, bitangent);

    float2 hit_uv;
    float4 hit_view_space_position;
    float hit_depth;
    float num_iterations;

    float phi = InterleavedGradientNoise(float2(coord));

    for (int i = 0; i < NUM_RAYS; i++)
    {
        const float2 blue_noise_sample = float2(
            SampleBlueNoise(int(coord.x), int(coord.y), 0, i * 2),
            SampleBlueNoise(int(coord.x), int(coord.y), 0, i * 2 + 1));

        const float2 blue_noise_scaled = blue_noise_sample + blue_noise_scale;
        const float2 rnd = fmod(blue_noise_scaled, float2(1.0, 1.0));

        const float3 d = SampleCosineWeightedHemisphere(rnd);

        const float3 ray_direction = tangent * d.x + bitangent * d.y + view_space_normal * d.z;
        const float3 ray_origin = P + ray_direction * 0.25;

        const bool intersects = TraceRays(ray_origin, ray_direction, hit_uv, hit_view_space_position, hit_depth, num_iterations);

        if (intersects)
        {
            if (hit_depth < 1.0)
            {
                float alpha = CalculateAlpha(num_iterations, hit_uv, ray_direction);

                if (alpha > HYP_FMATH_EPSILON)
                {
                    float2 sample_uv = saturate(hit_uv);

#ifdef EVAL_LIGHTING
                    float4 hit_albedo = SAMPLE_TEXTURE_2D(sampler_linear, gbuffer_albedo_texture, sample_uv);

                    float3 hit_normal = GBufferUnpackNormal(SAMPLE_TEXTURE_2D(sampler_nearest, gbuffer_normals_texture, sample_uv));

                    float4 hit_position = mul(camera.invViewMat, hit_view_space_position);
                    hit_position /= hit_position.w;

                    float4 radiance = float4(0.0, 0.0, 0.0, 0.0);

                    for (uint j = 0; j < ssr_params.num_bound_lights; j++)
                    {
                        uint light_index = ssr_params.light_indices[j / 4][j % 4];

                        radiance += CalculateDirectLighting(light_index, hit_albedo, hit_position.xyz, hit_normal);
                    }

                    float4 gi = radiance * hit_albedo * alpha;
#else
                // @TODO lod slection?
                    float4 gi = SAMPLE_TEXTURE_2D_LOD(sampler_linear, gbuffer_mip_chain, sample_uv, 6.0);
                    gi *= alpha;
#endif

                    accum_result += gi;

                    continue;
                }
            }

            const float3 world_space_ray_direction = mul(camera.invViewMat, float4(ray_direction, 0.0)).xyz;
            accum_result += SampleSky(world_space_ray_direction);
        }
    }

    accum_result *= (1.0 / float(NUM_RAYS));

    out_image[coord] = accum_result;
}
