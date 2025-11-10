#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : require

#include "../include/defines.inc"

layout(location = 1) in vec2 texcoord;
layout(location = 0) out vec4 color_output;

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#ifdef HYP_FEATURES_DYNAMIC_DESCRIPTOR_INDEXING
HYP_DESCRIPTOR_SRV(View, GBufferTextures) uniform texture2D gbuffer_textures[NUM_GBUFFER_TEXTURES];
#else
HYP_DESCRIPTOR_SRV(View, GBufferAlbedoTexture) uniform texture2D gbuffer_albedo_texture;
HYP_DESCRIPTOR_SRV(View, GBufferNormalsTexture) uniform texture2D gbuffer_normals_texture;
HYP_DESCRIPTOR_SRV(View, GBufferMaterialTexture) uniform utexture2D gbuffer_material_texture;
HYP_DESCRIPTOR_SRV(View, GBufferVelocityTexture) uniform texture2D gbuffer_velocity_texture;
#endif

HYP_DESCRIPTOR_SRV(View, GBufferMipChain) uniform texture2D gbuffer_mip_chain;
HYP_DESCRIPTOR_SRV(View, GBufferDepthTexture) uniform texture2D gbuffer_depth_texture;
HYP_DESCRIPTOR_SAMPLER(Global, SamplerNearest) uniform sampler sampler_nearest;
HYP_DESCRIPTOR_SAMPLER(Global, SamplerLinear) uniform sampler sampler_linear;

HYP_DESCRIPTOR_SRV(Global, RTRadianceResultTexture) uniform texture2D rt_radiance_final;

HYP_DESCRIPTOR_SRV(View, SSGIResultTexture) uniform texture2D ssgi_result;
HYP_DESCRIPTOR_SRV(View, TAAResultTexture) uniform texture2D temporal_aa_result;
HYP_DESCRIPTOR_SRV(View, SSRResultTexture) uniform texture2D ssr_result;
HYP_DESCRIPTOR_SRV(View, SSAOResultTexture) uniform texture2D ssao_gi;
HYP_DESCRIPTOR_SRV(View, DeferredIndirectResultTexture) uniform texture2D deferred_indirect_lighting;

#include "../include/shared.inc"
#include "../include/gbuffer.inc"
#include "../include/Entity.glsl"
#include "../include/scene.inc"

HYP_DESCRIPTOR_CBUFF_DYNAMIC(Global, CamerasBuffer) uniform CamerasBuffer
{
    Camera camera;
};

HYP_DESCRIPTOR_CBUFF(Global, WorldsBuffer) uniform WorldsBuffer
{
    WorldShaderData world_shader_data;
};

HYP_DESCRIPTOR_SRV(Global, ShadowMapsTextureArray) uniform texture2DArray shadow_maps;
HYP_DESCRIPTOR_SRV(Global, PointLightShadowMapsTextureArray) uniform textureCubeArray point_shadow_maps;

#include "../include/brdf.inc"

#include "../include/env_probe.inc"
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
HYP_DESCRIPTOR_SRV(View, ReflectionProbeResultTexture) uniform texture2D reflections_texture;

HYP_DESCRIPTOR_SRV(Global, LightFieldColorTexture) uniform texture2D light_field_color_texture;
HYP_DESCRIPTOR_SRV(Global, LightFieldDepthTexture) uniform texture2D light_field_depth_texture;

#include "./DeferredLighting.glsl"
#include "../include/shadows.inc"

HYP_DESCRIPTOR_SRV(LightmapVolume, IrradianceTexture) uniform texture2D IrradianceTexture;
HYP_DESCRIPTOR_SRV(LightmapVolume, RadianceTexture) uniform texture2D RadianceTexture;
HYP_DESCRIPTOR_SAMPLER(LightmapVolume, Sampler) uniform sampler Sampler;
HYP_DESCRIPTOR_SAMPLER(LightmapVolume, GBufferSampler) uniform sampler GBufferSampler;

HYP_DESCRIPTOR_CBUFF(LightmapVolume, LightmapVolumeUniforms) uniform LightmapVolumeUniforms
{
    float irradianceWeight;
    float radianceWeight;

    uint numAtlases;
};

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

void main()
{
    vec4 albedo = Texture2D(GBufferSampler, gbuffer_albedo_texture, texcoord);

    uvec4 materialData = texture(usampler2D(gbuffer_material_texture, GBufferSampler), texcoord);

    GBufferMaterialParams materialParams;
    GBufferUnpackMaterialParams(materialData.xy, materialParams);

    const float roughness = materialParams.roughness;
    const float metalness = materialParams.metalness;
    const float transmission = materialParams.transmission;
    const float ao = materialParams.ao;
    const uint object_mask = materialParams.mask;

    const mat4 inverse_proj = inverse(camera.projection);
    const mat4 inverse_view = inverse(camera.view);

    vec3 N = GBufferUnpackNormal(Texture2D(GBufferSampler, gbuffer_normals_texture, texcoord));
    vec2 UV1 = vec2(uintBitsToFloat(materialData.z), uintBitsToFloat(materialData.w));

    const float depth = Texture2D(GBufferSampler, gbuffer_depth_texture, texcoord).r;
    const vec3 P = ReconstructWorldSpacePositionFromDepth(inverse_proj, inverse_view, texcoord, depth).xyz;
    const vec3 V = normalize(camera.position.xyz - P);
    const vec3 R = normalize(reflect(-V, N));

    vec2 lightmap_uv = UV1;

    // sample lightmap atlases based on weights
    vec4 irradiance = Texture2D(Sampler, IrradianceTexture, lightmap_uv) * irradianceWeight;
    vec4 radiance = Texture2D(Sampler, RadianceTexture, lightmap_uv) * radianceWeight;

    vec3 ibl = vec3(0.0);
    vec3 F = vec3(0.0);

    float NdotV = max(0.0001, dot(N, V));

    const vec3 diffuse_color = CalculateDiffuseColor(albedo.rgb, metalness);
    const vec3 F0 = CalculateF0(albedo.rgb, metalness);

    F = CalculateFresnelTerm(F0, roughness, NdotV);
    const vec3 kD = (vec3(1.0) - F) * (1.0 - metalness);

    const vec3 dfg = CalculateDFG(F, roughness, NdotV);
    const vec3 E = CalculateE(F0, dfg);
    const vec3 energy_compensation = CalculateEnergyCompensation(F0, dfg);

    vec4 reflections_color = Texture2D(GBufferSampler, reflections_texture, texcoord);
    ibl = ibl * (1.0 - reflections_color.a) + (reflections_color.rgb * reflections_color.a);

    vec4 rt_radiance = Texture2D(GBufferSampler, rt_radiance_final, texcoord);
    ibl = ibl * (1.0 - rt_radiance.a) + (rt_radiance.rgb * rt_radiance.a);

    vec3 spec = (ibl * mix(dfg.xxx, dfg.yyy, F0)) * energy_compensation;

    color_output = (albedo * irradiance) + (albedo * radiance) + vec4(spec, 0.0);
    color_output.a = 1.0;
}