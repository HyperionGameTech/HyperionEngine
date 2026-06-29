#include "./include/Shared.hlsli"
#include "./include/Scene.hlsli"
#include "./include/Material.hlsli"

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
    float4 color : TEXCOORD2;
    nointerpolation float3 camera_position : TEXCOORD3;
    float4 position_ndc : TEXCOORD4;
    float4 previous_position_ndc : TEXCOORD5;
    nointerpolation uint object_index : TEXCOORD6;
    nointerpolation uint object_mask : TEXCOORD7;
};

#include "include/Entity.hlsli"

#ifdef INSTANCING
DECLARE_SRV(Default, EntitiesBuffer) StructuredBuffer<Entity> entities;
DECLARE_SRV_DYNAMIC(Default, EntityInstanceBatchesBuffer) ByteAddressBuffer EntityInstanceBatchBuffer;
#endif // INSTANCING

#ifdef SKINNING
DECLARE_SRV_DYNAMIC(Default, SkeletonsBuffer) StructuredBuffer<float4x4> SkeletonsBuffer;
#include "include/Skinning.hlsli"
#endif // SKINNING

DECLARE_BUFFER_DYNAMIC(Default, CBuffer) cbuffer CBuffer
{
#ifndef INSTANCING
    Entity entity;
#else // INSTANCING
    Entity dummyEntity;
#endif // !INSTANCING
    Camera camera;
    Material material;
};

#ifdef INSTANCING
static const uint s_offsetOfIndices = 64; // 64 bytes after header start
static const uint s_offsetOfTransforms = s_offsetOfIndices + (sizeof(uint4) * (MAX_ENTITIES_PER_INSTANCE_BATCH / 4));
static const uint s_offsetOfPrevTransforms = s_offsetOfTransforms + (sizeof(float4x4) * MAX_ENTITIES_PER_INSTANCE_BATCH);
#endif // INSTANCING

VSOutput VSMain(VSInput input, uint instanceId : SV_InstanceID)
{
    VSOutput output;

    float4 position;
    float4 previous_position;

#ifdef INSTANCING
    float4x4 transform = EntityInstanceBatchBuffer.Load<float4x4>(s_offsetOfTransforms + (sizeof(float4x4) * instanceId));
#ifdef VULKAN
    transform = transpose(transform);
#endif // VULKAN

    const uint entityIndex = EntityInstanceBatchBuffer.Load<uint>(s_offsetOfIndices + (instanceId * sizeof(uint)));

    Entity currentEntity = entities[entityIndex];
    float4x4 model_matrix = mul(currentEntity.model_matrix, transform);
    float3x3 normal_matrix = (float3x3)currentEntity.normal_matrix;//transpose(inverse((float3x3)model_matrix));
#else // !INSTANCING

#define currentEntity entity

    float4x4 model_matrix = entity.model_matrix;
    float3x3 normal_matrix = (float3x3)entity.normal_matrix;//transpose(inverse((float3x3)model_matrix));//
#endif // INSTANCING

#if defined(SKINNING) && defined(VT_Skeletal)
    float4x4 skinning_matrix = CreateSkinningMatrix(input.a_bone_indices, input.a_bone_weights);

    position = mul(model_matrix, mul(skinning_matrix, float4(input.a_position, 1.0)));
    previous_position = mul(currentEntity.previous_model_matrix, mul(skinning_matrix, float4(input.a_position, 1.0)));
    normal_matrix = mul(normal_matrix, (float3x3)skinning_matrix);
#else // !SKINNING || !VT_Skeletal
    position = mul(model_matrix, float4(input.a_position, 1.0));

#ifdef INSTANCING
    const float4x4 previousTransform = EntityInstanceBatchBuffer.Load<float4x4>(s_offsetOfPrevTransforms + (sizeof(float4x4) * instanceId));

    previous_position = mul(mul(currentEntity.previous_model_matrix, previousTransform), float4(input.a_position, 1.0));
#else // !INSTANCING
    previous_position = mul(currentEntity.previous_model_matrix, float4(input.a_position, 1.0));
#endif // !SKINNING || !VT_Skeletal
#endif // SKINNING && VT_Skeletal

    output.position = position.xyz / position.w;
    output.normal = mul(normal_matrix, input.a_normal);
    output.texcoord0 = float2(input.a_texcoord0.x, 1.0 - input.a_texcoord0.y);
    output.camera_position = camera.position.xyz;

#ifdef VT_UV1
    output.texcoord1 = float2(input.a_texcoord1.x, 1.0-input.a_texcoord1.y);
#else // !VT_UV1
    output.texcoord1 = float2(0.0, 0.0);
#endif // VT_UV1

    float3 tangent;
    float3 bitangent;
    ComputeOrthonormalBasis(input.a_normal, tangent, bitangent);

    output.tangent = mul(normal_matrix, tangent);
    output.bitangent = mul(normal_matrix, bitangent);

    // ViewProjection
    output.position_ndc = mul(camera.viewProjMat, position);
    output.previous_position_ndc = mul(camera.prevViewProjMat, previous_position);

    // // Jitter
    // float4x4 jitterMat = {
    //     1, 0, 0, 0,
    //     0, 1, 0, 0,
    //     0, 0, 1, 0,
    //     0, 0, 0, 1
    // };
    // jitterMat[0][3] += camera.jitter.x;
    // jitterMat[1][3] += camera.jitter.y;

    // output.position_cs = mul(jitterMat, output.position_ndc);

    output.position_cs = output.position_ndc;
    output.position_cs.xy += camera.jitter.xy * output.position_cs.w;

    output.color = material.albedo;

#ifdef INSTANCING
    output.object_index = OBJECT_INDEX;
#else // !INSTANCING
    output.object_index = ~0u; // unused
#endif // INSTANCING

    output.object_mask = (currentEntity.bucket == HYP_OBJECT_BUCKET_LIGHTMAPPED)
        ? OBJECT_MASK_LIGHTMAPPED
        : 0u;

#ifndef INSTANCING
#undef currentEntity
#endif // INSTANCING

    return output;
}
