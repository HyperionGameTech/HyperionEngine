#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec2 v_texcoord;
layout(location = 0) out vec4 color_output;

#include "../include/defines.inc"
#include "../include/shared.inc"

DECLARE_SRV(GenerateMipmap, InputTexture) uniform texture2D input_texture;
DECLARE_SAMPLER(GenerateMipmap, SamplerLinear) uniform sampler sampler_linear;
DECLARE_SAMPLER(GenerateMipmap, SamplerNearest) uniform sampler sampler_nearest;

void main()
{
    vec4 input_color = SAMPLE_TEXTURE_2D(sampler_linear, input_texture, v_texcoord);

    color_output = input_color;
}