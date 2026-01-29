#version 450

#define OUTLINE_WIDTH 1.15

#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

layout(location = 0) out vec3 v_position;
layout(location = 1) out vec3 v_normal;
layout(location = 2) out vec2 v_texcoord0;
layout(location = 4) out vec3 v_tangent;
layout(location = 5) out vec3 v_bitangent;
layout(location = 7) out flat vec3 v_camera_position
layout(location = 12) out vec3 v_view_space_position;
layout(location = 13) out uint v_object_index;

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_texcoord0;
layout(location = 3) in vec2 a_texcoord1;
layout(location = 4) in vec3 a_tangent;
layout(location = 5) in vec3 a_bitangent;
layout(location = 6) in vec4 a_bone_weights;
layout(location = 7) in vec4 a_bone_indices;

#include "include/scene.inc"

#include "include/Entity.inc"

#include "include/Skeleton.inc"

void main()
{
    vec4 position;
    mat4 normal_matrix;

    // extrude along normal
    vec4 extendedPosition = vec4(a_position + a_normal * OUTLINE_WIDTH, 1.0);

#ifdef SKINNING
    mat4 skinning_matrix = CreateSkinningMatrix(ivec4(a_bone_indices), a_bone_weights);

    position = entity.model_matrix * skinning_matrix * extendedPosition;
    normal_matrix = transpose(inverse(entity.model_matrix * skinning_matrix));
#else
    position = entity.model_matrix * extendedPosition;
    normal_matrix = transpose(inverse(entity.model_matrix));
#endif

    v_position = position.xyz;
    v_normal = (normal_matrix * vec4(a_normal, 0.0)).xyz;
    v_texcoord0 = vec2(a_texcoord0.x, 1.0 - a_texcoord0.y);
    v_camera_position = camera.position.xyz;

    v_tangent = normalize(normal_matrix * vec4(a_tangent, 0.0)).xyz;
    v_bitangent = normalize(normal_matrix * vec4(a_bitangent, 0.0)).xyz;

    v_view_space_position = (camera.view * position).xyz;

#ifdef INSTANCING
    v_object_index = OBJECT_INDEX;
#endif

    gl_Position = camera.viewProjMat * position;
}
