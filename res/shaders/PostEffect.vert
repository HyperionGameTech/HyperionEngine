#version 450

layout(location = 0) out vec3 v_position;
layout(location = 1) out vec2 v_texcoord;

HYP_ATTRIBUTE(0) vec3 a_position;
HYP_ATTRIBUTE(1) vec3 a_normal;
HYP_ATTRIBUTE(2) vec2 a_texcoord0;

void main()
{
    vec4 position = vec4(a_position, 1.0);

    v_position = position.xyz;
    v_texcoord = a_texcoord0;

    gl_Position = position;
}