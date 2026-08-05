#include "include/Defines.hlsli"
#include "include/Shared.hlsli"

PERMUTE(VSM);
PERMUTE(INSTANCING);
PERMUTE(SKINNING);
PERMUTE(ALPHA_DISCARD);

DECLARE_SAMPLER(Default, SamplerLinear) SamplerState sampler_linear;
DECLARE_SAMPLER(Default, SamplerNearest) SamplerState sampler_nearest;

#define texture_sampler sampler_linear

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "include/Scene.hlsli"
#include "include/Entity.hlsli"
#include "include/Material.hlsli"
#include "include/Packing.hlsli"

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#ifdef ALPHA_DISCARD
#ifdef HYP_FEATURES_BINDLESS_TEXTURES
    DECLARE_SRV(BindlessResources0, Textures) uniform texture2D textures[];
#else // !HYP_FEATURES_BINDLESS_TEXTURES
    DECLARE_SRV(Material, DiffuseMap) uniform texture2D DiffuseMap;
#endif // HYP_FEATURES_BINDLESS_TEXTURES
#endif // ALPHA_DISCARD

#ifdef INSTANCING
DECLARE_SRV(Default, EntitiesBuffer) StructuredBuffer<Entity> entities;
DECLARE_SRV_DYNAMIC(Default, EntityInstanceBatchesBuffer) ByteAddressBuffer EntityInstanceBatchBuffer;
#endif // INSTANCING

DECLARE_BUFFER_DYNAMIC(Default, CBuffer) cbuffer CBuffer
{
#ifndef INSTANCING
    Entity entity;
#else // INSTANCING
    Entity dummyEntity;
#endif // !INSTANCING
    Camera camera;
    Material material;
    float4x4 vpMatrix;
};

#ifndef CURRENT_MATERIAL
#define CURRENT_MATERIAL material
#endif // !CURRENT_MATERIAL

#ifdef VERTEX_SHADER

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
    float3 v_position : TEXCOORD0;
    float2 v_texcoord0 : TEXCOORD1;
    nointerpolation uint object_index : TEXCOORD2;
};

#ifdef SKINNING
DECLARE_SRV_DYNAMIC(Default, SkeletonsBuffer) StructuredBuffer<float4x4> SkeletonsBuffer;
#include "include/Skinning.hlsli"
#endif // SKINNING

VSOutput VSMain(VSInput input, uint instanceId : SV_InstanceID)
{
    VSOutput output;

    float4 position;

#ifdef INSTANCING
    MeshEntityInstanceBatch batch = EntityInstanceBatchBuffer.Load<MeshEntityInstanceBatch>(0);

    const uint objectIndex = OBJECT_INDEX;
    const uint dataOffset = OBJECT_DATA_OFFSET;

    Entity currentEntity = entities[objectIndex];

    float4x4 transform = batch.transforms[dataOffset];
#ifdef VULKAN
    float4x4 model_matrix = mul(currentEntity.model_matrix, transform);
#else
    float4x4 model_matrix = mul(transform, currentEntity.model_matrix);
#endif

#else
    float4x4 model_matrix = entity.model_matrix;
#endif

#if defined(SKINNING) && defined(HYP_ATTRIBUTE_a_bone_indices) && defined(HYP_ATTRIBUTE_a_bone_weights)
    float4x4 skinning_matrix = CreateSkinningMatrix(input.a_bone_indices, input.a_bone_weights);

    position = mul(model_matrix, mul(skinning_matrix, float4(input.a_position, 1.0)));
#else
    position = mul(model_matrix, float4(input.a_position, 1.0));
#endif

    output.v_position = position.xyz / position.w;
    output.v_texcoord0 = float2(input.a_texcoord0.x, 1.0 - input.a_texcoord0.y);

#ifdef INSTANCING
    output.object_index = objectIndex;
#else
    output.object_index = ~0u;
#endif

    float4 position_ndc = mul(vpMatrix, position);
    position_ndc /= position_ndc.w;

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
    nointerpolation uint object_index : TEXCOORD2;
};

struct PSOutput
{
    float4 output_shadow : SV_Target0;
};

PSOutput PSMain(PSInput input)
{
    PSOutput output;

#ifdef ALPHA_DISCARD
    if (HAS_TEXTURE(CURRENT_MATERIAL, DiffuseMap))
    {
        float4 albedo_texture = SAMPLE_MATERIAL_TEXTURE(CURRENT_MATERIAL, DiffuseMap, input.v_texcoord0);
        clip(albedo_texture.a - MATERIAL_ALPHA_DISCARD);
    }
#endif

    const float depth = input.position_cs.z / input.position_cs.w;

#ifdef VSM
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
