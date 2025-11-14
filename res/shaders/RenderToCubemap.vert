#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_separate_shader_objects : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_multiview : require

layout(location = 0) out vec3 v_position;
layout(location = 1) out vec3 v_normal;
layout(location = 2) out vec2 v_texcoord0;
layout(location = 7) out flat vec3 v_camera_position;
layout(location = 11) out flat uint v_object_index;
layout(location = 13) out flat uint v_cube_face_index;

HYP_ATTRIBUTE(0) vec3 a_position;
HYP_ATTRIBUTE(1) vec3 a_normal;
HYP_ATTRIBUTE(2) vec2 a_texcoord0;
HYP_ATTRIBUTE(3) vec2 a_texcoord1;
HYP_ATTRIBUTE(4) vec3 a_tangent;
HYP_ATTRIBUTE(5) vec3 a_bitangent;
HYP_ATTRIBUTE_OPTIONAL(6) vec4 a_bone_weights;
HYP_ATTRIBUTE_OPTIONAL(7) vec4 a_bone_indices;

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "include/scene.inc"

#include "include/Entity.glsl"

#ifdef SKINNING
#include "include/Skeleton.glsl"
#endif

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

HYP_DESCRIPTOR_CBUFF_DYNAMIC(Global, CamerasBuffer) uniform CameraShaderData
{
    Camera camera;
};

#ifdef ENV_PROBE

#include "include/env_probe.inc"

HYP_DESCRIPTOR_SSBO_DYNAMIC(Global, CurrentEnvProbe) readonly buffer CurrentEnvProbe
{
    EnvProbe current_env_probe;
};

#endif

#ifdef INSTANCING

HYP_DESCRIPTOR_SSBO(Global, EntitiesBuffer) readonly buffer EntitiesBuffer
{
    Entity entities[];
};

HYP_DESCRIPTOR_SSBO_DYNAMIC(Instancing, EntityInstanceBatchesBuffer) readonly buffer EntityInstanceBatchesBuffer
{
    EntityInstanceBatch entity_instance_batch;
};

#else

HYP_DESCRIPTOR_SSBO_DYNAMIC(Entity, CurrentEntity) readonly buffer EntitiesBuffer
{
    Entity entity;
};

#endif

// pairs of cubemap forward direction and up direction (interleaved order)
const vec3 cubemap_directions[12] = vec3[](
    vec3(1.0, 0.0, 0.0), vec3(0.0, 1.0, 0.0),
    vec3(-1.0, 0.0, 0.0), vec3(0.0, 1.0, 0.0),
    vec3(0.0, 1.0, 0.0), vec3(0.0, 0.0, -1.0),
    vec3(0.0, -1.0, 0.0), vec3(0.0, 0.0, 1.0),
    vec3(0.0, 0.0, 1.0), vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, -1.0), vec3(0.0, 1.0, 0.0));

mat4 LookAt(vec3 pos, vec3 target, vec3 up)
{
    mat4 lookat;

    vec3 dir = normalize(target - pos);

    vec3 zaxis = dir;
    vec3 xaxis = normalize(cross(dir, up));
    vec3 yaxis = cross(xaxis, zaxis);

    lookat[0] = vec4(xaxis, pos.x);
    lookat[1] = vec4(yaxis, pos.y);
    lookat[2] = vec4(zaxis, pos.z);
    lookat[3] = vec4(0.0, 0.0, 0.0, 1.0);
    lookat = transpose(lookat);

    return lookat;
}

#ifdef SKINNING

HYP_DESCRIPTOR_SSBO_DYNAMIC(Entity, SkeletonsBuffer) readonly buffer SkeletonsBuffer
{
    Skeleton skeleton;
};

mat4 CreateSkinningMatrix(ivec4 bone_indices, vec4 bone_weights)
{
    mat4 skinning = mat4(0.0);

    int index0 = min(bone_indices.x, HYP_MAX_BONES - 1);
    skinning += bone_weights.x * skeleton.bones[index0];
    int index1 = min(bone_indices.y, HYP_MAX_BONES - 1);
    skinning += bone_weights.y * skeleton.bones[index1];
    int index2 = min(bone_indices.z, HYP_MAX_BONES - 1);
    skinning += bone_weights.z * skeleton.bones[index2];
    int index3 = min(bone_indices.w, HYP_MAX_BONES - 1);
    skinning += bone_weights.w * skeleton.bones[index3];

    return skinning;
}

#endif

void main()
{
    vec4 position;
    mat4 normal_matrix;

#if defined(SKINNING) && defined(HYP_ATTRIBUTE_a_bone_weights) && defined(HYP_ATTRIBUTE_a_bone_indices)
    mat4 skinning_matrix = CreateSkinningMatrix(ivec4(a_bone_indices), a_bone_weights);

    position = entity.model_matrix * skinning_matrix * vec4(a_position, 1.0);
    normal_matrix = transpose(inverse(entity.model_matrix * skinning_matrix));
#else
    position = entity.model_matrix * vec4(a_position, 1.0);
    normal_matrix = transpose(inverse(entity.model_matrix));
#endif

    v_position = position.xyz;
    v_normal = (normal_matrix * vec4(a_normal, 0.0)).xyz;
    v_texcoord0 = vec2(a_texcoord0.x, 1.0 - a_texcoord0.y);

    const vec3 forward_direction = cubemap_directions[gl_ViewIndex * 2];
    const vec3 up_direction = cubemap_directions[gl_ViewIndex * 2 + 1];

    mat4 projection_matrix = camera.projection;

#if ENV_PROBE
    v_camera_position = current_env_probe.world_position.xyz;
    mat4 view_matrix = LookAt(v_camera_position, v_camera_position + forward_direction, up_direction);
#else
    v_camera_position = camera.position.xyz;
    mat4 view_matrix = camera.view;
#endif


#ifdef INSTANCING
    v_object_index = OBJECT_INDEX;
#endif

    gl_Position = projection_matrix * view_matrix * position;

    v_cube_face_index = gl_ViewIndex;
}
