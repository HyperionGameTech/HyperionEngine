#include "./include/shared.inc"
#include "./include/scene.inc"

struct VSInput
{
    HYP_ATTRIBUTE float3 a_position : POSITION;
    HYP_ATTRIBUTE float3 a_normal : NORMAL;
    HYP_ATTRIBUTE float2 a_texcoord0 : TEXCOORD0;
    HYP_ATTRIBUTE_OPTIONAL float2 a_texcoord1 : TEXCOORD1;
    HYP_ATTRIBUTE_OPTIONAL uint a_bone_indices : BLENDINDICES;
    HYP_ATTRIBUTE_OPTIONAL float4 a_bone_weights : BLENDWEIGHT;
};

struct VSOutput
{
    float4 position_cs : SV_POSITION;
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texcoord0 : TEXCOORD0;
    float2 texcoord1 : TEXCOORD1;
    float3 tangent : TANGENT;
    float3 bitangent : BINORMAL;
    nointerpolation float3 camera_position : TEXCOORD3;
    float4 position_ndc : TEXCOORD4;
    float4 previous_position_ndc : TEXCOORD5;
    nointerpolation uint object_index : TEXCOORD6;
    nointerpolation uint object_mask : TEXCOORD7;
};

DECLARE_BUFFER_DYNAMIC(Default, CamerasBuffer) cbuffer CamerasBuffer
{
    Camera camera;
};

#include "include/Entity.inc"

#ifdef INSTANCING
    DECLARE_SRV(Default, EntitiesBuffer) StructuredBuffer<Entity> entities;
    DECLARE_SRV_DYNAMIC(Default, EntityInstanceBatchesBuffer) StructuredBuffer<MeshEntityInstanceBatch> entity_instance_batches;

    #define entity_instance_batch entity_instance_batches[0]
#else
    DECLARE_SRV_DYNAMIC(Default, CurrentEntity) StructuredBuffer<Entity> entities;
#endif

#ifdef SKINNING

#include "include/Skeleton.inc"
DECLARE_SRV_DYNAMIC(Default, SkeletonsBuffer) StructuredBuffer<Skeleton> skeletons;

#endif

VSOutput VSMain(VSInput input, uint instanceId : SV_InstanceID)
{
    VSOutput output;
    
    float4 position;
    float4 previous_position;

#ifdef INSTANCING
    Entity currentEntity = entities[instanceId];
    float4x4 model_matrix = mul(entity_instance_batch.transforms[instanceId], currentEntity.model_matrix);
    float3x3 normal_matrix = transpose(inverse((float3x3)model_matrix));
#else
    Entity currentEntity = entity;
    float4x4 model_matrix = entity.model_matrix;
    float3x3 normal_matrix = transpose(inverse((float3x3)model_matrix));//(float3x3)entity.normal_matrix;
#endif

#if defined(SKINNING) && defined(VT_Skeletal)
    float4x4 skinning_matrix = CreateSkinningMatrix(skeletons[0], input.a_bone_indices, input.a_bone_weights);

    position = mul(model_matrix, mul(skinning_matrix, float4(input.a_position, 1.0)));
    previous_position = mul(currentEntity.previous_model_matrix, mul(skinning_matrix, float4(input.a_position, 1.0)));
    normal_matrix = mul(normal_matrix, (float3x3)skinning_matrix);
#else
    position = mul(model_matrix, float4(input.a_position, 1.0));

#ifdef INSTANCING
    previous_position = mul(mul(entity_instance_batch.previousTransforms[instanceId], currentEntity.previous_model_matrix), float4(input.a_position, 1.0));
#else
    previous_position = mul(currentEntity.previous_model_matrix, float4(input.a_position, 1.0));
#endif
#endif

    output.position = position.xyz / position.w;
    output.normal = mul(normal_matrix, input.a_normal);
    output.texcoord0 = float2(input.a_texcoord0.x, 1.0 - input.a_texcoord0.y);
    output.camera_position = camera.position.xyz;

#ifdef VT_UV1
    output.texcoord1 = float2(input.a_texcoord1.x, 1.0-input.a_texcoord1.y);
#else
    output.texcoord1 = float2(0.0, 0.0);
#endif

    float3 tangent;
    float3 bitangent;
    ComputeOrthonormalBasis(input.a_normal, tangent, bitangent);

    output.tangent = mul(normal_matrix, tangent);
    output.bitangent = mul(normal_matrix, bitangent);

    // ViewProjection
    output.position_ndc = mul(camera.viewProjMat, position);
    output.previous_position_ndc = mul(camera.prevViewProjMat, previous_position);

    // Jitter
    float4x4 jitterMat = { 
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1 
    };
    jitterMat[0][3] += camera.jitter.x;
    jitterMat[1][3] += camera.jitter.y;

    output.position_cs = mul(jitterMat, output.position_ndc);

#ifdef INSTANCING
    output.object_index = OBJECT_INDEX;
#else
    output.object_index = ~0u; // unused
#endif

    output.object_mask = GET_OBJECT_BUCKET_MASK(currentEntity);

    return output;
}