#include "include/defines.inc"
#include "include/shared.inc"
#include "include/scene.inc"

struct VSInput
{
    HYP_ATTRIBUTE(0) float3 a_position : POSITION;
    HYP_ATTRIBUTE(1) float3 a_normal : NORMAL;
    HYP_ATTRIBUTE(2) float2 a_texcoord0 : TEXCOORD0;
    HYP_ATTRIBUTE_OPTIONAL(3) float2 a_texcoord1 : TEXCOORD1;
    HYP_ATTRIBUTE_OPTIONAL(4) float3 a_tangent : TANGENT;
    HYP_ATTRIBUTE_OPTIONAL(5) float3 a_bitangent : BINORMAL;
    HYP_ATTRIBUTE_OPTIONAL(6) float4 a_bone_weights : BLENDWEIGHT;
    HYP_ATTRIBUTE_OPTIONAL(7) float4 a_bone_indices : BLENDINDICES;
};

struct VSOutput
{
    float4 position_cs : SV_POSITION;
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texcoord0 : TEXCOORD0;
    nointerpolation float3 camera_position : TEXCOORD3;
    nointerpolation uint object_index : TEXCOORD6;
    nointerpolation uint cube_face_index : TEXCOORD7;
};

HYP_DESCRIPTOR_BUFFER_DYNAMIC(Default, CamerasBuffer) cbuffer CamerasBuffer
{
    Camera camera;
};

#ifdef ENV_PROBE

#include "include/env_probe.inc"

HYP_DESCRIPTOR_SRV_DYNAMIC(Default, CurrentEnvProbe) StructuredBuffer<EnvProbe> current_env_probe_buffer;
#define current_env_probe current_env_probe_buffer[0]

#endif

#include "include/Entity.inc"

#ifdef INSTANCING
    HYP_DESCRIPTOR_SRV(Default, EntitiesBuffer) StructuredBuffer<Entity> entities;
    HYP_DESCRIPTOR_SRV_DYNAMIC(Default, EntityInstanceBatchesBuffer) StructuredBuffer<EntityInstanceBatch> entity_instance_batches;

    #define entity_instance_batch entity_instance_batches[0]
#else
    HYP_DESCRIPTOR_SRV_DYNAMIC(Default, CurrentEntity) StructuredBuffer<Entity> entities;
    #define entity entities[0]
#endif

#ifdef SKINNING
#include "include/Skeleton.glsl"

HYP_DESCRIPTOR_SRV_DYNAMIC(Default, SkeletonsBuffer) StructuredBuffer<Skeleton> skeletons;

float4x4 CreateSkinningMatrix(int4 bone_indices, float4 bone_weights)
{
    float4x4 skinning = (float4x4)0;

    int index0 = min(bone_indices.x, HYP_MAX_BONES - 1);
    skinning += bone_weights.x * skeletons[0].bones[index0];
    int index1 = min(bone_indices.y, HYP_MAX_BONES - 1);
    skinning += bone_weights.y * skeletons[0].bones[index1];
    int index2 = min(bone_indices.z, HYP_MAX_BONES - 1);
    skinning += bone_weights.z * skeletons[0].bones[index2];
    int index3 = min(bone_indices.w, HYP_MAX_BONES - 1);
    skinning += bone_weights.w * skeletons[0].bones[index3];

    return skinning;
}
#endif

float4x4 LookAt(float3 pos, float3 target, float3 up)
{
    float3 f = normalize(pos - target);
    float3 s = normalize(cross(f, up));
    float3 u = cross(s, f);

    return float4x4(
        s.x, s.y, s.z, -dot(s, pos),
        u.x, u.y, u.z, -dot(u, pos),
        -f.x, -f.y, -f.z, dot(f, pos),
        0.0, 0.0, 0.0, 1.0
    );
}

VSOutput VSMain(VSInput input, uint ViewId : SV_ViewID, uint instanceId : SV_InstanceID)
{
    VSOutput output;
    
    float4 position;
    float4x4 normal_matrix;

#ifdef INSTANCING
    Entity currentEntity = entities[instanceId];
    float4x4 model_matrix = mul(entity_instance_batch.transforms[instanceId], currentEntity.model_matrix);
#else
    Entity currentEntity = entity;
    float4x4 model_matrix = entity.model_matrix;
#endif

#if defined(SKINNING) && defined(HYP_ATTRIBUTE_a_bone_indices) && defined(HYP_ATTRIBUTE_a_bone_weights)
    float4x4 skinning_matrix = CreateSkinningMatrix((int4)input.a_bone_indices, input.a_bone_weights);

    position = mul(model_matrix, mul(skinning_matrix, float4(input.a_position, 1.0)));
    float4x4 skin_model = mul(model_matrix, skinning_matrix);
    normal_matrix = skin_model;
#else
    position = mul(model_matrix, float4(input.a_position, 1.0));
    normal_matrix = model_matrix;
#endif

    output.position = position.xyz / position.w;
    output.normal = mul((float3x3)normal_matrix, input.a_normal);
    output.texcoord0 = float2(input.a_texcoord0.x, 1.0 - input.a_texcoord0.y);

    const float3 forward_direction = g_cubemapDirections[ViewId * 2];
    const float3 up_direction = g_cubemapDirections[ViewId * 2 + 1];

    float4x4 projection_matrix = camera.projection;

    float4x4 view_matrix;

#if ENV_PROBE
    output.camera_position = current_env_probe.world_position.xyz;
    view_matrix = current_env_probe.face_view_matrices[ViewId];
#else
    output.camera_position = camera.position.xyz;
    view_matrix = LookAt(output.camera_position, output.camera_position + forward_direction, up_direction);
#endif

#ifdef INSTANCING
    output.object_index = OBJECT_INDEX;
#endif

    output.position_cs = mul(projection_matrix, mul(view_matrix, position));

    output.cube_face_index = ViewId;

    return output;
}

