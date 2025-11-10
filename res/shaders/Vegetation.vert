#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

layout(location = 0) out vec3 v_position;
layout(location = 1) out vec3 v_normal;
layout(location = 2) out vec2 v_texcoord0;
layout(location = 4) out vec3 v_tangent;
layout(location = 5) out vec3 v_bitangent;
layout(location = 7) out flat vec3 v_camera_position;
layout(location = 8) out mat3 v_tbn_matrix;
layout(location = 11) out vec4 v_position_ndc;
layout(location = 12) out vec4 v_previous_position_ndc;

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_texcoord0;
layout(location = 3) in vec2 a_texcoord1;
layout(location = 4) in vec3 a_tangent;
layout(location = 5) in vec3 a_bitangent;
layout(location = 6) in vec4 a_bone_weights;
layout(location = 7) in vec4 a_bone_indices;

#include "include/scene.inc"

#include "include/Entity.glsl"

#include "include/Skeleton.glsl"

void main()
{
    vec4 position;
    vec4 previous_position;
    mat4 normal_matrix;

    vec3 displaced_position = a_position;
    const float movement = a_texcoord0.t * a_texcoord0.t;
    displaced_position += vec3(sin(world_shader_data.game_time * 2.0) * 0.1, 0.0, cos(world_shader_data.game_time * 2.0) * 0.1) * movement;
    displaced_position += vec3(sin(world_shader_data.game_time * 0.5) * 0.4, 0.0, cos(world_shader_data.game_time * 0.5) * 0.4) * movement;

#ifdef SKINNING
    mat4 skinning_matrix = CreateSkinningMatrix(ivec4(a_bone_indices), a_bone_weights);

    position = entity.model_matrix * skinning_matrix * vec4(displaced_position, 1.0);
    previous_position = entity.previous_model_matrix * skinning_matrix * vec4(displaced_position, 1.0);
    normal_matrix = transpose(inverse(entity.model_matrix * skinning_matrix));
#else
    // This is not valid calc for prevous position with displacement
    position = entity.model_matrix * vec4(displaced_position, 1.0);
    previous_position = entity.previous_model_matrix * vec4(displaced_position, 1.0);
    normal_matrix = transpose(inverse(entity.model_matrix));
#endif

    v_position = position.xyz;
    v_normal = (normal_matrix * vec4(a_normal, 0.0)).xyz;
    v_texcoord0 = vec2(a_texcoord0.x, 1.0 - a_texcoord0.y);
    v_camera_position = camera.position.xyz;

    v_tangent = normalize(normal_matrix * vec4(a_tangent, 0.0)).xyz;
    v_bitangent = normalize(normal_matrix * vec4(a_bitangent, 0.0)).xyz;
    v_tbn_matrix = mat3(v_tangent, v_bitangent, v_normal);

    v_position_ndc = camera.projection * camera.view * position;
    v_previous_position_ndc = camera.projection * camera.previous_view * previous_position;

    gl_Position = v_position_ndc;
}
