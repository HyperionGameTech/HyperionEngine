#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec2 v_texcoord0;
layout(location = 0) out vec4 color_output;

#include "../include/shared.inc"
#include "../include/defines.inc"
#include "../include/gbuffer.inc"

layout(set = DECLARE_SET_GLOBAL, binding = 50) uniform texture2D temporal_aa_result;
layout(set = DECLARE_SET_GLOBAL, binding = 77) uniform texture2D dof_blur_hor;
layout(set = DECLARE_SET_GLOBAL, binding = 78) uniform texture2D dof_blur_vert;

layout(push_constant) uniform PushConstant
{
    uvec2 dimensions;
};

void main()
{
    vec4 color = vec4(0.0);

    const vec2 texel_size = vec2(1.0) / vec2(dimensions);
    const float radius = 0.70;
    vec2 offset = vec2(0.0);

#ifdef DIRECTION_VERTICAL
#define INPUT_TEXTURE dof_blur_hor

    offset = vec2(1.0, 0.0);
#else

#ifdef TEMPORAL_AA
#define INPUT_TEXTURE temporal_aa_result
#else
#define INPUT_TEXTURE gbuffer_deferred_result
#endif

    offset = vec2(0.0, 1.0);
#endif

    for (float i = -3.0; i <= 3.0; i += 1.0)
    {
        color += SAMPLE_TEXTURE_2D(gbuffer_sampler, INPUT_TEXTURE, v_texcoord0 + (offset * texel_size * radius * i));
    }

    color /= 7.0;

    color_output = color;
}