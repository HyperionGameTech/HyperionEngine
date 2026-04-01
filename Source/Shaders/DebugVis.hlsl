#include "./include/defines.inc"
#include "./include/shared.inc"

PERMUTE(IMMEDIATE_MODE);
PERMUTE(INSTANCING);

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
    float4 position_ndc : TEXCOORD11;
    float4 previous_position_ndc : TEXCOORD12;
    nointerpolation uint object_index : TEXCOORD15;
#ifdef IMMEDIATE_MODE
    float4 color : TEXCOORD16;
    nointerpolation uint env_probe_index : TEXCOORD17;
    nointerpolation uint env_probe_type : TEXCOORD18;
#endif
};

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "./include/env_probe.inc"
#include "./include/scene.inc"

#ifdef IMMEDIATE_MODE
DECLARE_SRV(DebugDrawerDescriptorSet, EnvProbesBuffer) StructuredBuffer<EnvProbe> env_probes;

struct ImmediateDraw
{
    float4x4 model_matrix;

    uint color_packed;
    uint env_probe_type;
    uint env_probe_index;
    uint _pad2;
};

DECLARE_SRV_DYNAMIC(DebugDrawerDescriptorSet, ImmediateDrawsBuffer) StructuredBuffer<ImmediateDraw> immediateDraws;

#define MODEL_MATRIX (immediateDraw.model_matrix)
#define PREV_MODEL_MATRIX (immediateDraw.model_matrix)

#else

#include "./include/Entity.inc"

#ifdef INSTANCING
DECLARE_SRV(DebugDrawerDescriptorSet, EntitiesBuffer) StructuredBuffer<Entity> entities;

DECLARE_SRV_DYNAMIC(DebugDrawerDescriptorSet, EntityInstanceBatchesBuffer) StructuredBuffer<MeshEntityInstanceBatch> entity_instance_batch_buffer;
#define entity_instance_batch entity_instance_batch_buffer[0]
#else
DECLARE_SRV_DYNAMIC(DebugDrawerDescriptorSet, CurrentEntity) StructuredBuffer<Entity> entities;
#endif

#define MODEL_MATRIX (entity.model_matrix)
#define PREV_MODEL_MATRIX (entity.previous_model_matrix)
#endif

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

DECLARE_BUFFER_DYNAMIC(DebugDrawerDescriptorSet, CamerasBuffer) cbuffer CamerasBuffer
{
    Camera camera;
};

VSOutput VSMain(VSInput input, uint instanceId : SV_InstanceID)
{
    VSOutput output;

#ifdef IMMEDIATE_MODE
    ImmediateDraw immediateDraw = immediateDraws[instanceId];
#endif

    float4 position = mul(MODEL_MATRIX, float4(input.a_position, 1.0));
    float4 previous_position = mul(PREV_MODEL_MATRIX, float4(input.a_position, 1.0));

    output.position = position.xyz;
    output.normal = input.a_normal;
    output.texcoord0 = input.a_texcoord0;

#ifdef IMMEDIATE_MODE
    output.object_index = ~0u;
    output.color = UINT_TO_VEC4(immediateDraw.color_packed);

    output.env_probe_type = immediateDraw.env_probe_type;
    output.env_probe_index = immediateDraw.env_probe_index;

    if (immediateDraw.env_probe_index != ~0u && immediateDraw.env_probe_type == ENV_PROBE_TYPE_AMBIENT)
    {
        SH9 sh9;

        for (int i = 0; i < 9; i++)
        {
            sh9.values[i] = env_probes[immediateDraw.env_probe_index].sh[i].rgb;
        }

        output.color = float4(SphericalHarmonicsSample(sh9, input.a_normal), 1.0);
    }
#elif defined(INSTANCING)
    output.object_index = OBJECT_INDEX;
#else
    output.object_index = 0;
#endif

    float4x4 jitterMat = { 
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1 
    };
    jitterMat[0][3] += camera.jitter.x;
    jitterMat[1][3] += camera.jitter.y;

    output.position_ndc = mul(camera.viewProjMat, position);
    output.previous_position_ndc = mul(camera.prevViewProjMat, previous_position);

    output.position_cs = mul(jitterMat, output.position_ndc);

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
    float4 position_ndc : TEXCOORD11;
    float4 previous_position_ndc : TEXCOORD12;
    nointerpolation uint object_index : TEXCOORD15;
#ifdef IMMEDIATE_MODE
    float4 color : TEXCOORD16;
    nointerpolation uint env_probe_index : TEXCOORD17;
    nointerpolation uint env_probe_type : TEXCOORD18;
#endif
};

struct PSOutput
{
    float4 gbuffer_albedo : SV_Target0;
    float4 gbuffer_normals : SV_Target1;
    uint4 gbuffer_material : SV_Target2;
    float2 gbuffer_velocity : SV_Target3;
};

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

DECLARE_SAMPLER(DebugDrawerDescriptorSet, SamplerLinear) SamplerState sampler_linear;
DECLARE_SAMPLER(DebugDrawerDescriptorSet, SamplerNearest) SamplerState sampler_nearest;

#include "include/material.inc"
#include "include/packing.inc"
#include "include/scene.inc"
#include "include/gbuffer.inc"

DECLARE_SRV(DebugDrawerDescriptorSet, GBufferMipChain) Texture2D gbuffer_mip_chain;

DECLARE_BUFFER_DYNAMIC(DebugDrawerDescriptorSet, CamerasBuffer) cbuffer CamerasBuffer
{
    Camera camera;
};

DECLARE_BUFFER(DebugDrawerDescriptorSet, WorldsBuffer) cbuffer WorldsBuffer
{
    WorldShaderData world_shader_data;
};

#include "include/Entity.inc"

#ifdef IMMEDIATE_MODE

#include "include/brdf.inc"

#elif defined(INSTANCING)
DECLARE_SRV(DebugDrawerDescriptorSet, EntitiesBuffer) StructuredBuffer<Entity> entities;
#else
DECLARE_SRV_DYNAMIC(DebugDrawerDescriptorSet, CurrentEntity) StructuredBuffer<Entity> entities;
#endif

#include "include/env_probe.inc"

#if ENV_PROBE_CUBEMAP
DECLARE_SRV(DebugDrawerDescriptorSet, EnvProbesTexture) TextureCubeArray envProbesTexture;
#else
DECLARE_SRV(DebugDrawerDescriptorSet, EnvProbesTexture) Texture2DArray envProbesTexture;
#endif

DECLARE_SRV(DebugDrawerDescriptorSet, EnvProbesBuffer) StructuredBuffer<EnvProbe> env_probes;

#define HYP_DEFERRED_NO_REFRACTION

#include "deferred/DeferredLighting.inc"

#undef HYP_DEFERRED_NO_REFRACTION
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#define OBJECT_INDEX input.object_index

PSOutput PSMain(PSInput input)
{
    PSOutput output;

    float3 normal = normalize(input.normal);

    float2 velocity = float2(((input.position_ndc.xy / input.position_ndc.w) * 0.5 + 0.5) - ((input.previous_position_ndc.xy / input.previous_position_ndc.w) * 0.5 + 0.5));

    GBufferMaterialParams materialParams;
    materialParams.roughness = 0.0;
    materialParams.metalness = 0.0;

    output.gbuffer_albedo = float4(0.0, 1.0, 0.0, 1.0);
    output.gbuffer_normals = GBufferPackNormal(normal);
    output.gbuffer_velocity = float2(velocity);

#ifdef IMMEDIATE_MODE
    output.gbuffer_albedo = float4(input.color.rgb, 1.0);

    materialParams.mask = OBJECT_MASK_TRANSLUCENT | OBJECT_MASK_DEBUG;

    if (input.env_probe_index != ~0u && input.env_probe_type == ENV_PROBE_TYPE_REFLECTION)
    {
        const float3 N = normal;
        const float3 V = normalize(camera.position.xyz - input.position.xyz);

        float4 ibl = float4(0.0, 0.0, 0.0, 0.0);

        const float3 R = reflect(-V, N);

        ApplyReflectionProbe(
            env_probes[input.env_probe_index].texture_index,
            env_probes[input.env_probe_index].world_position.xyz,
            env_probes[input.env_probe_index].aabb_min.xyz,
            env_probes[input.env_probe_index].aabb_max.xyz,
            input.position.xyz,
            R,
            0.0,
            ibl);

        output.gbuffer_albedo.rgb = ibl.rgb;
    }
#else
    materialParams.mask = GET_OBJECT_BUCKET(entity);
#endif

    float roughnessAndMetalPacked;
    uint maskPacked;
    GBufferPackMaterialParams(materialParams, roughnessAndMetalPacked, maskPacked);
    
    output.gbuffer_normals.x = roughnessAndMetalPacked;

    output.gbuffer_material.x = maskPacked;
    output.gbuffer_material.z = 0u;
    output.gbuffer_material.w = 0u;
    output.gbuffer_material.y = 0u;

    output.gbuffer_velocity = velocity;

    return output;
}

#endif // PIXEL_SHADER
