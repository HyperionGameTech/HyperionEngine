#include "../Include/Defines.hlsli"

#ifdef VERTEX_SHADER

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../Include/Scene.hlsli"
#include "../Include/Entity.hlsli"
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
    float3 position_ws : TEXCOORD2;
};

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
    output.position_ws = position.xyz;

    output.position_cs = mul(camera.viewProjMat, position);
    output.position_cs.xy += camera.jitter.xy * output.position_cs.w;

    return output;
}

#endif // VERTEX_SHADER

#ifdef PIXEL_SHADER

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../Include/Scene.hlsli"
#include "../Include/Shared.hlsli"
#include "../Include/Material.hlsli"
#include "../Include/Packing.hlsli"
#include "../Include/Gbuffer.hlsli"
#include "../Include/Entity.hlsli"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "../VolumetricFog/VolumetricFogCommon.hlsli"

struct PSInput
{
    float4 position_cs : SV_POSITION;
    float2 texcoord0 : TEXCOORD0;
    float4 color : TEXCOORD1;
    float3 position_ws : TEXCOORD2;
};

DECLARE_SRV(ParticleDescriptorSet, ParticleTexture) Texture2D ParticleTexture;
DECLARE_SAMPLER(ParticleDescriptorSet, SamplerLinear) SamplerState SamplerLinear;

DECLARE_SRV_DYNAMIC(ParticleDescriptorSet, CamerasBuffer) StructuredBuffer<Camera> _cameras_buffer;
#define camera _cameras_buffer[0]

DECLARE_SRV(ParticleDescriptorSet, VolumetricFogTexture) Texture3D<float4> VolumetricFogTexture;

DECLARE_BUFFER_DYNAMIC(ParticleDescriptorSet, ParticleFogConstants) cbuffer ParticleFogConstants
{
    VolumetricFogGrid volumetricFogGrid;
};

float4 PSMain(PSInput input) : SV_TARGET
{
    float4 color = SAMPLE_TEXTURE_2D(SamplerLinear, ParticleTexture, input.texcoord0);

    color.rgb = ApplyVolumetricFogSample(
        color.rgb,
        SampleVolumetricFog(VolumetricFogTexture, SamplerLinear, volumetricFogGrid, camera.viewProjMat, input.position_ws));

    return color;
}

#endif // PIXEL_SHADER
