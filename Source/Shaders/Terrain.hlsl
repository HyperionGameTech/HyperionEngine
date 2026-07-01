#include "include/Defines.hlsli"

struct PSInput
{
    float4 position_cs : SV_POSITION;
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texcoord0 : TEXCOORD0;
    float2 texcoord1 : TEXCOORD1;
    float3 tangent : TANGENT;
    float3 bitangent : BINORMAL;
    nointerpolation float3 camera_position : TEXCOORD3;
    float4 position_ndc : TEXCOORD4;
    float4 previous_position_ndc : TEXCOORD5;
    nointerpolation uint object_index : TEXCOORD6;
    nointerpolation uint object_mask : TEXCOORD7;
};

struct PSOutput
{
    float4 gbuffer_albedo : SV_Target0;
    float4 gbuffer_normals : SV_Target1;
    uint gbuffer_material : SV_Target2;
    float2 gbuffer_velocity : SV_Target3;
};

DECLARE_SAMPLER(Default, SamplerLinear) SamplerState sampler_linear;
DECLARE_SAMPLER(Default, SamplerNearest) SamplerState sampler_nearest;

#define texture_sampler sampler_linear

#define HAS_REFRACTION 1

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "include/Scene.hlsli"
#include "include/Material.hlsli"
#include "include/Entity.hlsli"
#include "include/Packing.hlsli"

#include "include/EnvProbes.hlsli"
#include "include/Gbuffer.hlsli"

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

DECLARE_SRV(Default, GBufferMipChain) Texture2D GBufferMipChain;

DECLARE_SRV_DYNAMIC(Default, CamerasBuffer) StructuredBuffer<Camera> _cameras_buffer;
#define camera _cameras_buffer[0]

DECLARE_SRV(Default, WorldsBuffer) StructuredBuffer<WorldShaderData> _worlds_buffer;
#define world_shader_data _worlds_buffer[0]

DECLARE_SRV(Default, ShadowMapsTextureArray) Texture2DArray<float> shadow_maps;
DECLARE_SRV(Default, PointLightShadowMapsTextureArray) TextureCubeArray point_shadow_maps;

#ifdef LIGHTING_FORWARD
#include "include/BRDF.hlsli"
#include "deferred/DeferredLighting.hlsli"
#include "include/Shadows.hlsli"
#endif

DECLARE_SRV_DYNAMIC(Default, CurrentEnvProbe) StructuredBuffer<EnvProbe> current_env_probe_buffer;
#define current_env_probe current_env_probe_buffer[0]

#ifdef INSTANCING
DECLARE_SRV(Default, EntitiesBuffer) StructuredBuffer<Entity> entities;
#endif // INSTANCING

DECLARE_SRV_DYNAMIC(Default, CurrentLight) StructuredBuffer<Light> current_light_buffer;
#define light current_light_buffer[0]

#ifndef INSTANCING
DECLARE_BUFFER_DYNAMIC(Default, CBuffer) cbuffer CBuffer
{
    Entity entity;
};
#endif // !INSTANCING

#ifndef CURRENT_MATERIAL
#define CURRENT_MATERIAL material
#endif

PSOutput PSMain(PSInput input)
{
    PSOutput output;

    float3x3 tbn_matrix = float3x3(normalize(input.tangent), normalize(input.bitangent), normalize(input.normal));

    float3 view_vector = normalize(input.camera_position - input.position);
    float3 N = normalize(input.normal);
    float NdotV = dot(N, view_vector);

    float3 tangent_view = mul(transpose(tbn_matrix), view_vector);
    float3 tangent_position = mul(tbn_matrix, input.position);

    float3 reflection_vector = reflect(view_vector, N);

    output.gbuffer_albedo = CURRENT_MATERIAL.albedo;
    output.gbuffer_albedo.a = 1.0;

    float ao = 1.0;
    float metalness = GET_MATERIAL_PARAM(CURRENT_MATERIAL, MATERIAL_PARAM_METALNESS);
    float roughness = GET_MATERIAL_PARAM(CURRENT_MATERIAL, MATERIAL_PARAM_ROUGHNESS);
    float transmission = GET_MATERIAL_PARAM(CURRENT_MATERIAL, MATERIAL_PARAM_TRANSMISSION);

    float2 texcoord = input.texcoord0 * CURRENT_MATERIAL.uv_scale;

#if HAS_DIFFUSE_MAP
    float4 albedo_texture = SAMPLE_MATERIAL_TEXTURE_TRIPLANAR(CURRENT_MATERIAL, DiffuseMap, input.position, N);

    output.gbuffer_albedo = float4(albedo_texture.rgb, 1.0);
#endif

    float4 normals_texture = (float4)0.0;

#if HAS_NORMAL_MAP
    normals_texture = SAMPLE_MATERIAL_TEXTURE_TRIPLANAR(CURRENT_MATERIAL, NormalMap, input.position, N) * 2.0 - 1.0;
    N = normalize(mul(tbn_matrix, normals_texture.rgb));
#endif

#if HAS_ROUGHNESS_MAP
    float roughness_sample = SAMPLE_MATERIAL_TEXTURE_TRIPLANAR(CURRENT_MATERIAL, RoughnessMap, input.position, N).r;

    roughness = roughness_sample;
#endif

    float2 velocity = ((input.position_ndc.xy / input.position_ndc.w) * 0.5 + 0.5)
        - ((input.previous_position_ndc.xy / input.previous_position_ndc.w) * 0.5 + 0.5);

    output.gbuffer_normals = GBufferPackNormal(N);
    output.gbuffer_velocity = velocity;

    output.gbuffer_material = 0;

    return output;
}
