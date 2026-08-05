/**
 *  Author: Andrew J. MacDonald
 *  Date: 2026/03/27
 **/

#include "./include/Defines.hlsli"
#include "./include/Shared.hlsli"
#include "./include/Scene.hlsli"
#include "./include/Material.hlsli"

PERMUTE(INSTANCING);
PERMUTE(SKINNING);

#ifdef VERTEX_SHADER

struct VSInput
{
    HYP_ATTRIBUTE_OPTIONAL float3 a_position : POSITION;
    HYP_ATTRIBUTE_OPTIONAL float3 a_normal : NORMAL;
    HYP_ATTRIBUTE_OPTIONAL float2 a_texcoord0 : TEXCOORD0;
    HYP_ATTRIBUTE_OPTIONAL float2 a_texcoord1 : TEXCOORD1;
    HYP_ATTRIBUTE_OPTIONAL uint a_bone_indices : BLENDINDICES;
    HYP_ATTRIBUTE_OPTIONAL float4 a_bone_weights : BLENDWEIGHT;
};

struct VSOutput
{
    float4 position_cs : SV_POSITION;
    float3 v_position : POSITION;
    float2 v_texcoord0 : TEXCOORD0;
    nointerpolation uint object_index : TEXCOORD6;
};

DECLARE_SRV_DYNAMIC(Default, CamerasBuffer) StructuredBuffer<Camera> _cameras_buffer;
#define camera _cameras_buffer[0]

#include "include/Entity.hlsli"

#ifdef INSTANCING
DECLARE_SRV(Default, EntitiesBuffer) StructuredBuffer<Entity> entities;
DECLARE_SRV_DYNAMIC(Default, EntityInstanceBatchesBuffer) ByteAddressBuffer EntityInstanceBatchBuffer;
#endif // INSTANCING

#if defined(SKINNING) && defined(HYP_ATTRIBUTE_a_bone_indices) && defined(HYP_ATTRIBUTE_a_bone_weights) && defined(HYP_ATTRIBUTE_a_position)
DECLARE_SRV_DYNAMIC(Default, SkeletonsBuffer) StructuredBuffer<float4x4> SkeletonsBuffer;
#include "include/Skinning.hlsli"
#endif // SKINNING && bone attrs

#ifndef INSTANCING
DECLARE_BUFFER_DYNAMIC(Default, CBuffer) cbuffer CBuffer
{
    Entity entity;
};
#endif // !INSTANCING

VSOutput VSMain(VSInput input, uint instanceId : SV_InstanceID)
{
    VSOutput output = (VSOutput)0;

#ifdef INSTANCING
    MeshEntityInstanceBatch batch = EntityInstanceBatchBuffer.Load<MeshEntityInstanceBatch>(0);

    const uint objectIndex = OBJECT_INDEX;
    const uint dataOffset = OBJECT_DATA_OFFSET;

    float4x4 transform = batch.transforms[dataOffset];
#ifdef VULKAN
    transform = transpose(transform);
#endif

    float4x4 model_matrix = mul(transform, entities[objectIndex].model_matrix);
    output.object_index = objectIndex;
#else
    float4x4 model_matrix = entity.model_matrix;
    output.object_index  = 0;
#endif

#ifdef HYP_ATTRIBUTE_a_position
    float4 position = float4(input.a_position, 1.0);

#if defined(SKINNING) && defined(HYP_ATTRIBUTE_a_bone_indices) && defined(HYP_ATTRIBUTE_a_bone_weights)
    float4x4 skinning_matrix = CreateSkinningMatrix(input.a_bone_indices, input.a_bone_weights);
    position = mul(skinning_matrix, position);
#endif

    position = mul(model_matrix, position);
#else
    // No position attribute, emit degenerate vertex
    float4 position = float4(0.0, 0.0, 0.0, 1.0);
#endif

    output.v_position  = position.xyz / position.w;
    output.position_cs = mul(camera.viewProjMat, position);

#ifdef HYP_ATTRIBUTE_a_texcoord0
    output.v_texcoord0 = float2(input.a_texcoord0.x, 1.0 - input.a_texcoord0.y);
#endif

    return output;
}

#endif // VERTEX_SHADER

#ifdef PIXEL_SHADER

struct PSInput
{
    float4 position_cs : SV_POSITION;
    float3 v_position : POSITION;
    float2 v_texcoord0 : TEXCOORD0;
    nointerpolation uint object_index : TEXCOORD6;
};

struct PSOutput
{
    float4 color : SV_Target0;
};

PSOutput PSMain(PSInput input)
{
    PSOutput output;

    // Emit some crap to show that this is not meant to be here
#ifdef HYP_ATTRIBUTE_a_texcoord0
    const float2 tile = floor(input.v_texcoord0 * 8.0);
    const float  checker = fmod(tile.x + tile.y, 2.0);
    output.color = lerp(float4(1.0, 0.0, 1.0, 1.0), float4(0.1, 0.0, 0.1, 1.0), checker);
#else
    output.color = float4(1.0, 0.0, 1.0, 1.0);
#endif

    return output;
}

#endif // PIXEL_SHADER
