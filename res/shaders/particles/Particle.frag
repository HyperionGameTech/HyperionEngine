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
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec3 v_normal;
layout(location = 2) in vec2 v_texcoord0;
layout(location = 3) in vec4 v_color;

layout(location = 0) out vec4 gbuffer_albedo;
layout(location = 5) out uint gbuffer_mask;

HYP_DESCRIPTOR_SRV(ParticleDescriptorSet, ParticleTexture) uniform texture2D albedo_texture;
HYP_DESCRIPTOR_SAMPLER(Global, SamplerLinear) uniform sampler texture_sampler;

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../include/Entity.glsl"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

void main()
{
    vec4 color = Texture2D(texture_sampler, albedo_texture, v_texcoord0);
    color *= v_color;

    gbuffer_albedo = color;
    gbuffer_mask = OBJECT_MASK_TRANSLUCENT | OBJECT_MASK_PARTICLE;
}
