#include "include/Defines.hlsli"
#include "include/Shared.hlsli"
#include "include/Scene.hlsli"

PERMUTE(MODE_SHADOWS)
PERMUTE(WRITE_NORMALS)
PERMUTE(WRITE_MOMENTS)
PERMUTE(INSTANCING)
PERMUTE(SKINNING)

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
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texcoord0 : TEXCOORD0;
    nointerpolation float3 camera_position : TEXCOORD3;
    nointerpolation uint object_index : TEXCOORD6;
    nointerpolation uint cube_face_index : TEXCOORD7;
};

DECLARE_SRV_DYNAMIC(Default, CamerasBuffer) StructuredBuffer<Camera> _cameras_buffer;
#define camera _cameras_buffer[0]

#include "include/Entity.hlsli"

#ifdef INSTANCING
DECLARE_SRV(Default, EntitiesBuffer) StructuredBuffer<Entity> entities;
DECLARE_SRV_DYNAMIC(Default, EntityInstanceBatchesBuffer) ByteAddressBuffer entity_instance_batches;

#define entity_instance_batch entity_instance_batches.Load<MeshEntityInstanceBatch>(0)
#else // !INSTANCING

DECLARE_BUFFER_DYNAMIC(Default, CBuffer) cbuffer CBuffer
{
    Entity entity;
};

#endif // INSTANCING

#ifdef SKINNING
DECLARE_SRV_DYNAMIC(Default, SkeletonsBuffer) StructuredBuffer<float4x4> SkeletonsBuffer;
#include "include/Skinning.hlsli"
#endif // SKINNING

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

#ifdef INSTANCING
    Entity currentEntity = entities[entity_instance_batch.indices[instanceId / 4][instanceId % 4]];
    float4x4 model_matrix = mul(entity_instance_batch.transforms[instanceId], currentEntity.model_matrix);
    float3x3 normal_matrix = transpose(inverse((float3x3)model_matrix));
#else
    Entity currentEntity = entity;
    float4x4 model_matrix = entity.model_matrix;
    float3x3 normal_matrix = transpose(inverse((float3x3)model_matrix));
#endif

#if defined(SKINNING) && defined(HYP_ATTRIBUTE_a_bone_indices) && defined(HYP_ATTRIBUTE_a_bone_weights)
    float4x4 skinning_matrix = CreateSkinningMatrix(input.a_bone_indices, input.a_bone_weights);

    position = mul(model_matrix, mul(skinning_matrix, float4(input.a_position, 1.0)));
    normal_matrix = mul(normal_matrix, transpose(inverse((float3x3)skinning_matrix)));
#else
    position = mul(model_matrix, float4(input.a_position, 1.0));
#endif

    output.position = position.xyz / position.w;
    output.normal = mul(normal_matrix, input.a_normal);
    output.texcoord0 = float2(input.a_texcoord0.x, 1.0 - input.a_texcoord0.y);

    const float3 forward_direction = g_cubemapDirections[ViewId * 2];
    const float3 up_direction = g_cubemapDirections[ViewId * 2 + 1];

    float4x4 projection_matrix = camera.projection;

    float4x4 view_matrix;
    
    output.camera_position = camera.position.xyz;
    view_matrix = LookAt(output.camera_position, output.camera_position + forward_direction, up_direction);

#ifdef INSTANCING
    output.object_index = OBJECT_INDEX;
#endif

    output.position_cs = mul(projection_matrix, mul(view_matrix, position));

    output.cube_face_index = ViewId;

    return output;
}

#endif

#ifdef PIXEL_SHADER

struct PSInput
{
    float4 position_cs : SV_POSITION;
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texcoord0 : TEXCOORD0;
    nointerpolation float3 camera_position : TEXCOORD3;
    nointerpolation uint object_index : TEXCOORD6;
    nointerpolation uint cube_face_index : TEXCOORD7;
};

struct PSOutput
{
#ifdef MODE_SHADOWS
    #ifdef WRITE_MOMENTS // For variant shadow map
        float2 output_moments : SV_Target0;
    #endif // WRITE_MOMENTS
#else // !MODE_SHADOWS
    float4 output_color : SV_Target0;

    #ifdef WRITE_NORMALS
        float2 output_normals : SV_Target1;
        #ifdef WRITE_MOMENTS
            float2 output_moments : SV_Target2;
        #endif // !WRITE_MOMENTS
    #else // !WRITE_NORMALS
        #ifdef WRITE_MOMENTS
            float2 output_moments : SV_Target1;
        #endif
    #endif // WRITE_NORMALS
#endif // MODE_SHADOWS
};

DECLARE_SAMPLER(Default, SamplerLinear) SamplerState sampler_linear;
DECLARE_SAMPLER(Default, SamplerNearest) SamplerState sampler_nearest;

#define texture_sampler sampler_linear

#include "include/Material.hlsli"
#include "include/Gbuffer.hlsli"
#include "include/EnvProbes.hlsli"
#include "include/Octahedron.hlsli"
#include "include/Entity.hlsli"
#include "include/Packing.hlsli"
#include "include/BRDF.hlsli"

#define HYP_CUBEMAP_AMBIENT 0.005

#ifdef INSTANCING
DECLARE_SRV(Default, EntitiesBuffer) StructuredBuffer<Entity> entities;
#endif // INSTANCING

DECLARE_BUFFER_DYNAMIC(Default, CBuffer) cbuffer CBuffer
{
    Entity entity;
    Material material;
};

#ifndef CURRENT_MATERIAL
#define CURRENT_MATERIAL material
#endif

PSOutput PSMain(PSInput input)
{
    PSOutput output;

    float3 V = normalize(input.camera_position - input.position);
    float3 N = normalize(input.normal);
    float3 R = reflect(-V, N);

    float4 albedo = float4(1.0, 1.0, 1.0, 1.0);

    if (HAS_TEXTURE(CURRENT_MATERIAL, DiffuseMap))
    {
        float2 texcoord = input.texcoord0 * CURRENT_MATERIAL.uv_scale;
        albedo = CURRENT_MATERIAL.albedo;

        float4 albedo_texture = SAMPLE_MATERIAL_TEXTURE(CURRENT_MATERIAL, DiffuseMap, texcoord);

        clip(albedo_texture.a - 0.2);

        albedo *= albedo_texture;
    }

#ifdef WRITE_MOMENTS
    const float dist = distance(input.position, input.camera_position);
    float2 moments = float2(dist, HYP_FMATH_SQR(dist));
    
#ifdef MODE_SHADOWS
    float dx = ddx(dist);
    float dy = ddy(dist);

    moments.y += 0.25 * (HYP_FMATH_SQR(dx) + HYP_FMATH_SQR(dy));
#endif // MODE_SHADOWS

#endif // WRITE_MOMENTS

#ifndef MODE_SHADOWS
    output.output_color.rgb = albedo.rgb;
    output.output_color.a = 1.0;

#ifdef WRITE_NORMALS
    output.output_normals = PackNormalVec2(N);
#endif // WRITE_NORMALS

#endif // !MODE_SHADOWS

#ifdef WRITE_MOMENTS
    output.output_moments = moments;
#endif // WRITE_MOMENTS

    return output;
}

#endif // PIXEL_SHADER