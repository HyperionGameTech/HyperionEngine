#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_separate_shader_objects : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_scalar_block_layout : require

#include "include/defines.inc"

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec3 v_normal;
layout(location = 2) in vec2 v_texcoord0;
layout(location = 4) in vec3 v_tangent;
layout(location = 5) in vec3 v_bitangent;
layout(location = 11) in vec4 v_position_ndc;
layout(location = 12) in vec4 v_previous_position_ndc;
layout(location = 15) in flat uint v_object_index;
#ifdef IMMEDIATE_MODE
layout(location = 16) in vec4 v_color;
layout(location = 17) in flat uint v_env_probe_index;
layout(location = 18) in flat uint v_env_probe_type;
#endif

layout(location = 0) out vec4 gbuffer_albedo;
layout(location = 1) out vec4 gbuffer_normals;
layout(location = 2) out uvec4 gbuffer_material;
layout(location = 3) out vec2 gbuffer_velocity;

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

HYP_DESCRIPTOR_SAMPLER(Global, SamplerLinear) uniform sampler sampler_linear;
HYP_DESCRIPTOR_SAMPLER(Global, SamplerNearest) uniform sampler sampler_nearest;

#include "include/material.inc"
#include "include/packing.inc"
#include "include/scene.inc"
#include "include/gbuffer.inc"

HYP_DESCRIPTOR_SRV(View, GBufferMipChain) uniform texture2D gbuffer_mip_chain;

HYP_DESCRIPTOR_CBUFF_DYNAMIC(Global, CamerasBuffer) uniform CamerasBuffer
{
    Camera camera;
};

HYP_DESCRIPTOR_CBUFF(Global, WorldsBuffer) uniform WorldsBuffer
{
    WorldShaderData world_shader_data;
};

HYP_DESCRIPTOR_SRV(Global, LightFieldColorTexture) uniform texture2D light_field_color_texture;
HYP_DESCRIPTOR_SRV(Global, LightFieldDepthTexture) uniform texture2D light_field_depth_texture;

#include "include/Entity.glsl"

#ifdef IMMEDIATE_MODE

#include "include/brdf.inc"

#elif defined(INSTANCING)

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

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "include/env_probe.inc"

HYP_DESCRIPTOR_CBUFF_DYNAMIC(Global, EnvGridsBuffer) uniform EnvGridsBuffer
{
    EnvGrid env_grid;
};

#include "deferred/DeferredLighting.glsl"

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

    vec2 velocity = vec2(((v_position_ndc.xy / v_position_ndc.w) * 0.5 + 0.5) - ((v_previous_position_ndc.xy / v_previous_position_ndc.w) * 0.5 + 0.5));

    GBufferMaterialParams materialParams;
    materialParams.roughness = 0.0;
    materialParams.metalness = 0.0;
    materialParams.transmission = 0.0;
    materialParams.ao = 1.0;

    gbuffer_albedo = vec4(0.0, 1.0, 0.0, 1.0);
    gbuffer_normals = GBufferPackNormal(normal);
    gbuffer_velocity = vec2(velocity);

#ifdef IMMEDIATE_MODE
    gbuffer_albedo = vec4(v_color.rgb, 1.0);

    materialParams.mask = OBJECT_MASK_TRANSLUCENT | OBJECT_MASK_DEBUG;

    if (v_env_probe_index != ~0u && v_env_probe_type == ENV_PROBE_TYPE_REFLECTION)
    {
        const vec3 N = normal;
        const vec3 V = normalize(camera.position.xyz - v_position.xyz);

        vec4 ibl = vec4(0.0);

        const vec3 R = reflect(-V, N);

        ApplyReflectionProbe(
            env_probes[v_env_probe_index].texture_index,
            env_probes[v_env_probe_index].world_position.xyz,
            env_probes[v_env_probe_index].aabb_min.xyz,
            env_probes[v_env_probe_index].aabb_max.xyz,
            v_position.xyz,
            R,
            0.0,
            ibl);

        gbuffer_albedo.rgb = ibl.rgb;
    }
#else
    materialParams.mask = GET_OBJECT_BUCKET(entity);
#endif

    float roughnessAndMetalPacked;
    uint maskPacked;
    GBufferPackMaterialParams(materialParams, roughnessAndMetalPacked, maskPacked);
    
    gbuffer_normals.x = roughnessAndMetalPacked;

    gbuffer_material.x = maskPacked;
    gbuffer_material.z = 0u;
    gbuffer_material.w = 0u;
    gbuffer_material.y = 0u;

    gbuffer_velocity = velocity;
}
