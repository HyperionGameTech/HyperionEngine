#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) out vec3 v_position;
layout(location = 1) out vec2 v_texcoord;

HYP_ATTRIBUTE(0) in vec3 a_position;
HYP_ATTRIBUTE(2) in vec2 a_texcoord0;

void main()
{
    vec4 position = vec4(a_position, 1.0);

    v_position = position.xyz;
    v_texcoord = a_texcoord0;

    gl_Position = position;
}