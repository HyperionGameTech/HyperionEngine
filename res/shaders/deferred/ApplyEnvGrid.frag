#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : require

#include "../include/defines.inc"

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec2 v_texcoord;

layout(location = 0) out vec4 color_output;

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

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "../include/scene.inc"

HYP_DESCRIPTOR_CBUFF_DYNAMIC(Global, CamerasBuffer) uniform CamerasBuffer
{
    Camera camera;
};

HYP_DESCRIPTOR_CBUFF(Global, WorldsBuffer) uniform WorldsBuffer
{
    WorldShaderData world_shader_data;
};

#include "../include/shared.inc"
#include "../include/gbuffer.inc"
#include "../include/brdf.inc"
#include "../include/aabb.inc"

#define HYP_DEFERRED_NO_RT_RADIANCE // temp

#include "../include/env_probe.inc"
HYP_DESCRIPTOR_CBUFF_DYNAMIC(Global, EnvGridsBuffer) uniform EnvGridsBuffer
{
    EnvGrid env_grid;
};
HYP_DESCRIPTOR_SRV(Global, EnvProbeTextures, count = 16) uniform texture2D env_probe_textures[16];
HYP_DESCRIPTOR_SSBO(Global, EnvProbesBuffer) readonly buffer EnvProbesBuffer
{
    EnvProbe env_probes[];
};

HYP_DESCRIPTOR_SRV(Global, LightFieldColorTexture) uniform texture2D light_field_color_texture;
HYP_DESCRIPTOR_SRV(Global, LightFieldDepthTexture) uniform texture2D light_field_depth_texture;

#include "./DeferredLighting.glsl"

#if defined(MODE_RADIANCE) || defined(IRRADIANCE_MODE_VOXEL)
HYP_DESCRIPTOR_SSBO(Global, BlueNoiseBuffer) readonly buffer BlueNoiseBuffer
{
    ivec4 sobol_256spp_256d[256 * 256 / 4];
    ivec4 scrambling_tile[128 * 128 * 8 / 4];
    ivec4 ranking_tile[128 * 128 * 8 / 4];
};

HYP_DESCRIPTOR_SRV(Global, VoxelGridTexture) uniform texture3D voxel_image;

#include "./EnvGridVoxelConeTracing.glsl"
#endif

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

layout(push_constant) uniform PushConstant
{
    DeferredParams deferred_params;
};

void main()
{
    uvec2 screen_resolution = uvec2(deferred_params.screen_width, deferred_params.screen_height);
    vec2 pixel_size = 1.0 / vec2(screen_resolution);
    vec2 texcoord = v_texcoord;
    // vec2 texcoord = min(v_texcoord + (pixel_size * float(world_shader_data.frame_counter & 1)), vec2(1.0));
    uvec2 pixel_coord = uvec2(texcoord * vec2(screen_resolution) - 0.5);

    vec3 irradiance = vec3(0.0);

    const mat4 inverse_proj = inverse(camera.projection);
    const mat4 inverse_view = inverse(camera.view);

    const float depth = Texture2D(sampler_nearest, gbuffer_depth_texture, texcoord).r;
    const vec3 N = DecodeNormal(Texture2D(sampler_nearest, gbuffer_ws_normals_texture, texcoord));
    const vec3 P = ReconstructWorldSpacePositionFromDepth(inverse_proj, inverse_view, texcoord, depth).xyz;
    const vec3 V = normalize(camera.position.xyz - P);

#if defined(MODE_RADIANCE) || defined(IRRADIANCE_MODE_VOXEL)
    uvec2 materialData = texture(usampler2D(gbuffer_material_texture, HYP_SAMPLER_NEAREST), texcoord).rg;

    GBufferMaterialParams materialParams;
    GBufferUnpackMaterialParams(materialData, materialParams);

    float roughness = materialParams.roughness;

    AABB voxel_grid_aabb;
    voxel_grid_aabb.min = env_grid.voxel_grid_aabb_min.xyz;
    voxel_grid_aabb.max = env_grid.voxel_grid_aabb_max.xyz;
#endif

#if defined(MODE_IRRADIANCE)
#if defined(IRRADIANCE_MODE_VOXEL)
    irradiance = ComputeVoxelIrradiance(P, N, pixel_coord, screen_resolution, world_shader_data.frame_counter, ivec3(env_grid.density.xyz), voxel_grid_aabb).rgb;
#else
    irradiance = CalculateEnvGridIrradiance(P, N, V);
#endif

    color_output = vec4(irradiance, 1.0);
#elif defined(MODE_RADIANCE)
    vec4 radiance = ComputeVoxelRadiance(P, N, V, roughness, pixel_coord, screen_resolution, world_shader_data.frame_counter, ivec3(env_grid.density.xyz), voxel_grid_aabb);

    color_output = radiance;
#endif
}
