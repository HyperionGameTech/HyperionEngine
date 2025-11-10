#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec3 v_normal;
layout(location = 2) in vec2 v_texcoord0;
layout(location = 4) in vec3 v_tangent;
layout(location = 5) in vec3 v_bitangent;
layout(location = 7) in flat vec3 v_camera_position;
layout(location = 8) in mat3 v_tbn_matrix;
layout(location = 12) in vec3 v_view_space_position;

layout(location = 0) out vec4 gbuffer_albedo;
layout(location = 1) out vec4 gbuffer_normals;
layout(location = 2) out uvec2 gbuffer_material;

#include "include/scene.inc"
#include "include/material.inc"
#include "include/Entity.glsl"
#include "include/packing.inc"

void main()
{
    vec3 normal = normalize(v_normal);
    vec2 texcoord = v_texcoord0;

    gbuffer_albedo = vec4(1.0);
    gbuffer_normals = vec4(0.0);
    gbuffer_material = uvec2(0);
}
