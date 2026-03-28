#include "include/defines.inc"
#include "include/shared.inc"
#include "include/scene.inc"

#ifdef VERTEX_SHADER

struct VSInput
{
    HYP_ATTRIBUTE float3 a_position : POSITION;
    HYP_ATTRIBUTE float3 a_normal : NORMAL;
    HYP_ATTRIBUTE float2 a_texcoord0 : TEXCOORD0;
    HYP_ATTRIBUTE_OPTIONAL float2 a_texcoord1 : TEXCOORD1;
    HYP_ATTRIBUTE_OPTIONAL float3 a_tangent : TANGENT;
    HYP_ATTRIBUTE_OPTIONAL float3 a_bitangent : BINORMAL;
    HYP_ATTRIBUTE_OPTIONAL float4 a_bone_weights : BLENDWEIGHT;
    HYP_ATTRIBUTE_OPTIONAL float4 a_bone_indices : BLENDINDICES;
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

#ifdef INSTANCING
    Entity currentEntity = entities[instanceId];
    float4x4 model_matrix = mul(entity_instance_batch.transforms[instanceId], currentEntity.model_matrix);
    float3x3 normal_matrix = transpose(inverse((float3x3)model_matrix));
#else
    Entity currentEntity = entity;
    float4x4 model_matrix = entity.model_matrix;
    float3x3 normal_matrix = transpose(inverse((float3x3)model_matrix));
#endif

#if defined(SKINNING) && defined(HYP_ATTRIBUTE_a_bone_indices) && defined(HYP_ATTRIBUTE_a_bone_weights)
    float4x4 skinning_matrix = CreateSkinningMatrix((int4)input.a_bone_indices, input.a_bone_weights);

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
    float2 output_color : SV_Target0;
#else
    float4 output_color : SV_Target0;
#endif

#ifdef WRITE_NORMALS
    float2 output_normals : SV_Target1;
    #ifdef WRITE_MOMENTS
    float2 output_moments : SV_Target2;
    #endif
#else
    #ifdef WRITE_MOMENTS
    float2 output_moments : SV_Target1;
    #endif
#endif
};

DECLARE_SAMPLER(Default, SamplerLinear) SamplerState sampler_linear;
DECLARE_SAMPLER(Default, SamplerNearest) SamplerState sampler_nearest;

#define texture_sampler sampler_linear

#include "include/material.inc"
#include "include/gbuffer.inc"
#include "include/env_probe.inc"
#include "include/Octahedron.inc"
#include "include/Entity.inc"
#include "include/packing.inc"
#include "include/brdf.inc"

#define HYP_CUBEMAP_AMBIENT 0.005

DECLARE_SRV(Default, ShadowMapsTextureArray) Texture2DArray<float> shadow_maps;
DECLARE_SRV(Default, PointLightShadowMapsTextureArray) TextureCubeArray point_shadow_maps;

#ifdef INSTANCING
    DECLARE_SRV(Default, EntitiesBuffer) StructuredBuffer<Entity> entities;
#else
    DECLARE_SRV_DYNAMIC(Default, CurrentEntity) StructuredBuffer<Entity> entities;
#endif

DECLARE_SRV_DYNAMIC(Default, MaterialsBuffer) StructuredBuffer<Material> materials_buffer;
#define material materials_buffer[0]

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

#if HAS_DIFFUSE_MAP
    float2 texcoord = input.texcoord0 * CURRENT_MATERIAL.uv_scale;
    albedo = CURRENT_MATERIAL.albedo;

    float4 albedo_texture = SAMPLE_MATERIAL_TEXTURE(CURRENT_MATERIAL, DiffuseMap, texcoord);

    if (albedo_texture.a < 0.2)
    {
        discard;
    }

    albedo *= albedo_texture;
#endif

#if defined(WRITE_MOMENTS) || defined(MODE_SHADOWS)
    const float dist = distance(input.position, input.camera_position);
    float2 moments = float2(dist, HYP_FMATH_SQR(dist));
#endif

#ifdef MODE_SHADOWS
    float dx = ddx(dist);
    float dy = ddy(dist);

    moments.y += 0.25 * (HYP_FMATH_SQR(dx) + HYP_FMATH_SQR(dy));

    output.output_color = moments;
#else
    output.output_color.rgb = albedo.rgb;
    output.output_color.a = 1.0;

#ifdef WRITE_NORMALS
    output.output_normals = PackNormalVec2(N);
#endif

#ifdef WRITE_MOMENTS
output.output_moments = moments;
    #endif
#endif

    return output;
}

#endif // PIXEL_SHADER