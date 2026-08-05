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
static const uint s_offsetOfTransforms = s_offsetOfIndices + (sizeof(uint4) * (MAX_ENTITIES_PER_INSTANCE_BATCH >> 2));
static const uint s_offsetOfPrevTransforms = s_offsetOfTransforms + (sizeof(float4x4) * MAX_ENTITIES_PER_INSTANCE_BATCH);

// ByteAddressBuffer.Load<float4x4> comes back transposed relative to the row-major Mat4f uploaded from the
// CPU (this is a DXC/SPIR-V quirk, not affected by pack_matrix(row_major)). A transposed affine matrix moves
// translation out of the last column and into the last row, which corrupts the homogeneous w of every vertex
// by an amount that depends on that vertex's local position -- i.e. non-rigid shear that scales with how far
// the instance is translated. Load the four rows individually as float4 (no orientation ambiguity) instead.
float4x4 LoadInstanceTransform(uint offset)
{
    return float4x4(
        EntityInstanceBatchBuffer.Load<float4>(offset + 0),
        EntityInstanceBatchBuffer.Load<float4>(offset + 16),
        EntityInstanceBatchBuffer.Load<float4>(offset + 32),
        EntityInstanceBatchBuffer.Load<float4>(offset + 48));
}
#endif // INSTANCING

VSOutput VSMain(VSInput input, uint instanceId : SV_InstanceID)
{
    VSOutput output;

    float4 position;
    float4 previous_position;

#ifdef INSTANCING
    float4x4 transform = LoadInstanceTransform(s_offsetOfTransforms + (sizeof(float4x4) * instanceId));

    const uint entityIndex = EntityInstanceBatchBuffer.Load<uint>(s_offsetOfIndices + (instanceId * sizeof(uint)));
    output.object_index = entityIndex;

    Entity currentEntity = entities[entityIndex];
    float4x4 model_matrix = mul(currentEntity.model_matrix, transform);
    float3x3 normal_matrix = (float3x3)currentEntity.normal_matrix;//transpose(inverse((float3x3)model_matrix));
#else // !INSTANCING
    output.object_index = ~0u; // unused

#define currentEntity entity

    float4x4 model_matrix = entity.model_matrix;
    float3x3 normal_matrix = (float3x3)entity.normal_matrix;//transpose(inverse((float3x3)model_matrix));//
#endif // INSTANCING

#if defined(SKINNING) && defined(VT_Skeletal)
    float4x4 skinning_matrix = CreateSkinningMatrix(input.a_bone_indices, input.a_bone_weights);

    position = mul(model_matrix, mul(skinning_matrix, float4(input.a_position, 1.0)));

#ifdef INSTANCING
    // Must include the per-instance previousTransform here too, same as the non-skinned path below --
    // otherwise this is computed as if the instance had zero offset from the entity, so the reported
    // velocity is dominated by the IMP's static offset instead of actual frame-to-frame motion.
    const float4x4 previousTransform = LoadInstanceTransform(s_offsetOfPrevTransforms + (sizeof(float4x4) * instanceId));
    float4x4 previous_model_matrix = mul(currentEntity.previous_model_matrix, previousTransform);
#else // !INSTANCING
    float4x4 previous_model_matrix = currentEntity.previous_model_matrix;
#endif // INSTANCING

    previous_position = mul(previous_model_matrix, mul(skinning_matrix, float4(input.a_position, 1.0)));
    normal_matrix = mul(normal_matrix, (float3x3)skinning_matrix);
#else // !SKINNING || !VT_Skeletal
    position = mul(model_matrix, float4(input.a_position, 1.0));

#ifdef INSTANCING
    const float4x4 previousTransform = LoadInstanceTransform(s_offsetOfPrevTransforms + (sizeof(float4x4) * instanceId));

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
    output.texcoord1 = float2(input.a_texcoord1.x, 1.0 - input.a_texcoord1.y);
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

    output.position_cs = output.position_ndc;
    output.position_cs.xy += camera.jitter.xy * output.position_cs.w;

    output.color = material.albedo;

    output.object_mask = ((uint)(currentEntity.bucket == HYP_OBJECT_BUCKET_LIGHTMAPPED) * OBJECT_MASK_LIGHTMAPPED)
        | (min(1u, GET_MATERIAL_PARAM_BIT(material, 0)) * OBJECT_MASK_UNLIT);

#ifndef INSTANCING
#undef currentEntity
#endif // INSTANCING

    return output;
}
