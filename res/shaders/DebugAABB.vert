#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_scalar_block_layout : require

layout(location = 0) out vec3 v_position;
layout(location = 1) out vec3 v_normal;
layout(location = 2) out vec2 v_texcoord0;
layout(location = 4) out vec3 v_tangent;
layout(location = 5) out vec3 v_bitangent;
layout(location = 11) out vec4 v_position_ndc;
layout(location = 12) out vec4 v_previous_position_ndc;
layout(location = 15) out flat uint v_object_index;

#ifdef IMMEDIATE_MODE
layout(location = 16) out vec4 v_color;
layout(location = 17) out flat uint v_env_probe_index;
layout(location = 18) out flat uint v_env_probe_type;
#endif

HYP_ATTRIBUTE(0) vec3 a_position;
HYP_ATTRIBUTE(1) vec3 a_normal;
HYP_ATTRIBUTE(2) vec2 a_texcoord0;
HYP_ATTRIBUTE(3) vec3 a_texcoord1;
HYP_ATTRIBUTE(4) vec3 a_tangent;
HYP_ATTRIBUTE(5) vec3 a_bitangent;
HYP_ATTRIBUTE_OPTIONAL(6) vec4 a_bone_weights;
HYP_ATTRIBUTE_OPTIONAL(7) vec4 a_bone_indices;

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "include/env_probe.inc"
#include "include/scene.inc"

#ifdef IMMEDIATE_MODE
HYP_DESCRIPTOR_SSBO(Global, EnvProbesBuffer) readonly buffer EnvProbesBuffer
{
    EnvProbe env_probes[];
};

HYP_DESCRIPTOR_SRV(Global, LightFieldColorTexture) uniform texture2D light_field_color_texture;
HYP_DESCRIPTOR_SRV(Global, LightFieldDepthTexture) uniform texture2D light_field_depth_texture;

struct ImmediateDraw
{
    mat4 model_matrix;

    uint color_packed;
    uint env_probe_type;
    uint env_probe_index;
    uint _pad2;
};

HYP_DESCRIPTOR_SSBO_DYNAMIC(DebugDrawerDescriptorSet, ImmediateDrawsBuffer, standard = scalar) readonly buffer ImmediateDrawsBuffer
{
    ImmediateDraw immediateDraws[];
};

#define MODEL_MATRIX (immediateDraw.model_matrix)
#define PREV_MODEL_MATRIX (immediateDraw.model_matrix)
#else
#include "include/Entity.glsl"

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

#define MODEL_MATRIX (entity.model_matrix)
#define PREV_MODEL_MATRIX (entity.previous_model_matrix)
#endif

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

HYP_DESCRIPTOR_CBUFF_DYNAMIC(Global, CamerasBuffer) uniform CamerasBuffer
{
    Camera camera;
};

void main()
{
#ifdef IMMEDIATE_MODE
    ImmediateDraw immediateDraw = immediateDraws[gl_InstanceIndex];
#endif

    vec4 position = MODEL_MATRIX * vec4(a_position, 1.0);
    vec4 previous_position = PREV_MODEL_MATRIX * vec4(a_position, 1.0);

    mat4 normal_matrix = transpose(inverse(MODEL_MATRIX));

    v_position = position.xyz;
    v_normal = (normal_matrix * vec4(a_normal, 0.0)).xyz;
    v_tangent = (normal_matrix * vec4(a_tangent, 0.0)).xyz;
    v_bitangent = (normal_matrix * vec4(a_bitangent, 0.0)).xyz;
    v_texcoord0 = a_texcoord0;

#ifdef IMMEDIATE_MODE
    v_object_index = ~0u; // unused
    v_color = UINT_TO_VEC4(immediateDraw.color_packed);

    v_env_probe_type = immediateDraw.env_probe_type;
    v_env_probe_index = immediateDraw.env_probe_index;

    if (immediateDraw.env_probe_index != ~0u && immediateDraw.env_probe_type == ENV_PROBE_TYPE_AMBIENT)
    {
        SH9 sh9;

        for (int i = 0; i < 9; i++)
        {
            sh9.values[i] = env_probes[immediateDraw.env_probe_index].sh[i].rgb;
        }

        v_color = vec4(SphericalHarmonicsSample(sh9, a_normal), 1.0);
    }
#elif defined(INSTANCING)
    v_object_index = OBJECT_INDEX;
#else
    v_object_index = 0;
#endif

    mat4 jitter_matrix = mat4(1.0);
    jitter_matrix[3][0] += camera.jitter.x;
    jitter_matrix[3][1] += camera.jitter.y;

    v_position_ndc = (jitter_matrix * camera.projection) * camera.view * position;
    v_previous_position_ndc = (jitter_matrix * camera.projection) * camera.previous_view * previous_position;

    gl_Position = camera.projection * camera.view * position;
}