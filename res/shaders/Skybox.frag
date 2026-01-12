#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_separate_shader_objects : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_scalar_block_layout : require

#include "include/defines.inc"

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec3 v_normal;
layout(location = 2) in vec2 v_texcoord0;
layout(location = 3) in flat uint v_object_index;

layout(location = 0) out vec4 gbuffer_albedo;
layout(location = 1) out vec4 gbuffer_normals;
layout(location = 2) out uvec4 gbuffer_material;
layout(location = 3) out vec2 gbuffer_velocity;

HYP_DESCRIPTOR_SAMPLER(Global, SamplerLinear) uniform sampler texture_sampler;

#include "include/gbuffer.inc"
#include "include/Entity.glsl"
#include "include/packing.inc"

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS 1 // don't want to define AlbedoMap as a 2D texture
#include "include/material.inc"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

HYP_DESCRIPTOR_SRV(Material, AlbedoMap) uniform textureCube AlbedoMap;

HYP_DESCRIPTOR_SSBO(Global, EntitiesBuffer) readonly buffer EntitiesBuffer
{
    Entity entities[];
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

HYP_DESCRIPTOR_SSBO_DYNAMIC(Entity, MaterialsBuffer) readonly buffer MaterialsBuffer
{
    Material material;
};

#ifndef CURRENT_MATERIAL
#define CURRENT_MATERIAL material
#endif
#endif

void main()
{
    vec3 normal = normalize(v_normal);

#if defined(HYP_MATERIAL_CUBEMAP_TEXTURES) && HYP_MATERIAL_CUBEMAP_TEXTURES
    gbuffer_albedo = vec4(SAMPLE_TEXTURE_CUBE(CURRENT_MATERIAL, AlbedoMap, v_position).rgb, 1.0);
#else
    gbuffer_albedo = vec4(0.0);
#endif

    gbuffer_normals = GBufferPackNormal(normal);

    gbuffer_material.x = OBJECT_MASK_SKY;
    gbuffer_material.y = 0u;
    gbuffer_material.z = 0u;
    gbuffer_material.w = 0u;
    
    gbuffer_velocity = vec2(0.0);
}
