#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

layout(location = 0) out vec3 v_position;
layout(location = 1) out vec4 v_positionNdc;

HYP_ATTRIBUTE(0) vec3 a_position;
HYP_ATTRIBUTE(1) vec3 a_normal;
HYP_ATTRIBUTE(2) vec2 a_texcoord0;
HYP_ATTRIBUTE_OPTIONAL(3) vec2 a_texcoord1;
HYP_ATTRIBUTE_OPTIONAL(4) vec3 a_tangent;
HYP_ATTRIBUTE_OPTIONAL(5) vec3 a_bitangent;
HYP_ATTRIBUTE_OPTIONAL(6) vec4 a_bone_weights;
HYP_ATTRIBUTE_OPTIONAL(7) vec4 a_bone_indices;

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "../include/scene.inc"
#include "../include/shared.inc"

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "FogVolume.inl"

HYP_DESCRIPTOR_CBUFF_DYNAMIC(Global, CamerasBuffer) uniform CamerasBuffer
{
    Camera camera;
};

HYP_DESCRIPTOR_CBUFF(FogVolume, FogVolumeUniforms) uniform FogVolumeUniforms
{
    FogVolume fogVolume;
};

void main()
{
    vec4 position = fogVolume.transformMatrix * vec4(a_position, 1.0);

    v_position = position.xyz / position.w;

    mat4 jitter_matrix = mat4(1.0);
    jitter_matrix[3][0] += camera.jitter.x;
    jitter_matrix[3][1] += camera.jitter.y;

    v_positionNdc = (jitter_matrix * camera.projection) * camera.view * position;

    gl_Position = v_positionNdc;
}
