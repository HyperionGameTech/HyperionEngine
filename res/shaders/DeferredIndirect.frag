#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "include/defines.inc"

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec2 v_texcoord0;

layout(location = 0) out vec4 output_color;
layout(location = 1) out vec4 output_normals;
layout(location = 2) out vec4 output_positions;

// clang-format off

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#ifdef HYP_FEATURES_DYNAMIC_DESCRIPTOR_INDEXING
HYP_DESCRIPTOR_SRV(View, GBufferTextures, count = 7) uniform texture2D gbuffer_textures[NUM_GBUFFER_TEXTURES];
#else
HYP_DESCRIPTOR_SRV(View, GBufferAlbedoTexture) uniform texture2D gbuffer_albedo_texture;
HYP_DESCRIPTOR_SRV(View, GBufferNormalsTexture) uniform texture2D gbuffer_normals_texture;
HYP_DESCRIPTOR_SRV(View, GBufferMaterialTexture) uniform utexture2D gbuffer_material_texture;
HYP_DESCRIPTOR_SRV(View, GBufferVelocityTexture) uniform texture2D gbuffer_velocity_texture;
HYP_DESCRIPTOR_SRV(View, GBufferLightmapTexture) uniform texture2D gbuffer_albedo_lightmap_texture;
HYP_DESCRIPTOR_SRV(View, GBufferWSNormalsTexture) uniform texture2D gbuffer_ws_normals_texture;
HYP_DESCRIPTOR_SRV(View, GBufferTranslucentTexture) uniform texture2D gbuffer_albedo_texture_translucent;
#endif

HYP_DESCRIPTOR_SRV(View, GBufferMipChain) uniform texture2D gbuffer_mip_chain;
HYP_DESCRIPTOR_SRV(View, GBufferDepthTexture) uniform texture2D gbuffer_depth_texture;
HYP_DESCRIPTOR_SAMPLER(Global, SamplerNearest) uniform sampler sampler_nearest;
HYP_DESCRIPTOR_SAMPLER(Global, SamplerLinear) uniform sampler sampler_linear;

HYP_DESCRIPTOR_SRV(View, SSAOResultTexture) uniform texture2D ssao_gi_result;
HYP_DESCRIPTOR_SRV(View, SSGIResultTexture) uniform texture2D ssgi_result;
HYP_DESCRIPTOR_SRV(Global, RTRadianceResultTexture) uniform texture2D rt_radiance_final;
HYP_DESCRIPTOR_SRV(View, EnvGridRadianceResultTexture) uniform texture2D env_grid_radiance_texture;
HYP_DESCRIPTOR_SRV(View, EnvGridIrradianceResultTexture) uniform texture2D env_grid_irradiance_texture;
HYP_DESCRIPTOR_SRV(View, ReflectionProbeResultTexture) uniform texture2D reflections_texture;

#include "include/env_probe.inc"
HYP_DESCRIPTOR_SRV(Global, EnvProbeTextures, count = 16) uniform texture2D env_probe_textures[16];
HYP_DESCRIPTOR_SSBO(Global, EnvProbesBuffer) readonly buffer EnvProbesBuffer
{
    EnvProbe env_probes[];
};
HYP_DESCRIPTOR_CBUFF_DYNAMIC(Global, EnvGridsBuffer) uniform EnvGridsBuffer
{
    EnvGrid env_grid;
};
HYP_DESCRIPTOR_SSBO_DYNAMIC(Global, CurrentEnvProbe) readonly buffer CurrentEnvProbe
{
    EnvProbe current_env_probe;
};

HYP_DESCRIPTOR_SRV(Global, LightFieldColorTexture) uniform texture2D light_field_color_texture;
HYP_DESCRIPTOR_SRV(Global, LightFieldDepthTexture) uniform texture2D light_field_depth_texture;

#include "include/gbuffer.inc"
#include "include/material.inc"

#include "include/scene.inc"
HYP_DESCRIPTOR_CBUFF_DYNAMIC(Global, CamerasBuffer) uniform CamerasBuffer
{
    Camera camera;
};

HYP_DESCRIPTOR_CBUFF(Global, WorldsBuffer) uniform WorldsBuffer
{
    WorldShaderData world_shader_data;
};

#include "include/PhysicalCamera.inc"

#define HYP_DEFERRED_NO_REFRACTION
#include "./deferred/DeferredLighting.glsl"
#undef HYP_DEFERRED_NO_REFRACTION

vec2 texcoord = v_texcoord0;

#define HYP_VCT_REFLECTIONS_ENABLED 1
#define HYP_VCT_INDIRECT_ENABLED 1

#include "include/rt/probe/probe_uniforms.inc"

HYP_DESCRIPTOR_CBUFF(Global, DDGIUniforms, size = 256) uniform DDGIUniformBuffer
{
    DDGIUniforms ddgiUniforms;
};

HYP_DESCRIPTOR_SRV(Global, DDGIIrradianceTexture) uniform texture2D probe_irradiance;
HYP_DESCRIPTOR_SRV(Global, DDGIDepthTexture) uniform texture2D probe_depth;
#include "include/DDGI.inc"

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

// clang-format on

layout(push_constant) uniform PushConstant
{
    DeferredParams deferred_params;
};

#define DDGI_MULTIPLIER 1.0

void main()
{
    vec4 albedo = Texture2D(sampler_nearest, gbuffer_albedo_texture, texcoord);
    vec3 normal = GBufferUnpackNormal(Texture2D(sampler_nearest, gbuffer_normals_texture, texcoord).xy);
    vec3 ws_normal = DecodeNormal(Texture2D(sampler_nearest, gbuffer_ws_normals_texture, texcoord));

    float depth = Texture2D(sampler_nearest, gbuffer_depth_texture, texcoord).r;
    vec4 position = ReconstructWorldSpacePositionFromDepth(inverse(camera.projection), inverse(camera.view), texcoord, depth);

    uvec2 materialData = texture(usampler2D(gbuffer_material_texture, HYP_SAMPLER_NEAREST), texcoord).rg;

    GBufferMaterialParams materialParams;
    GBufferUnpackMaterialParams(materialData, materialParams);

    const float roughness = materialParams.roughness;
    const float metalness = materialParams.metalness;
    const uint mask = materialParams.mask;

    vec3 result = vec3(0.0);

    vec3 N = normalize(normal);
    vec3 V = normalize(camera.position.xyz - position.xyz);
    vec3 R = normalize(reflect(-V, N));

    float ao = 1.0;
    vec3 irradiance = vec3(0.0);
    vec4 reflections = vec4(0.0);
    vec3 ibl = vec3(0.0);

    const vec4 ssao_data = Texture2D(HYP_SAMPLER_NEAREST, ssao_gi_result, v_texcoord0);
    ao = min(mix(1.0, ssao_data.a, bool(deferred_params.flags & DEFERRED_FLAGS_HBAO_ENABLED)), materialParams.ao);

    const vec3 diffuse_color = CalculateDiffuseColor(albedo.rgb, metalness);

    const float perceptual_roughness = sqrt(roughness);

    reflections = Texture2D(HYP_SAMPLER_LINEAR, reflections_texture, texcoord);

#ifdef ENV_GRID_REFLECTIONS
    vec4 env_grid_radiance = Texture2D(HYP_SAMPLER_LINEAR, env_grid_radiance_texture, texcoord);
    reflections = reflections * (1.0 - env_grid_radiance.a) + (vec4(env_grid_radiance.rgb, 1.0) * env_grid_radiance.a);
#endif

#ifdef ENV_GRID_GI
    irradiance += Texture2D(HYP_SAMPLER_LINEAR, env_grid_irradiance_texture, texcoord).rgb * ENV_GRID_MULTIPLIER;
#endif

    // SH9 sh9;
    // for (int i = 0; i < 9; i++) {
    //     sh9.values[i] = current_env_probe.sh[i].rgb;
    // }
    // irradiance += SphericalHarmonicsSample(sh9, ws_normal) * ENV_GRID_MULTIPLIER;

#ifdef SSGI_ENABLED
    const vec4 ssgi = Texture2D(HYP_SAMPLER_LINEAR, ssgi_result, v_texcoord0);
    irradiance = irradiance * (1.0 - ssgi.a) + (ssgi.rgb * ssgi.a);
#endif

#ifdef RT_REFLECTIONS
    CalculateRaytracingReflection(deferred_params, texcoord, reflections);
#endif

#ifdef RT_GI
    irradiance += DDGISampleIrradiance(position.xyz, ws_normal, V).rgb * DDGI_MULTIPLIER;
#endif

#ifdef HBIL_ENABLED
    CalculateHBILIrradiance(deferred_params, ssao_data, irradiance);
#endif

    const float NdotV = max(0.0001, dot(N, V));
    const vec3 F0 = CalculateF0(albedo.rgb, metalness);
    const vec3 F = CalculateFresnelTerm(F0, roughness, NdotV);
    const vec3 dfg = CalculateDFG(F, roughness, NdotV);
    const vec3 E = CalculateE(F0, dfg);
    vec3 Fd = diffuse_color.rgb * irradiance * (1.0 - E) * ao;

    vec3 specular_ao = vec3(SpecularAO_Lagarde(NdotV, ao, roughness));

    const vec3 energy_compensation = CalculateEnergyCompensation(F0, dfg);
    specular_ao *= energy_compensation;

    vec3 Fr = ibl * E * specular_ao;

    reflections.rgb *= specular_ao;
    Fr = Fr * (1.0 - reflections.a) + (E * reflections.rgb);

    result = Fd + Fr;

#ifdef PATHTRACER
    result = CalculatePathTracing(deferred_params, texcoord).rgb;
#elif defined(DEBUG_REFLECTIONS)
    result = E * reflections.rgb;
#elif defined(DEBUG_IRRADIANCE)
    result = irradiance.rgb;
#endif

    output_color = vec4(result, 1.0);
}