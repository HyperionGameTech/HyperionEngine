
#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_separate_shader_objects : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec2 v_texcoord;

layout(location = 0) out vec4 color_output;

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "include/shared.inc"

#include "include/scene.inc"

HYP_DESCRIPTOR_CBUFF(Global, WorldsBuffer) uniform WorldsBuffer
{
    WorldShaderData world_shader_data;
};

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

HYP_DESCRIPTOR_SRV(RenderTextureToScreenDescriptorSet, InTexture) uniform texture2D src_image;
HYP_DESCRIPTOR_SAMPLER(Global, SamplerNearest) uniform sampler sampler_nearest;
HYP_DESCRIPTOR_SAMPLER(Global, SamplerLinear) uniform sampler sampler_linear;

void main()
{
    color_output = Texture2D(sampler_linear, src_image, v_texcoord);
}
