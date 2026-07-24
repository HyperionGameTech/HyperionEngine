#include "./include/Defines.hlsli"
#include "./include/Shared.hlsli"

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
#endif // IMMEDIATE_MODE
};

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "./include/EnvProbes.hlsli"
#include "./include/Scene.hlsli"
#include "./include/Entity.hlsli"

#ifdef IMMEDIATE_MODE
DECLARE_SRV(DebugDrawerDescriptorSet, EnvProbesBuffer) StructuredBuffer<EnvProbe> env_probes;

struct ImmediateDraw
{
    float4x4 transform;

    uint color_packed;
    uint env_probe_type;
    uint env_probe_index;
    uint idx;
};

DECLARE_SRV(DebugDrawerDescriptorSet, ImmediateDrawsBuffer) StructuredBuffer<ImmediateDraw> ImmediateDrawsBuffer;

#define MODEL_MATRIX (immediateDraw.transform)
#define PREV_MODEL_MATRIX (immediateDraw.transform)

#else // !IMMEDIATE_MODE

#ifdef INSTANCING
DECLARE_SRV(DebugDrawerDescriptorSet, EntitiesBuffer) StructuredBuffer<Entity> entities;
DECLARE_SRV_DYNAMIC(Default, EntityInstanceBatchesBuffer) ByteAddressBuffer EntityInstanceBatchBuffer;
#endif // INSTANCING

#define MODEL_MATRIX (entity.model_matrix)
#define PREV_MODEL_MATRIX (entity.previous_model_matrix)

#endif // IMMEDIATE_MODE

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#if !defined(INSTANCING) || defined(IMMEDIATE_MODE)
DECLARE_BUFFER_DYNAMIC(DebugDrawerDescriptorSet, CBuffer) cbuffer CBuffer
{
#if !defined(INSTANCING)
    // To match CBuffer in RendererMain:
    Entity entity;
#endif // INSTANCING

#ifdef IMMEDIATE_MODE
    // DebugDrawer CBuffer (IMMEDIATE_MODE)
    uint immediateDrawOffset;
#endif // IMMEDIATE_MODE
};
#endif // INSTANCING || IMMEDIATE_MODE

DECLARE_SRV_DYNAMIC(DebugDrawerDescriptorSet, CamerasBuffer) StructuredBuffer<Camera> _cameras_buffer;
#define camera _cameras_buffer[0]

VSOutput VSMain(VSInput input, uint instanceId : SV_InstanceID)
{
    VSOutput output;

#ifdef IMMEDIATE_MODE
    ImmediateDraw immediateDraw = ImmediateDrawsBuffer[immediateDrawOffset + instanceId];
#elif defined(INSTANCING)
    MeshEntityInstanceBatch batch = EntityInstanceBatchBuffer.Load<MeshEntityInstanceBatch>(0);
#endif // IMMEDIATE_MODE

    float4 position = mul(MODEL_MATRIX, float4(input.a_position, 1.0));
    position /= position.w;

    float4 previous_position = mul(PREV_MODEL_MATRIX, float4(input.a_position, 1.0));
    previous_position /= previous_position.w;

    output.position = position.xyz;
    output.normal = input.a_normal;
    output.texcoord0 = input.a_texcoord0;

#ifdef IMMEDIATE_MODE
    output.object_index = ~0u;
    output.color = UINT_TO_VEC4(immediateDraw.color_packed);

    output.env_probe_type = immediateDraw.env_probe_type;
    output.env_probe_index = immediateDraw.env_probe_index;
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
#endif // IMMEDIATE_MODE
};

struct PSOutput
{
    float4 gbuffer_albedo : SV_Target0;
    float4 gbuffer_normals : SV_Target1;
    uint gbuffer_material : SV_Target2;
    float2 gbuffer_velocity : SV_Target3;
};

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

DECLARE_SAMPLER(DebugDrawerDescriptorSet, SamplerLinear) SamplerState sampler_linear;
DECLARE_SAMPLER(DebugDrawerDescriptorSet, SamplerNearest) SamplerState sampler_nearest;

#include "include/Material.hlsli"
#include "include/Packing.hlsli"
#include "include/Scene.hlsli"
#include "include/Gbuffer.hlsli"
#include "include/Entity.hlsli"

DECLARE_SRV(DebugDrawerDescriptorSet, GBufferMipChain) Texture2D GBufferMipChain;

DECLARE_SRV_DYNAMIC(DebugDrawerDescriptorSet, CamerasBuffer) StructuredBuffer<Camera> _cameras_buffer;
#define camera _cameras_buffer[0]

DECLARE_SRV(DebugDrawerDescriptorSet, WorldsBuffer) StructuredBuffer<WorldShaderData> _worlds_buffer;
#define world_shader_data _worlds_buffer[0]

#include "include/BRDF.hlsli"

#ifndef IMMEDIATE_MODE
#ifdef INSTANCING
DECLARE_SRV(DebugDrawerDescriptorSet, EntitiesBuffer) StructuredBuffer<Entity> entities;
#else // !INSTANCING
DECLARE_BUFFER_DYNAMIC(DebugDrawerDescriptorSet, CBuffer) cbuffer CBuffer
{
    Entity entity;
    Material material;
};
#endif // INSTANCING
#endif // !IMMEDIATE_MODE

#include "include/EnvProbes.hlsli"

DECLARE_SRV(DebugDrawerDescriptorSet, EnvProbesColorTexture) TextureCubeArray envProbesColorTexture;

DECLARE_SRV(DebugDrawerDescriptorSet, EnvProbesBuffer) StructuredBuffer<EnvProbe> env_probes;

#define HYP_DEFERRED_NO_REFRACTION

#include "deferred/DeferredLighting.hlsli"

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

    materialParams.mask = OBJECT_MASK_UNLIT;

#ifdef IMMEDIATE_MODE
    output.gbuffer_albedo = float4(input.color.rgb, 1.0);

    if (input.env_probe_index != ~0u)
    {
        if (input.env_probe_type == EPT_REFLECTION)
        {
            const float3 N = normal;
            const float3 V = normalize(camera.position.xyz - input.position.xyz);
            const float3 R = reflect(-V, N);

            float4 ibl = float4(0.0, 0.0, 0.0, 0.0);

            const float lod = 1.5; // give it a little roughness to keep things interesting

            ApplyReflectionProbe(
                GET_ENV_PROBE_COLOR_TEXTURE_INDEX(env_probes[input.env_probe_index]),
                R,
                lod,
                ibl);

            output.gbuffer_albedo = ibl;
        }
        else
        {
            const float3 shColor = EnvProbeSH(env_probes[input.env_probe_index], normal);

            output.gbuffer_albedo.rgb = shColor;
        }
    }
#endif // IMMEDIATE_MODE

    float roughnessAndMetalPacked;
    uint maskPacked;
    GBufferPackMaterialParams(materialParams, roughnessAndMetalPacked, maskPacked);

    output.gbuffer_normals.x = roughnessAndMetalPacked;

    // mask is stored in last 4 bits
    output.gbuffer_material = (maskPacked << 28u);

    output.gbuffer_velocity = velocity;

    return output;
}

#endif // PIXEL_SHADER
