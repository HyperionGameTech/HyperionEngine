#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_separate_shader_objects : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_samplerless_texture_functions : require

#include "include/defines.inc"

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec3 v_normal;
layout(location = 2) in vec2 v_texcoord0;
layout(location = 3) in vec2 v_texcoord1;
layout(location = 4) in vec3 v_tangent;
layout(location = 5) in vec3 v_bitangent;
layout(location = 7) in flat vec3 v_camera_position;
layout(location = 8) in mat3 v_tbn_matrix;
layout(location = 11) in vec4 v_position_ndc;
layout(location = 12) in vec4 v_previous_position_ndc;
layout(location = 15) in flat uint v_object_index;
layout(location = 16) in flat uint v_object_mask;

layout(location = 0) out vec4 gbuffer_albedo;
layout(location = 1) out vec4 gbuffer_normals;
layout(location = 2) out uvec2 gbuffer_material;
layout(location = 4) out vec2 gbuffer_velocity;
layout(location = 5) out vec4 gbuffer_ws_normals;

HYP_DESCRIPTOR_SAMPLER(Global, SamplerLinear) uniform sampler sampler_linear;
HYP_DESCRIPTOR_SAMPLER(Global, SamplerNearest) uniform sampler sampler_nearest;

#define texture_sampler sampler_linear

#define HAS_REFRACTION 1

#include "include/scene.inc"
#include "include/material.inc"
#include "include/Entity.glsl"
#include "include/packing.inc"

#include "include/env_probe.inc"
#include "include/gbuffer.inc"

HYP_DESCRIPTOR_SRV(View, GBufferMipChain) uniform texture2D gbuffer_mip_chain;

HYP_DESCRIPTOR_SRV(Global, EnvProbeTextures, count = 16) uniform texture2D env_probe_textures[16];
HYP_DESCRIPTOR_CBUFF_DYNAMIC(Global, EnvGridsBuffer) uniform EnvGridsBuffer
{
    EnvGrid env_grid;
};
HYP_DESCRIPTOR_SSBO(Global, EnvProbesBuffer) readonly buffer EnvProbesBuffer
{
    EnvProbe env_probes[];
};

HYP_DESCRIPTOR_SRV(Global, LightFieldColorTexture) uniform texture2D light_field_color_texture;
HYP_DESCRIPTOR_SRV(Global, LightFieldDepthTexture) uniform texture2D light_field_depth_texture;

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

#ifdef LIGHTING_FORWARD
#include "include/brdf.inc"
#include "deferred/DeferredLighting.glsl"
#include "include/shadows.inc"
#endif

HYP_DESCRIPTOR_SSBO_DYNAMIC(Global, CurrentEnvProbe) readonly buffer CurrentEnvProbe
{
    EnvProbe current_env_probe;
};

#ifdef INSTANCING

HYP_DESCRIPTOR_SSBO(Global, EntitiesBuffer) readonly buffer EntitiesBuffer
{
    Entity entities[];
};

#else

HYP_DESCRIPTOR_SSBO_DYNAMIC(Entity, CurrentEntity) readonly buffer EntitiesBuffer
{
    Entity entity;
};

#endif

HYP_DESCRIPTOR_SSBO_DYNAMIC(Global, CurrentLight) readonly buffer CurrentLight
{
    Light light;
};

#ifdef HYP_USE_INDEXED_ARRAY_FOR_OBJECT_DATA
HYP_DESCRIPTOR_SSBO(Entity, MaterialsBuffer) readonly buffer MaterialsBuffer
{
    Material materials[HYP_MAX_MATERIALS];
};

#ifndef CURRENT_MATERIAL
#define CURRENT_MATERIAL (materials[entity.material_index])
#endif
#else

HYP_DESCRIPTOR_SSBO_DYNAMIC(Entity, MaterialsBuffer)
readonly buffer MaterialsBuffer
{
    Material material;
};

#ifndef CURRENT_MATERIAL
#define CURRENT_MATERIAL material
#endif
#endif

void main()
{
    vec3 view_vector = normalize(v_camera_position - v_position);
    vec3 normal = normalize(v_normal);
    float NdotV = dot(normal, view_vector);

    vec3 tangent_view = transpose(v_tbn_matrix) * view_vector;
    vec3 tangent_position = v_tbn_matrix * v_position;

    vec3 reflection_vector = reflect(view_vector, normal);

    gbuffer_albedo = CURRENT_MATERIAL.albedo;
    gbuffer_albedo.a = 1.0;

    float ao = 1.0;
    float metalness = GET_MATERIAL_PARAM(CURRENT_MATERIAL, MATERIAL_PARAM_METALNESS);
    float roughness = GET_MATERIAL_PARAM(CURRENT_MATERIAL, MATERIAL_PARAM_ROUGHNESS);
    float transmission = GET_MATERIAL_PARAM(CURRENT_MATERIAL, MATERIAL_PARAM_TRANSMISSION);

    vec2 texcoord = v_texcoord0 * CURRENT_MATERIAL.uv_scale;

#if HAS_ALBEDO_MA
    vec4 albedo_texture = SAMPLE_TEXTURE_TRIPLANAR(CURRENT_MATERIAL, AlbedoMap, v_position, normal);

    // if (albedo_texture.a < MATERIAL_ALPHA_DISCARD) {
    //     discard;
    // }

    gbuffer_albedo = vec4(albedo_texture.rgb, 1.0);
#endif

    // temp grass color
    // gbuffer_albedo.rgb = vec3(0.5, 0.8, 0.35) * 0.15;

    // lerp to rock based on slope
    // gbuffer_albedo.rgb = mix(gbuffer_albedo.rgb, vec3(0.11), 1.0 - smoothstep(0.45, 0.65, abs(dot(normal, vec3(0.0, 1.0, 0.0)))));

    // gbuffer_albedo.a = 0.0;// no lighting for now

    vec4 normals_texture = vec4(0.0);

#if HAS_NORMAL_MAP
    normals_texture = SAMPLE_TEXTURE_TRIPLANAR(CURRENT_MATERIAL, NormalMap, v_position, normal) * 2.0 - 1.0;
    normal = normalize(v_tbn_matrix * normals_texture.rgb);
#endif

    // if (HAS_TEXTURE(CURRENT_MATERIAL, MATERIAL_TEXTURE_METALNESS_MAP)) {
    //     float metalness_sample = SAMPLE_TEXTURE(CURRENT_MATERIAL, MATERIAL_TEXTURE_METALNESS_MAP, texcoord).r;

    //     metalness = metalness_sample;//mix(metalness, metalness_sample, metalness_sample);
    // }
#if HAS_ROUGHNESS_MAP
    float roughness_sample = SAMPLE_TEXTURE_TRIPLANAR(CURRENT_MATERIAL, RoughnessMap, v_position, normal).r;

    roughness = roughness_sample; // mix(roughness, roughness_sample, roughness_sample);
#endif

    // if (HAS_TEXTURE(CURRENT_MATERIAL, MATERIAL_TEXTURE_AO_MAP)) {
    //     ao = SAMPLE_TEXTURE(CURRENT_MATERIAL, MATERIAL_TEXTURE_AO_MAP, texcoord).r;
    // }

    // gbuffer_albedo.rgb = GetTriplanarBlend(normal);

    vec2 velocity = vec2(((v_position_ndc.xy / v_position_ndc.w) * 0.5 + 0.5) - ((v_previous_position_ndc.xy / v_previous_position_ndc.w) * 0.5 + 0.5));

    GBufferMaterialParams materialParams;
    materialParams.roughness = roughness;
    materialParams.metalness = metalness;
    materialParams.transmission = transmission;
    materialParams.ao = ao;
    materialParams.mask = GET_OBJECT_BUCKET(entity) | OBJECT_MASK_TERRAIN;

    gbuffer_normals = GBufferPackNormalUV1(normal, v_texcoord1);
    gbuffer_material = GBufferPackMaterialParams(materialParams);
    gbuffer_velocity = velocity;
}
