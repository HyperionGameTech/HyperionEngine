#include "../include/defines.inc"

#ifdef VERTEX_SHADER

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../include/scene.inc"
#include "../include/Entity.inc"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

struct ParticleShaderData
{
    float4 position;   // w = scale
    float4 velocity;   // w = final scale
    float4 color;
    float4 attributes; // x = lifetime
};

struct VSInput
{
    HYP_ATTRIBUTE float3 a_position : POSITION;
    HYP_ATTRIBUTE float3 a_normal : NORMAL;
    HYP_ATTRIBUTE float2 a_texcoord0 : TEXCOORD0;
};

struct VSOutput
{
    float4 position_cs : SV_POSITION;
    float2 texcoord0 : TEXCOORD0;
    float4 color : TEXCOORD1;
};

#ifndef MAX_PARTICLES
#define MAX_PARTICLES 1024
#endif

DECLARE_UAV(ParticleDescriptorSet, ParticlesBuffer) RWStructuredBuffer<ParticleShaderData> instances;

DECLARE_SRV(ParticleDescriptorSet, WorldsBuffer) StructuredBuffer<WorldShaderData> _worlds_buffer;
#define world_shader_data _worlds_buffer[0]

DECLARE_SRV_DYNAMIC(ParticleDescriptorSet, CamerasBuffer) StructuredBuffer<Camera> _cameras_buffer;
#define camera _cameras_buffer[0]

VSOutput VSMain(VSInput input, uint instanceId : SV_InstanceID)
{
    VSOutput output;

    const uint instance_index = instanceId;

    float4 position = float4(input.a_position, 1.0);

    ParticleShaderData instance = instances[instance_index];

    const float4 particle_position_world = instance.position;

    position.xyz *= particle_position_world.w;

    const float3 lookat_dir = normalize(camera.position.xyz - particle_position_world.xyz);

    const float3 lookat_z = lookat_dir;
    const float3 lookat_x = normalize(cross(float3(0.0, 1.0, 0.0), lookat_dir));
    const float3 lookat_y = normalize(cross(lookat_dir, lookat_x));

    position.xyz = lookat_x * position.x + lookat_y * position.y + lookat_z * position.z;
    position.xyz += particle_position_world.xyz;

    output.texcoord0 = float2(input.a_texcoord0.x, 1.0 - input.a_texcoord0.y);
    output.color = instance.color;

    output.position_cs = mul(camera.viewProjMat, position);

    return output;
}

#endif // VERTEX_SHADER

#ifdef PIXEL_SHADER

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../include/shared.inc"
#include "../include/material.inc"
#include "../include/packing.inc"
#include "../include/gbuffer.inc"
#include "../include/Entity.inc"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

struct PSInput
{
    float4 position_cs : SV_POSITION;
    float2 texcoord0 : TEXCOORD0;
    float4 color : TEXCOORD1;
};

struct PSOutput
{
    float4 gbuffer_albedo : SV_Target0;
    uint4 gbuffer_material : SV_Target2;
};

DECLARE_SRV(ParticleDescriptorSet, ParticleTexture) Texture2D ParticleTexture;
DECLARE_SAMPLER(ParticleDescriptorSet, SamplerLinear) SamplerState SamplerLinear;

PSOutput PSMain(PSInput input)
{
    PSOutput output;

    float4 color = SAMPLE_TEXTURE_2D(SamplerLinear, ParticleTexture, input.texcoord0);
    // color *= input.color;

    output.gbuffer_albedo = color;

    output.gbuffer_material.x = OBJECT_MASK_TRANSLUCENT;
    output.gbuffer_material.y = 0u;
    output.gbuffer_material.z = 0u;
    output.gbuffer_material.w = 0u;

    return output;
}

#endif // PIXEL_SHADER
