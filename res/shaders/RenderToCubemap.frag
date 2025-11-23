#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_separate_shader_objects : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_scalar_block_layout : require

#include "include/defines.inc"

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec3 v_normal;
layout(location = 2) in vec2 v_texcoord0;
layout(location = 7) in flat vec3 v_camera_position;
layout(location = 11) in flat uint v_object_index;
layout(location = 13) in flat uint v_cube_face_index;

#ifdef MODE_SHADOWS
layout(location = 0) out vec2 output_color;
#else
layout(location = 0) out vec4 output_color;
#endif // !MODE_SHADOWS

#ifdef WRITE_NORMALS
layout(location = 1) out vec2 output_normals;
#ifdef WRITE_MOMENTS
layout(location = 2) out vec2 output_moments;
#endif // WRITE_MOMENTS
#else  // WRITE_NORMALS
#ifdef WRITE_MOMENTS
layout(location = 1) out vec2 output_moments;
#endif // WRITE_MOMENTS
#endif // !WRITE_NORMALS

HYP_DESCRIPTOR_SAMPLER(Global, SamplerLinear) uniform sampler sampler_linear;
HYP_DESCRIPTOR_SAMPLER(Global, SamplerNearest) uniform sampler sampler_nearest;

#define texture_sampler sampler_linear

#include "include/scene.inc"
#include "include/shared.inc"
#include "include/material.inc"
#include "include/gbuffer.inc"
#include "include/env_probe.inc"
#include "include/Octahedron.glsl"
#include "include/Entity.glsl"
#include "include/packing.inc"
#include "include/brdf.inc"

#define HYP_CUBEMAP_AMBIENT 0.005

HYP_DESCRIPTOR_SRV(Global, ShadowMapsTextureArray) uniform texture2DArray shadow_maps;
HYP_DESCRIPTOR_SRV(Global, PointLightShadowMapsTextureArray) uniform textureCubeArray point_shadow_maps;

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

HYP_DESCRIPTOR_SSBO_DYNAMIC(Entity, MaterialsBuffer) readonly buffer MaterialsBuffer
{
    Material material;
};

#ifndef CURRENT_MATERIAL
#define CURRENT_MATERIAL material
#endif
#endif

const vec3 face_debug_colors[6] = vec3[](
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0),
    vec3(1.0, 1.0, 0.0),
    vec3(1.0, 0.0, 1.0),
    vec3(0.0, 1.0, 1.0)
);

void main()
{
    vec3 V = normalize(v_camera_position - v_position);
    vec3 N = normalize(v_normal);
    vec3 R = reflect(-V, N);

    vec4 albedo = vec4(1.0);

#if HAS_ALBEDO_MAP
    vec2 texcoord = v_texcoord0 * CURRENT_MATERIAL.uv_scale;
    albedo = CURRENT_MATERIAL.albedo;

    vec4 albedo_texture = SAMPLE_TEXTURE(CURRENT_MATERIAL, AlbedoMap, texcoord);

    if (albedo_texture.a < 0.2)
    {
        discard;
    }

    albedo *= albedo_texture;
#endif

#if defined(WRITE_MOMENTS) || defined(MODE_SHADOWS)
    // Write distance, mean distance for variance.
    const float dist = distance(v_position, v_camera_position);

    vec2 moments = vec2(dist, HYP_FMATH_SQR(dist));
#endif

#ifdef MODE_SHADOWS
    float dx = dFdx(dist);
    float dy = dFdy(dist);

    moments.y += 0.25 * (HYP_FMATH_SQR(dx) + HYP_FMATH_SQR(dy));

    output_color = moments;
#else
    vec4 previous_value = vec4(0.0);

    output_color.rgb = albedo.rgb;
    output_color.a = 1.0;

#ifdef WRITE_NORMALS
    output_normals = PackNormalVec2(N);
#endif

#ifdef WRITE_MOMENTS
    output_moments = moments;
#endif
#endif
}
