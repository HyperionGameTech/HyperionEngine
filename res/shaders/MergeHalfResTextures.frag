
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

DECLARE_BUFFER(MergeHalfResTexture, WorldsBuffer) uniform WorldsBuffer
{
    WorldShaderData world_shader_data;
};

DECLARE_SAMPLER(MergeHalfResTexture, SamplerNearest) uniform sampler sampler_nearest;
DECLARE_SAMPLER(MergeHalfResTexture, SamplerLinear) uniform sampler sampler_linear;

DECLARE_SRV(MergeHalfResTexture, InTexture) uniform texture2D src_image;

DECLARE_BUFFER(MergeHalfResTexture, UniformBuffer) uniform UniformBuffer
{
    uvec2 dimensions;
};

void main()
{
    vec2 texcoord_a = v_texcoord * vec2(0.5, 1.0);
    vec2 texcoord_b = texcoord_a + vec2(0.5, 0.0);

    uvec2 pixel_coord = uvec2(v_texcoord * (vec2(dimensions) - 1.0));
    uint checkerboard = (pixel_coord.x & 1) ^ (pixel_coord.y & 1);

    vec2 texcoord = mix(texcoord_a, texcoord_b, float(checkerboard));

    color_output = SAMPLE_TEXTURE_2D(sampler_nearest, src_image, texcoord);
}
