#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_samplerless_texture_functions : enable

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../include/defines.inc"
#include "../include/shared.inc"
#include "../include/material.inc"
#include "../include/packing.inc"
#include "../include/gbuffer.inc"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec3 v_normal;
layout(location = 2) in vec2 v_texcoord0;
layout(location = 3) in vec4 v_color;

layout(location = 0) out vec4 gbuffer_albedo;
layout(location = 2) out uvec4 gbuffer_material;

DECLARE_SRV(ParticleDescriptorSet, ParticleTexture) uniform texture2D ParticleTexture;
DECLARE_SAMPLER(ParticleDescriptorSet, SamplerLinear) uniform sampler SamplerLinear;

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../include/Entity.inc"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

void main()
{
    vec4 color = SAMPLE_TEXTURE_2D(SamplerLinear, ParticleTexture, v_texcoord0);
    //color *= v_color;

    gbuffer_albedo = color;

    gbuffer_material.x = OBJECT_MASK_TRANSLUCENT | OBJECT_MASK_PARTICLE;
    gbuffer_material.y = 0u;
    gbuffer_material.z = 0u;
    gbuffer_material.w = 0u;
}
