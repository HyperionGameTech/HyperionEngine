#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : require

#include "../include/defines.inc"

layout(location = 1) in vec2 texcoord;
layout(location = 0) out vec4 color_output;

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

DECLARE_SRV(LightmapPass, GBufferAlbedoTexture) uniform texture2D gbuffer_albedo_texture;
DECLARE_SRV(LightmapPass, GBufferNormalsTexture) uniform texture2D gbuffer_normals_texture;
DECLARE_SRV(LightmapPass, GBufferMaterialTexture) uniform utexture2D gbuffer_material_texture;
DECLARE_SRV(LightmapPass, GBufferVelocityTexture) uniform texture2D gbuffer_velocity_texture;
DECLARE_SRV(LightmapPass, GBufferMipChain) uniform texture2D gbuffer_mip_chain;
DECLARE_SRV(LightmapPass, GBufferDepthTexture) uniform texture2D gbuffer_depth_texture;

DECLARE_SAMPLER(LightmapPass, SamplerNearest) uniform sampler sampler_nearest;
DECLARE_SAMPLER(LightmapPass, SamplerLinear) uniform sampler sampler_linear;

DECLARE_SRV(LightmapPass, RTRadianceResultTexture) uniform texture2D rt_radiance_final;

DECLARE_SRV(LightmapPass, SSGIResultTexture) uniform texture2D ssgi_result;
DECLARE_SRV(LightmapPass, SSAOResultTexture) uniform texture2D ssao_gi;

DECLARE_SRV(LightmapPass, ReflectionProbeResultTexture) uniform texture2D ReflectionProbeResultTexture;

#include "../include/shared.inc"
#include "../include/gbuffer.inc"
#include "../include/Entity.inc"
#include "../include/scene.inc"

DECLARE_BUFFER_DYNAMIC(LightmapPass, CamerasBuffer) uniform CamerasBuffer
{
    Camera camera;
};

DECLARE_BUFFER(LightmapPass, WorldsBuffer) uniform WorldsBuffer
{
    WorldShaderData world_shader_data;
};

#include "../include/brdf.inc"

DECLARE_SRV(LightmapPass, ShadowMapsTextureArray) uniform texture2DArray shadow_maps;
DECLARE_SRV(LightmapPass, PointLightShadowMapsTextureArray) uniform textureCubeArray point_shadow_maps;

#include "../include/shadows.inc"

DECLARE_SRV(LightmapPass, IrradianceTexture) uniform texture2D IrradianceTexture;
DECLARE_SRV(LightmapPass, RadianceTexture) uniform texture2D RadianceTexture;
DECLARE_SAMPLER(LightmapPass, LightmapSampler) uniform sampler LightmapSampler;

DECLARE_BUFFER(LightmapPass, LightmapVolumeUniforms) uniform LightmapVolumeUniforms
{
    float irradianceWeight;
    float radianceWeight;

    uint numAtlases;
};

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "../include/env_probe.inc"

#if ENV_PROBE_CUBEMAP
DECLARE_SRV(LightmapPass, EnvProbesTexture) uniform textureCubeArray envProbesTexture;
#else
DECLARE_SRV(LightmapPass, EnvProbesTexture) uniform texture2DArray envProbesTexture;
#endif

DECLARE_SRV(LightmapPass, EnvProbesBuffer) readonly buffer EnvProbesBuffer { EnvProbe env_probes[]; };

DECLARE_SRV_DYNAMIC(LightmapPass, CurrentEnvProbe) readonly buffer CurrentEnvProbe
{
    EnvProbe current_env_probe;
};

DECLARE_BUFFER_DYNAMIC(LightmapPass, EnvGridsBuffer) uniform EnvGridsBuffer
{
    EnvGrid env_grid;
};

#include "./DeferredLighting.inc"

void main()
{
    const vec4 albedo = SAMPLE_TEXTURE_2D(sampler_nearest, gbuffer_albedo_texture, texcoord);
    const vec4 normalSample = SAMPLE_TEXTURE_2D(sampler_nearest, gbuffer_normals_texture, texcoord);

    const uvec4 materialData = texture(usampler2D(gbuffer_material_texture, sampler_nearest), texcoord);

    GBufferMaterialParams materialParams;
    GBufferUnpackMaterialParams(normalSample.x, materialData.x, materialParams);

    const float roughness = materialParams.roughness;
    const float metalness = materialParams.metalness;
    const float ao = 1.0; // @TODO

    const mat4 inverse_proj = inverse(camera.projection);
    const mat4 inverse_view = inverse(camera.view);

    vec3 N = GBufferUnpackNormal(SAMPLE_TEXTURE_2D(sampler_nearest, gbuffer_normals_texture, texcoord));
    vec2 UV1 = vec2(uintBitsToFloat(materialData.z), uintBitsToFloat(materialData.w));

    const float depth = SAMPLE_TEXTURE_2D(sampler_nearest, gbuffer_depth_texture, texcoord).r;
    const vec3 P = ReconstructWorldSpacePositionFromDepth(inverse_proj, inverse_view, texcoord, depth).xyz;
    const vec3 V = normalize(camera.position.xyz - P);
    const vec3 R = normalize(reflect(-V, N));

    vec2 lightmapUV = UV1;

    // sample lightmap atlases based on weights
    vec4 irradiance = SAMPLE_TEXTURE_2D(LightmapSampler, IrradianceTexture, lightmapUV) * irradianceWeight;
    irradiance.a = 1.0;

    vec4 radiance = SAMPLE_TEXTURE_2D(LightmapSampler, RadianceTexture, lightmapUV) * radianceWeight;
    radiance.a = 1.0;

    vec3 ibl = vec3(0.0);
    vec3 F = vec3(0.0);

    float NdotV = max(0.0001, dot(N, V));

    const vec3 diffuse_color = CalculateDiffuseColor(albedo.rgb, metalness);
    const vec3 F0 = CalculateF0(albedo.rgb, metalness);

    F = CalculateFresnelTerm(F0, roughness, NdotV);
    const vec3 kD = (vec3(1.0) - F) * (1.0 - metalness);

    const vec3 dfg = CalculateDFG(F, roughness, NdotV);
    const vec3 E = CalculateE(F0, dfg);
    const vec3 energyCompensation = CalculateEnergyCompensation(F0, dfg);

    vec4 reflections = SAMPLE_TEXTURE_2D(sampler_nearest, ReflectionProbeResultTexture, texcoord);

    ibl = ibl * (1.0 - reflections.a) + (reflections.rgb * reflections.a);

    vec3 spec = (ibl * mix(dfg.xxx, dfg.yyy, F0)) * energyCompensation;

    color_output.rgb = (diffuse_color * irradiance.rgb) + (diffuse_color * radiance.rgb * ao) + spec;
    color_output.a = 1.0;
}