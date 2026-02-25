#include "include/defines.inc"

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "include/gbuffer.inc"
#include "include/scene.inc"
#include "include/Entity.inc"
#include "include/packing.inc"
#include "include/material.inc"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

DECLARE_SRV_DYNAMIC(Default, MaterialsBuffer) StructuredBuffer<Material> material_buffer;
#define material material_buffer[0]

DECLARE_BUFFER_DYNAMIC(Default, CamerasBuffer) cbuffer CamerasBuffer
{
    Camera camera;
};

#ifdef VERTEX_SHADER

struct VSInput
{
    HYP_ATTRIBUTE(0) float3 a_position : POSITION;
    HYP_ATTRIBUTE(1) float3 a_normal : NORMAL;
    HYP_ATTRIBUTE(2) float2 a_texcoord0 : TEXCOORD0;
    HYP_ATTRIBUTE_OPTIONAL(3) float2 a_texcoord1 : TEXCOORD1;
    HYP_ATTRIBUTE_OPTIONAL(4) float3 a_tangent : TANGENT;
    HYP_ATTRIBUTE_OPTIONAL(5) float3 a_bitangent : BINORMAL;
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
DECLARE_SRV_DYNAMIC(Default, EntityInstanceBatchesBuffer) StructuredBuffer<EntityInstanceBatch> entity_instance_batch_buffer;
#define entity_instance_batch entity_instance_batch_buffer[0]
#else
DECLARE_SRV_DYNAMIC(Default, CurrentEntity) StructuredBuffer<Entity> entities;
#endif

VSOutput VSMain(VSInput input, uint instanceId : SV_InstanceID)
{
    VSOutput output;

    float4 position = mul(entity.model_matrix, float4(input.a_position, 1.0));
    float3x3 normal_matrix = (float3x3)entity.model_matrix;

    output.position = input.a_position.xyz;
    output.normal = mul(normal_matrix, input.a_normal);
    output.texcoord0 = input.a_texcoord0;

    float4x4 view_matrix = camera.view;
    view_matrix[3] = float4(0.0, 0.0, 0.0, 1.0);

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
    float4 gbuffer_normals : SV_Target1;
    uint4 gbuffer_material : SV_Target2;
    float2 gbuffer_velocity : SV_Target3;
};

DECLARE_SAMPLER(Default, SamplerLinear) SamplerState texture_sampler;

#ifdef HYP_FEATURES_BINDLESS_TEXTURES
DECLARE_SRV(BindlessResources0, Textures) TextureCube textures[]; // aliasing texture2D as textureCube
#else
DECLARE_SRV(Default, DiffuseMap) TextureCube DiffuseMap;
#endif

PSOutput PSMain(PSInput input)
{
    PSOutput output;

    float3 normal = normalize(input.normal);

    output.gbuffer_albedo = float4(SAMPLE_MATERIAL_TEXTURE_CUBE(material, DiffuseMap, input.position).rgb, 1.0);

    output.gbuffer_normals = GBufferPackNormal(normal);

    output.gbuffer_material.x = OBJECT_MASK_SKY;
    output.gbuffer_material.y = 0u;
    output.gbuffer_material.z = 0u;
    output.gbuffer_material.w = 0u;
    
    output.gbuffer_velocity = float2(0.0, 0.0);

    return output;
}

#endif // PIXEL_SHADER
