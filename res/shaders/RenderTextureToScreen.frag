
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

DECLARE_SAMPLER(RenderTextureToScreenDescriptorSet, SamplerLinear) uniform sampler sampler_linear;

DECLARE_BUFFER(RenderTextureToScreenDescriptorSet, WorldsBuffer) uniform WorldsBuffer
{
    WorldShaderData world_shader_data;
};

DECLARE_SRV(RenderTextureToScreenDescriptorSet, InTexture) uniform texture2D src_image;

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

void main()
{
    vec2 texcoord = v_texcoord;

#ifdef HALFRES
    // map texcoords to previous frame's output coords
    texcoord = (texcoord * 0.5) + vec2(0.5 * float((world_shader_data.frame_counter - 1) & 1), 0.0);
#endif

    color_output = SAMPLE_TEXTURE_2D(sampler_linear, src_image, texcoord);
}
