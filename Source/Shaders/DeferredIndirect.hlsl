#include "./include/defines.inc"

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
    float2 texcoord : TEXCOORD0;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;

    float4 position = float4(input.a_position, 1.0);

    output.position = position.xyz;
    output.texcoord = input.a_texcoord0;

    output.position_cs = position;

    return output;
}

#endif // VERTEX_SHADER

#ifdef PIXEL_SHADER

struct PSInput
{
    float4 position_cs : SV_POSITION;
    float3 position : POSITION;
    float2 texcoord : TEXCOORD0;
};

struct PSOutput
{
    float4 output_color : SV_Target0;
};

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

DECLARE_SRV(DeferredPass, GBufferAlbedoTexture) Texture2D gbuffer_albedo_texture;
DECLARE_SRV(DeferredPass, GBufferNormalsTexture) Texture2D gbuffer_normals_texture;
DECLARE_SRV(DeferredPass, GBufferMaterialTexture) Texture2D<uint4> gbuffer_material_texture;
DECLARE_SRV(DeferredPass, GBufferVelocityTexture) Texture2D gbuffer_velocity_texture;

DECLARE_SRV(DeferredPass, GBufferMipChain) Texture2D gbuffer_mip_chain;
DECLARE_SRV(DeferredPass, GBufferDepthTexture) Texture2D gbuffer_depth_texture;
DECLARE_SAMPLER(DeferredPass, SamplerNearest) SamplerState sampler_nearest;
DECLARE_SAMPLER(DeferredPass, SamplerLinear) SamplerState sampler_linear;

DECLARE_SRV(DeferredPass, SSAOResultTexture) Texture2D ssao_gi_result;
DECLARE_SRV(DeferredPass, SSGIResultTexture) Texture2D ssgi_result;
DECLARE_SRV(DeferredPass, RTRadianceResultTexture) Texture2D rt_radiance_final;
DECLARE_SRV(DeferredPass, ReflectionProbeResultTexture) Texture2D reflections_texture;

#include "./include/gbuffer.inc"
#include "./include/material.inc"

#include "./include/scene.inc"
DECLARE_BUFFER_DYNAMIC(DeferredPass, CamerasBuffer) cbuffer CamerasBuffer
{
    Camera camera;
};

DECLARE_BUFFER(DeferredPass, WorldsBuffer) cbuffer WorldsBuffer
{
    WorldShaderData world_shader_data;
};

#include "./include/PhysicalCamera.inc"

#define HYP_VCT_REFLECTIONS_ENABLED 1
#define HYP_VCT_INDIRECT_ENABLED 1

#if RT_GI
DECLARE_SRV(DeferredPass, DDGIIrradianceTexture) Texture2D probe_irradiance;
DECLARE_SRV(DeferredPass, DDGIDepthTexture) Texture2D probe_depth;

#include "include/rt/probe/probe_uniforms.inc"

DECLARE_BUFFER(DeferredPass, DDGIConstants) cbuffer DDGI
{
    DDGIConstants ddgiConstants;
};

#include "include/rt/probe/SampleDDGI.hlsli"

#endif

#include "./include/env_probe.inc"
DECLARE_BUFFER_DYNAMIC(DeferredPass, EnvGridsBuffer) cbuffer EnvGridsBuffer
{
    EnvGrid env_grid;
};

#define HYP_DEFERRED_NO_REFRACTION
#define HYP_DEFERRED_NO_ENV_PROBE
#include "./deferred/DeferredLighting.inc"
#undef HYP_DEFERRED_NO_REFRACTION
#undef HYP_DEFERRED_NO_ENV_PROBE

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#define DDGI_MULTIPLIER 1.0

PSOutput PSMain(PSInput input)
{
    PSOutput output;

    float2 texcoord = input.texcoord;

    uint2 gbufferDimensions;
    gbuffer_albedo_texture.GetDimensions(gbufferDimensions.x, gbufferDimensions.y);

    const uint2 pixelCoord = uint2(texcoord * max(0, int2(gbufferDimensions) - 1));

    float4 albedo = SAMPLE_TEXTURE_2D(sampler_nearest, gbuffer_albedo_texture, texcoord);
    float4 normalSample = SAMPLE_TEXTURE_2D(sampler_nearest, gbuffer_normals_texture, texcoord);
    float3 normal = GBufferUnpackNormal(normalSample);

    float depth = SAMPLE_TEXTURE_2D(sampler_nearest, gbuffer_depth_texture, texcoord).r;
    float4 position = ReconstructWorldSpacePositionFromDepth(camera.invProjMat, camera.invViewMat, texcoord, depth);

    uint2 materialData = gbuffer_material_texture.Load(int3(pixelCoord, 0)).xy;

    GBufferMaterialParams materialParams;
    GBufferUnpackMaterialParams(normalSample.x, materialData.x, materialParams);

    const float roughness = materialParams.roughness;
    const float metalness = materialParams.metalness;
    const uint mask = materialParams.mask;

    float3 result = float3(0.0, 0.0, 0.0);

    float3 N = normalize(normal);
    float3 V = normalize(camera.position.xyz - position.xyz);
    float3 R = normalize(reflect(-V, N));

    float ao = 1.0;
    float3 irradiance = float3(0.0, 0.0, 0.0);
    float4 reflections = float4(0.0, 0.0, 0.0, 0.0);
    float3 ibl = float3(0.0, 0.0, 0.0);

#if HBAO_ENABLED
    const float4 ssao_data = SAMPLE_TEXTURE_2D(HYP_SAMPLER_NEAREST, ssao_gi_result, texcoord);
    ao = ssao_data.a;
#endif

    const float3 diffuse_color = CalculateDiffuseColor(albedo.rgb, metalness);

    const float perceptual_roughness = sqrt(roughness);

    reflections = SAMPLE_TEXTURE_2D(HYP_SAMPLER_LINEAR, reflections_texture, texcoord);

#if RT_REFLECTIONS
    CalculateRayTracingReflection(texcoord, reflections);
#endif

#if RT_GI
    irradiance += DDGISampleIrradiance(position.xyz, normal, V).rgb * DDGI_MULTIPLIER;
#endif

#if SSGI_ENABLED
    const float4 ssgi = SAMPLE_TEXTURE_2D(HYP_SAMPLER_LINEAR, ssgi_result, texcoord);
    irradiance = irradiance * (1.0 - ssgi.a) + (ssgi.rgb * ssgi.a);
#endif

#if HBIL_ENABLED
    CalculateHBILIrradiance(ssao_data, irradiance);
#endif

    const float NdotV = max(0.0001, dot(N, V));
    const float3 F0 = CalculateF0(albedo.rgb, metalness);
    const float3 F = CalculateFresnelTerm(F0, roughness, NdotV);
    const float3 dfg = CalculateDFG(F, roughness, NdotV);
    const float3 E = CalculateE(F0, dfg);
    float3 Fd = diffuse_color.rgb * irradiance * (1.0 - E) * ao;

    float3 specular_ao = float3(SpecularAO_Lagarde(NdotV, ao, roughness), SpecularAO_Lagarde(NdotV, ao, roughness), SpecularAO_Lagarde(NdotV, ao, roughness));

    const float3 energy_compensation = CalculateEnergyCompensation(F0, dfg);
    specular_ao *= energy_compensation;

    float3 Fr = ibl * E * specular_ao;

    reflections.rgb *= specular_ao;
    Fr = Fr * (1.0 - reflections.a) + (E * reflections.rgb);

    result = Fd + Fr;

#ifdef PATHTRACER
    result = CalculatePathTracing(texcoord).rgb;
#elif defined(DEBUG_REFLECTIONS)
    result = E * reflections.rgb;
#elif defined(DEBUG_IRRADIANCE)
    result = irradiance.rgb;
#elif defined(DEBUG_VELOCITY)
    float4 velocity = SAMPLE_TEXTURE_2D(sampler_nearest, gbuffer_velocity_texture, texcoord);
    result = velocity.rgb;
#elif defined(DEBUG_NORMALS)
    result = normal * 0.5 + 0.5;
#elif defined(DEBUG_AO)
    result = float3(ao, ao, ao);
#endif

    output.output_color = float4(result, 1.0);

    return output;
}

#endif // PIXEL_SHADER
