#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : enable

#include "include/defines.inc"

layout(location = 1) in vec3 v_position;
layout(location = 2) in vec2 v_texcoord0;
layout(location = 6) in flat vec3 v_camera_position;
layout(location = 9) in flat uint v_object_index;

layout(location = 0) out vec4 output_shadow;

HYP_DESCRIPTOR_SAMPLER(Default, SamplerLinear) uniform sampler sampler_linear;
HYP_DESCRIPTOR_SAMPLER(Default, SamplerNearest) uniform sampler sampler_nearest;

#define texture_sampler sampler_linear

#include "include/Entity.inc"
#include "include/material.inc"
#include "include/shared.inc"
#include "include/packing.inc"

#ifdef INSTANCING

HYP_DESCRIPTOR_SRV(Default, EntitiesBuffer) readonly buffer EntitiesBuffer
{
    Entity entities[];
};

#else

HYP_DESCRIPTOR_SRV_DYNAMIC(Default, CurrentEntity) readonly buffer CurrentEntity
{
    Entity entity;
};

#endif

HYP_DESCRIPTOR_SRV_DYNAMIC(Default, MaterialsBuffer) readonly buffer MaterialsBuffer
{
    Material material;
};

#ifndef CURRENT_MATERIAL
#define CURRENT_MATERIAL material
#endif

void main()
{
    // if (bool(GET_OBJECT_BUCKET(entity) & OBJECT_MASK_SKY)) {
    //     discard;
    // }

#if defined(ALPHA_DISCARD) && HAS_ALBEDO_MAP
    vec4 albedo_texture = SAMPLE_MATERIAL_TEXTURE(CURRENT_MATERIAL, AlbedoMap, v_texcoord0);

    if (albedo_texture.a < MATERIAL_ALPHA_DISCARD)
    {
        discard;
    }
#endif

    const float depth = gl_FragCoord.z / gl_FragCoord.w;

#ifdef MODE_VSM
    vec2 moments = vec2(depth, HYP_FMATH_SQR(depth));

    float dx = dFdx(depth);
    float dy = dFdy(depth);

    moments.y += 0.25 * (HYP_FMATH_SQR(dx) + HYP_FMATH_SQR(dy));

    output_shadow = vec4(moments, 0.0, 0.0);
#else
    output_shadow = PackDepth(depth);
#endif
}
