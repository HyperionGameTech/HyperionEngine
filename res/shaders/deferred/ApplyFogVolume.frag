#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_separate_shader_objects : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_samplerless_texture_functions : require

#include "../include/defines.inc"

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec3 v_normal;
layout(location = 2) in vec2 v_texcoord0;
layout(location = 3) in vec2 v_texcoord1;
layout(location = 4) in vec3 v_tangent;
layout(location = 5) in vec3 v_bitangent;
layout(location = 7) in flat vec3 v_camera_position;
layout(location = 11) in vec4 v_position_ndc;
layout(location = 12) in vec4 v_previous_position_ndc;
layout(location = 15) in flat uint v_object_index;
layout(location = 16) in flat uint v_object_mask;

layout(location = 0) out vec4 gbuffer_albedo;
layout(location = 1) out vec4 gbuffer_normals;
layout(location = 2) out uvec4 gbuffer_material;
layout(location = 3) out vec2 gbuffer_velocity;

HYP_DESCRIPTOR_SAMPLER(Global, SamplerLinear) uniform sampler sampler_linear;
HYP_DESCRIPTOR_SAMPLER(Global, SamplerNearest) uniform sampler sampler_nearest;

#define texture_sampler sampler_linear

#include "../include/scene.inc"
#include "../include/material.inc"
#include "../include/Entity.glsl"
#include "../include/packing.inc"
#include "../include/shared.inc"

#include "../include/env_probe.inc"
#include "../include/gbuffer.inc"

HYP_DESCRIPTOR_SRV(View, GBufferMipChain) uniform texture2D gbuffer_mip_chain;

HYP_DESCRIPTOR_CBUFF_DYNAMIC(Global, EnvGridsBuffer) uniform EnvGridsBuffer
{
    EnvGrid env_grid;
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

#ifdef SHADING_TYPE_FORWARD
#include "../include/brdf.inc"
#include "DeferredLighting.glsl"
#include "../include/shadows.inc"
#endif

HYP_DESCRIPTOR_SSBO_DYNAMIC(Global, CurrentEnvProbe) readonly buffer CurrentEnvProbe
{
    EnvProbe current_env_probe;
};

HYP_DESCRIPTOR_SSBO_DYNAMIC(Global, EntitiesBuffer) readonly buffer EntitiesBuffer
{
    Entity entity;
};

// @TODO Refactor to use LightsBuffer instead

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
#define CURRENT_MATERIAL (materials[entity.material_index % HYP_MAX_MATERIALS])
#endif
#else

HYP_DESCRIPTOR_SSBO_DYNAMIC(Entity, MaterialsBuffer) readonly buffer MaterialsBuffer
{
    Material material;
};

#ifndef CURRENT_MATERIAL
#define CURRENT_MATERIAL material
#endif
#endif

#include "FogVolume.inl"

HYP_DESCRIPTOR_SRV(FogVolume, VolumeTexture) uniform texture3D VolumeTexture;
HYP_DESCRIPTOR_CBUFF(FogVolume, FogVolumeUniforms) uniform FogVolumeUniforms
{
    FogVolume fogVolume;
};

void main()
{
    vec3 fogCoordWorld = v_position;
    vec4 fogCoordLocal = inverse(fogVolume.transformMatrix) * vec4(fogCoordWorld, 1.0);
    fogCoordLocal /= fogCoordLocal.w;
    vec3 fogCoord = fogCoordLocal.xyz * 0.5 + 0.5;

    vec4 fogValues = Texture3D(texture_sampler, VolumeTexture, fogCoord);

    vec3 fogColor = fogCoord; // Simple gray-blue fog color

    gbuffer_albedo = vec4(fogValues.xyz, 1.0);
    gbuffer_normals = vec4(0.0);
    gbuffer_material = uvec4(0);
    gbuffer_velocity = vec2(0.0);
}