#include "include/defines.inc"
#include "include/shared.inc"

DECLARE_SAMPLER(Default, SamplerLinear) SamplerState sampler_linear;
DECLARE_SAMPLER(Default, SamplerNearest) SamplerState sampler_nearest;

#define texture_sampler sampler_linear

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "include/scene.inc"
#include "include/Entity.inc"
#include "include/material.inc"
#include "include/packing.inc"

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

DECLARE_BUFFER_DYNAMIC(Default, CamerasBuffer) cbuffer CamerasBuffer
{
    Camera camera;
};

#ifdef INSTANCING
DECLARE_SRV(Default, EntitiesBuffer) StructuredBuffer<Entity> entities;
DECLARE_SRV_DYNAMIC(Default, EntityInstanceBatchesBuffer) StructuredBuffer<EntityInstanceBatch> entity_instance_batches;
#define entity_instance_batch entity_instance_batches[0]
#else
DECLARE_SRV_DYNAMIC(Default, CurrentEntity) StructuredBuffer<Entity> entities;
#endif

DECLARE_SRV_DYNAMIC(Default, MaterialsBuffer) StructuredBuffer<Material> materials_buffer;
#define material materials_buffer[0]

#ifndef CURRENT_MATERIAL
#define CURRENT_MATERIAL material
#endif

#ifdef VERTEX_SHADER

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
    float3 v_position : TEXCOORD0;
    float2 v_texcoord0 : TEXCOORD1;
    nointerpolation float3 v_camera_position : TEXCOORD2;
    nointerpolation uint v_object_index : TEXCOORD3;
};

#ifdef SKINNING

#include "include/Skeleton.inc"
DECLARE_SRV_DYNAMIC(Default, SkeletonsBuffer) StructuredBuffer<Skeleton> skeletons;

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

VSOutput VSMain(VSInput input, uint instanceId : SV_InstanceID)
{
    VSOutput output;
    
    float4 position;

#ifdef INSTANCING
    Entity currentEntity = entities[instanceId];
    float4x4 model_matrix = mul(entity_instance_batch.transforms[instanceId], currentEntity.model_matrix);
#else
    float4x4 model_matrix = entity.model_matrix;
#endif

#if defined(SKINNING) && defined(HYP_ATTRIBUTE_a_bone_indices) && defined(HYP_ATTRIBUTE_a_bone_weights)
    float4x4 skinning_matrix = CreateSkinningMatrix((int4)input.a_bone_indices, input.a_bone_weights);

    position = mul(model_matrix, mul(skinning_matrix, float4(input.a_position, 1.0)));
#else
    position = mul(model_matrix, float4(input.a_position, 1.0));
#endif

    output.v_position = position.xyz / position.w;
    output.v_texcoord0 = float2(input.a_texcoord0.x, 1.0 - input.a_texcoord0.y);
    output.v_camera_position = camera.position.xyz;

#ifdef INSTANCING
    output.v_object_index = OBJECT_INDEX;
#else
    output.v_object_index = ~0u;
#endif

    float4 position_ndc = mul(camera.viewProjMat, position);

    output.position_cs = position_ndc;

    return output;
}

#endif // VERTEX_SHADER

#ifdef PIXEL_SHADER

struct PSInput
{
    float4 position_cs : SV_POSITION;
    float3 v_position : TEXCOORD0;
    float2 v_texcoord0 : TEXCOORD1;
    nointerpolation float3 v_camera_position : TEXCOORD2;
    nointerpolation uint v_object_index : TEXCOORD3;
};

struct PSOutput
{
    float4 output_shadow : SV_Target0;
};

PSOutput PSMain(PSInput input)
{
    PSOutput output;

#if defined(ALPHA_DISCARD) && HAS_ALBEDO_MAP
    float4 albedo_texture = SAMPLE_MATERIAL_TEXTURE(CURRENT_MATERIAL, AlbedoMap, input.v_texcoord0);
    clip(albedo_texture.a - MATERIAL_ALPHA_DISCARD);
#endif

    const float depth = input.position_cs.z / input.position_cs.w;

#ifdef MODE_VSM
    float2 moments = float2(depth, HYP_FMATH_SQR(depth));

    float dx = ddx(depth);
    float dy = ddy(depth);

    moments.y += 0.25 * (HYP_FMATH_SQR(dx) + HYP_FMATH_SQR(dy));

    output.output_shadow = float4(moments, 0.0, 0.0);
#else
    output.output_shadow = PackDepth(depth);
#endif

    return output;
}

#endif // PIXEL_SHADER
