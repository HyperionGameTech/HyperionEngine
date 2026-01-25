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

HYP_DESCRIPTOR_SRV(DeferredPass, GBufferAlbedoTexture) uniform texture2D gbuffer_albedo_texture;
HYP_DESCRIPTOR_SRV(DeferredPass, GBufferNormalsTexture) uniform texture2D gbuffer_normals_texture;
HYP_DESCRIPTOR_SRV(DeferredPass, GBufferMaterialTexture) uniform utexture2D gbuffer_material_texture;
HYP_DESCRIPTOR_SRV(DeferredPass, GBufferVelocityTexture) uniform texture2D gbuffer_velocity_texture;

HYP_DESCRIPTOR_SRV(DeferredPass, GBufferMipChain) uniform texture2D gbuffer_mip_chain;
HYP_DESCRIPTOR_SRV(DeferredPass, GBufferDepthTexture) uniform texture2D gbuffer_depth_texture;
HYP_DESCRIPTOR_SAMPLER(DeferredPass, SamplerNearest) uniform sampler sampler_nearest;
HYP_DESCRIPTOR_SAMPLER(DeferredPass, SamplerLinear) uniform sampler sampler_linear;

HYP_DESCRIPTOR_SRV(DeferredPass, SSAOResultTexture) uniform texture2D ssao_gi_result;
HYP_DESCRIPTOR_SRV(DeferredPass, SSGIResultTexture) uniform texture2D ssgi_result;
HYP_DESCRIPTOR_SRV(DeferredPass, RTRadianceResultTexture) uniform texture2D rt_radiance_final;
HYP_DESCRIPTOR_SRV(DeferredPass, ReflectionProbeResultTexture) uniform texture2D reflections_texture;

#include "include/gbuffer.inc"
#include "include/material.inc"

#include "include/scene.inc"
HYP_DESCRIPTOR_BUFFER_DYNAMIC(DeferredPass, CamerasBuffer) uniform CamerasBuffer
{
    Camera camera;
};

HYP_DESCRIPTOR_BUFFER(DeferredPass, WorldsBuffer) uniform WorldsBuffer
{
    WorldShaderData world_shader_data;
};

#include "include/PhysicalCamera.inc"

vec2 texcoord = v_texcoord0;

#define HYP_VCT_REFLECTIONS_ENABLED 1
#define HYP_VCT_INDIRECT_ENABLED 1

#include "include/rt/probe/probe_uniforms.inc"

#if RT_GI
HYP_DESCRIPTOR_BUFFER(DeferredPass, DDGIConstants) uniform DDGI
{
    DDGIConstants ddgiConstants;
};

HYP_DESCRIPTOR_SRV(DeferredPass, DDGIIrradianceTexture) uniform texture2D probe_irradiance;
HYP_DESCRIPTOR_SRV(DeferredPass, DDGIDepthTexture) uniform texture2D probe_depth;
#include "include/DDGI.inc"

#endif

#include "include/env_probe.inc"
HYP_DESCRIPTOR_BUFFER_DYNAMIC(DeferredPass, EnvGridsBuffer) uniform EnvGridsBuffer
{
    EnvGrid env_grid;
};

#define HYP_DEFERRED_NO_REFRACTION
#define HYP_DEFERRED_NO_ENV_PROBE
#include "./deferred/DeferredLighting.glsl"
#undef HYP_DEFERRED_NO_REFRACTION
#undef HYP_DEFERRED_NO_ENV_PROBE

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

// clang-format on

#define DDGI_MULTIPLIER 1.0

void main()
{
    vec4 albedo = SAMPLE_TEXTURE_2D(sampler_nearest, gbuffer_albedo_texture, texcoord);
    vec4 normalSample = SAMPLE_TEXTURE_2D(sampler_nearest, gbuffer_normals_texture, texcoord);
    vec3 normal = GBufferUnpackNormal(normalSample);

    float depth = SAMPLE_TEXTURE_2D(sampler_nearest, gbuffer_depth_texture, texcoord).r;
    vec4 position = ReconstructWorldSpacePositionFromDepth(inverse(camera.projection), inverse(camera.view), texcoord, depth);

    uvec2 materialData = texture(usampler2D(gbuffer_material_texture, HYP_SAMPLER_NEAREST), texcoord).rg;

    GBufferMaterialParams materialParams;
    GBufferUnpackMaterialParams(normalSample.x, materialData.x, materialParams);

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

#if HBAO_ENABLED
    const vec4 ssao_data = SAMPLE_TEXTURE_2D(HYP_SAMPLER_NEAREST, ssao_gi_result, v_texcoord0);
    ao = ssao_data.a;
#endif

    const vec3 diffuse_color = CalculateDiffuseColor(albedo.rgb, metalness);

    const float perceptual_roughness = sqrt(roughness);

    reflections = SAMPLE_TEXTURE_2D(HYP_SAMPLER_LINEAR, reflections_texture, texcoord);

#if SSGI_ENABLED
    const vec4 ssgi = SAMPLE_TEXTURE_2D(HYP_SAMPLER_LINEAR, ssgi_result, v_texcoord0);
    irradiance = irradiance * (1.0 - ssgi.a) + (ssgi.rgb * ssgi.a);
#endif

#if RT_REFLECTIONS
    CalculateRayTracingReflection(texcoord, reflections);
#endif

#if RT_GI
    irradiance += DDGISampleIrradiance(position.xyz, normal, V).rgb * DDGI_MULTIPLIER;
#endif

#if HBIL_ENABLED
    CalculateHBILIrradiance(ssao_data, irradiance);
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
    result = CalculatePathTracing(texcoord).rgb;
#elif defined(DEBUG_REFLECTIONS)
    result = E * reflections.rgb;
#elif defined(DEBUG_IRRADIANCE)
    result = irradiance.rgb;
#elif defined(DEBUG_VELOCITY)
    vec4 velocity = SAMPLE_TEXTURE_2D(sampler_nearest, gbuffer_velocity_texture, texcoord);
    result = velocity.rgb;
#endif

    output_color = vec4(result, 1.0);
}