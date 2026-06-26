#include "include/Defines.hlsli"

PERMUTE(SHADING_TYPE, FORWARD);

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "include/Gbuffer.hlsli"
#include "include/Scene.hlsli"
#include "include/Entity.hlsli"
#include "include/Packing.hlsli"
#include "include/Material.hlsli"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

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

#ifdef VERTEX_SHADER

struct VSInput
{
    HYP_ATTRIBUTE float3 a_position : POSITION;
    HYP_ATTRIBUTE float3 a_normal : NORMAL;
    HYP_ATTRIBUTE float2 a_texcoord0 : TEXCOORD0;
};

struct VSOutput
{
    float4 position_cs : SV_POSITION;
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texcoord0 : TEXCOORD0;
    nointerpolation uint object_index : TEXCOORD1;
};

#ifdef INSTANCING
DECLARE_SRV(Default, EntitiesBuffer) StructuredBuffer<Entity> entities;
DECLARE_SRV_DYNAMIC(Default, EntityInstanceBatchesBuffer) ByteAddressBuffer EntityInstanceBatchBuffer;
#endif // INSTANCING

VSOutput VSMain(VSInput input, uint instanceId : SV_InstanceID)
{
    VSOutput output;

    float4 position = mul(entity.model_matrix, float4(input.a_position, 1.0));

    float3x3 normal_matrix = (float3x3)entity.normal_matrix;

    output.position = input.a_position.xyz;
    output.normal = mul(normal_matrix, input.a_normal);
    output.texcoord0 = input.a_texcoord0;

    float4x4 view_matrix = camera.view;
    
    // strip the translation from the view matrix
    view_matrix[0][3] = 0.0;
    view_matrix[1][3] = 0.0;
    view_matrix[2][3] = 0.0;

#ifdef INSTANCING
    output.object_index = OBJECT_INDEX;
#else
    output.object_index = 0;
#endif

    output.position_cs = mul(camera.projection, mul(view_matrix, position));

    return output;
}

#endif // VERTEX_SHADER

#ifdef PIXEL_SHADER

struct PSInput
{
    float4 position_cs : SV_POSITION;
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texcoord0 : TEXCOORD0;
    nointerpolation uint object_index : TEXCOORD1;
};

struct PSOutput
{
    float4 gbuffer_albedo : SV_Target0;
};

DECLARE_SAMPLER(Default, SamplerLinear) SamplerState texture_sampler;

#ifdef HYP_FEATURES_BINDLESS_TEXTURES
DECLARE_SRV(BindlessResources0, Textures) TextureCube textures[]; // aliasing texture2D as textureCube
#else // !HYP_FEATURES_BINDLESS_TEXTURES
DECLARE_SRV(Default, DiffuseMap) TextureCube DiffuseMap;
#endif // HYP_FEATURES_BINDLESS_TEXTURES

PSOutput PSMain(PSInput input)
{
    PSOutput output;

    float3 normal = normalize(input.normal);

    output.gbuffer_albedo = float4(SAMPLE_MATERIAL_TEXTURE_CUBE(material, DiffuseMap, input.position).rgb, 1.0);

    return output;
}

#endif // PIXEL_SHADER
