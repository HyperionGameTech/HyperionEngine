#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec2 v_texcoord;

layout(location = 0) out vec4 color_output;

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../include/defines.inc"
#include "../include/shared.inc"
#include "../include/gbuffer.inc"
#include "../include/scene.inc"
#include "../include/brdf.inc"
#include "../include/noise.inc"

HYP_DESCRIPTOR_CBUFF_DYNAMIC(Global, CamerasBuffer) uniform CamerasBuffer
{
    Camera camera;
};

HYP_DESCRIPTOR_CBUFF(Global, WorldsBuffer) uniform WorldsBuffer
{
    WorldShaderData world_shader_data;
};

HYP_DESCRIPTOR_SSBO(Global, BlueNoiseBuffer) readonly buffer BlueNoiseBuffer
{
    ivec4 sobol_256spp_256d[256 * 256 / 4];
    ivec4 scrambling_tile[128 * 128 * 8 / 4];
    ivec4 ranking_tile[128 * 128 * 8 / 4];
};

HYP_DESCRIPTOR_CBUFF(Global, SphereSamplesBuffer) uniform SphereSamplesBuffer
{
    vec4 sphere_samples[4096];
};

#ifdef HYP_FEATURES_DYNAMIC_DESCRIPTOR_INDEXING
HYP_DESCRIPTOR_SRV(View, GBufferTextures) uniform texture2D gbuffer_textures[NUM_GBUFFER_TEXTURES];
#else
HYP_DESCRIPTOR_SRV(View, GBufferAlbedoTexture) uniform texture2D gbuffer_albedo_texture;
HYP_DESCRIPTOR_SRV(View, GBufferNormalsTexture) uniform texture2D gbuffer_normals_texture;
HYP_DESCRIPTOR_SRV(View, GBufferMaterialTexture) uniform utexture2D gbuffer_material_texture;
HYP_DESCRIPTOR_SRV(View, GBufferVelocityTexture) uniform texture2D gbuffer_velocity_texture;
HYP_DESCRIPTOR_SRV(View, GBufferWSNormalsTexture) uniform texture2D gbuffer_ws_normals_texture;
#endif

HYP_DESCRIPTOR_SRV(View, GBufferMipChain) uniform texture2D gbuffer_mip_chain;
HYP_DESCRIPTOR_SRV(View, GBufferDepthTexture) uniform texture2D gbuffer_depth_texture;
HYP_DESCRIPTOR_SAMPLER(Global, SamplerNearest) uniform sampler sampler_nearest;
HYP_DESCRIPTOR_SAMPLER(Global, SamplerLinear) uniform sampler sampler_linear;

#define HYP_DEFERRED_NO_RT_RADIANCE
#define HYP_DEFERRED_NO_ENV_GRID

#include "../include/env_probe.inc"
HYP_DESCRIPTOR_SRV(Global, EnvProbeTextures, count = 16) uniform texture2D env_probe_textures[16];
HYP_DESCRIPTOR_SSBO(Global, EnvProbesBuffer) readonly buffer EnvProbesBuffer
{
    EnvProbe env_probes[];
};
HYP_DESCRIPTOR_SSBO_DYNAMIC(Global, CurrentEnvProbe) readonly buffer CurrentEnvProbe
{
    EnvProbe current_env_probe;
};

#include "./DeferredLighting.glsl"
#include "../include/BlueNoise.glsl"

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

layout(push_constant) uniform PushConstant
{
    DeferredParams deferred_params;
};

#define SAMPLE_COUNT 4

void main()
{
    uvec2 screen_resolution = uvec2(deferred_params.screen_width, deferred_params.screen_height);
    vec2 pixel_size = 1.0 / vec2(screen_resolution);
    // vec2 texcoord = min(v_texcoord + (pixel_size * float(world_shader_data.frame_counter & 1)), vec2(1.0));
    vec2 texcoord = v_texcoord;
    uvec2 pixel_coord = uvec2(texcoord * vec2(screen_resolution) - 0.5);

    const float depth = Texture2DLod(sampler_nearest, gbuffer_depth_texture, texcoord, 0.0).r;
    const vec3 N =  GBufferUnpackNormal(Texture2D(sampler_nearest, gbuffer_normals_texture, texcoord).xy);
    const vec3 P = ReconstructWorldSpacePositionFromDepth(inverse(camera.projection), inverse(camera.view), texcoord, depth).xyz;
    const vec3 V = normalize(camera.position.xyz - P);
    const vec3 R = normalize(reflect(-V, N));

    uvec2 materialData = texture(usampler2D(gbuffer_material_texture, HYP_SAMPLER_NEAREST), texcoord).rg;

    GBufferMaterialParams materialParams;
    GBufferUnpackMaterialParams(materialData, materialParams);

    float roughness = materialParams.roughness;

    const float lod = HYP_FMATH_SQR(sqrt(roughness)) * 12.0;

    vec4 ibl = vec4(0.0);

    const uint probe_texture_index = max(0, min(current_env_probe.texture_index, HYP_MAX_BOUND_REFLECTION_PROBES - 1));

    vec3 tangent;
    vec3 bitangent;
    ComputeOrthonormalBasis(N, tangent, bitangent);

#if 0
    float phi = InterleavedGradientNoise(vec2(pixel_coord));

    for (int i = 0; i < SAMPLE_COUNT; i++)
    {
        vec2 rnd = VogelDisk(i, SAMPLE_COUNT, phi);

        vec3 H = ImportanceSampleGGX(rnd, N, roughness);
        H = tangent * H.x + bitangent * H.y + N * H.z;

        const vec3 dir = normalize(2.0 * dot(V, H) * H - V);

        vec4 sample_ibl = vec4(0.0);

        ApplyReflectionProbe(
            current_env_probe.texture_index,
            current_env_probe.world_position.xyz,
            current_env_probe.aabb_min.xyz,
            current_env_probe.aabb_max.xyz,
            P,
            dir,
            lod,
            sample_ibl);

        ibl += sample_ibl;
    }
#else

    // vec2 blue_noise_sample = vec2(
    //     SampleBlueNoise(int(pixel_coord.x), int(pixel_coord.y), 0, 0),
    //     SampleBlueNoise(int(pixel_coord.x), int(pixel_coord.y), 0, 1)
    // );

    // vec2 blue_noise_scaled = blue_noise_sample + float(world_shader_data.frame_counter % 256) * 1.618;
    // const vec2 rnd = fmod(blue_noise_scaled, vec2(1.0));

    for (int i = 0; i < SAMPLE_COUNT; i++)
    {
        vec2 rnd = Hammersley(uint(i), uint(SAMPLE_COUNT));

        vec3 H = ImportanceSampleGGX(rnd, N, roughness);
        H = tangent * H.x + bitangent * H.y + N * H.z;

        const vec3 dir = normalize(2.0 * dot(V, H) * H - V);

        vec4 sample_ibl = vec4(0.0);

        ApplyReflectionProbe(
            current_env_probe.texture_index,
            current_env_probe.world_position.xyz,
            current_env_probe.aabb_min.xyz,
            current_env_probe.aabb_max.xyz,
            P,
            dir,
            lod,
            sample_ibl);

        ibl += sample_ibl;
    }
#endif

    color_output = ibl * (1.0 / float(SAMPLE_COUNT));
}