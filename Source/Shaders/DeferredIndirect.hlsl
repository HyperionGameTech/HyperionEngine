#include "./include/defines.inc"

PERMUTE(SSGI_ENABLED);
PERMUTE(SSR_ENABLED);
PERMUTE(RT_GI);
PERMUTE(RT_REFLECTIONS);
PERMUTE(HBAO_ENABLED);

STATIC(TILE_Z_BINS, 16);
STATIC(TILE_SIZE, 32);

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

DECLARE_SAMPLER(DeferredPass, SamplerNearest) SamplerState sampler_nearest;
DECLARE_SAMPLER(DeferredPass, SamplerLinear) SamplerState sampler_linear;

DECLARE_SRV(DeferredPass, GBufferAlbedoTexture) Texture2D gbuffer_albedo_texture;
DECLARE_SRV(DeferredPass, GBufferNormalsTexture) Texture2D gbuffer_normals_texture;
DECLARE_SRV(DeferredPass, GBufferMaterialTexture) Texture2D<uint4> gbuffer_material_texture;
DECLARE_SRV(DeferredPass, GBufferVelocityTexture) Texture2D gbuffer_velocity_texture;

DECLARE_SRV(DeferredPass, GBufferMipChain) Texture2D gbuffer_mip_chain;
DECLARE_SRV(DeferredPass, GBufferDepthTexture) Texture2D gbuffer_depth_texture;

DECLARE_SRV(DeferredPass, SSAOResultTexture) Texture2D SSAOResultTexture;

#ifdef SSGI_ENABLED
DECLARE_SRV(DeferredPass, SSGIResultTexture) Texture2D SSGIResultTexture;
#endif // SSGI_ENABLED

#ifdef SSR_ENABLED
DECLARE_SRV(DeferredPass, SSRResultTexture) Texture2D SSRResultTexture;
#endif // SSR_ENABLED

#if defined(RT_REFLECTIONS) || defined(PATHTRACER)
DECLARE_SRV(DeferredPass, RTRadianceResultTexture) Texture2D RTRadianceResultTexture;
#endif // RT_REFLECTIONS

#include "./include/gbuffer.inc"
#include "./include/material.inc"

#include "./include/scene.inc"

DECLARE_SRV(DeferredPass, WorldsBuffer) StructuredBuffer<WorldShaderData> _worlds_buffer;
#define world_shader_data _worlds_buffer[0]

#include "./include/PhysicalCamera.inc"

#ifdef RT_GI
DECLARE_SRV(DeferredPass, DDGIIrradianceTexture) Texture2D probe_irradiance;
DECLARE_SRV(DeferredPass, DDGIDepthTexture) Texture2D probe_depth;

#include "include/rt/probe/probe_uniforms.inc"

DECLARE_BUFFER(DeferredPass, DDGIConstants) cbuffer DDGI
{
    DDGIConstants ddgiConstants;
};

#include "include/rt/probe/SampleDDGI.hlsli"

#endif // RT_GI

#include "./include/EnvProbes.hlsli"

#if ENV_PROBE_CUBEMAP
DECLARE_SRV(DeferredPass, EnvProbesTexture) TextureCubeArray envProbesTexture;
#else // !ENV_PROBE_CUBEMAP
DECLARE_SRV(DeferredPass, EnvProbesTexture) Texture2DArray envProbesTexture;
#endif // ENV_PROBE_CUBEMAP

#define HYP_DEFERRED_NO_REFRACTION

DECLARE_SRV(DeferredPass, EnvProbesBuffer) StructuredBuffer<EnvProbe> EnvProbesBuffer;
DECLARE_SRV(DeferredPass, LightsBuffer) StructuredBuffer<Light> LightsBuffer;
DECLARE_SRV_DYNAMIC(DeferredPass, ClusterGridBuffer) StructuredBuffer<uint2> ClusterGridBuffer;
DECLARE_SRV_DYNAMIC(DeferredPass, ClusterIndexBuffer) ByteAddressBuffer ClusterIndexBuffer;

#include "./deferred/ClusteredShading.hlsli"
#include "./deferred/DeferredLighting.hlsli"

#define DDGI_MULTIPLIER 1.0

DECLARE_BUFFER_DYNAMIC(DeferredPass, CBuffer) cbuffer CBuffer
{
    Camera camera;
};

PSOutput PSMain(PSInput input)
{
    PSOutput output;

    float2 texcoord = input.texcoord;

    uint2 gbufferDimensions;
    gbuffer_albedo_texture.GetDimensions(gbufferDimensions.x, gbufferDimensions.y);

    const uint2 pixelCoord = uint2(texcoord * max(0, int2(gbufferDimensions) - 1));

    float4 albedo = SAMPLE_TEXTURE_2D_LOD(sampler_nearest, gbuffer_albedo_texture, texcoord, 0);
    float4 normalSample = SAMPLE_TEXTURE_2D_LOD(sampler_nearest, gbuffer_normals_texture, texcoord, 0);
    float3 normal = GBufferUnpackNormal(normalSample);

    float depth = SAMPLE_TEXTURE_2D_LOD(sampler_nearest, gbuffer_depth_texture, texcoord, 0).r;
    
    float4 positionVS = ReconstructViewSpacePositionFromDepth(camera.invProjMat, texcoord, depth);
    
    float4 positionWS = mul(camera.invViewMat, positionVS);
    positionWS /= positionWS.w;

    uint2 materialData = gbuffer_material_texture.Load(int3(pixelCoord, 0)).xy;

    GBufferMaterialParams materialParams;
    GBufferUnpackMaterialParams(normalSample.x, materialData.x, materialParams);

    const float roughness = materialParams.roughness;
    const float metalness = materialParams.metalness;
    const uint mask = materialParams.mask;
    
    const float perceptualRoughness = sqrt(roughness);

    float3 result = (float3)0.0;

    float3 N = normalize(normal);
    float3 V = normalize(camera.position.xyz - positionWS.xyz);
    float3 R = normalize(reflect(-V, N));

    float ao = 1.0;
    float4 irradiance = (float4)0.0;
    float4 reflections = (float4)0.0;
    float3 ibl = (float3)0.0;

#if HBAO_ENABLED || SSAO_ENABLED
    const float4 ssao_data = SAMPLE_TEXTURE_2D_LOD(sampler_linear, SSAOResultTexture, texcoord, 0);
    ao = ssao_data.r;
#endif

    const float3 diffuse_color = CalculateDiffuseColor(albedo.rgb, metalness);
    
    CalculateEnvProbesContribution(
        positionVS.xyz, positionWS.xyz,
        N, V, R,
        camera.near, camera.far,
        roughness, perceptualRoughness,
        texcoord, gbufferDimensions,
        /* inout */ reflections,
        /* inout */ irradiance);

    // set irradiance to zero for this pixel if it is in the lightmap bucket
    // (don't want to double apply indirect contribution approximations)
    irradiance *= 1.0 - min(1.0, float(mask & OBJECT_MASK_LIGHTMAPPED));
    irradiance.a = 0.0;

#ifdef SSR_ENABLED
    float4 ssrResult = SAMPLE_TEXTURE_2D_LOD(sampler_linear, SSRResultTexture, texcoord, 0);
    reflections = (reflections * (1.0 - ssrResult.a)) + (ssrResult * ssrResult.a);
#endif // SSR_ENABLED

#ifdef RT_REFLECTIONS
    CalculateRayTracingReflection(texcoord, reflections);
#endif // RT_REFLECTIONS

#ifdef SSGI_ENABLED
    // Blend ssgi result into irradiance - if no hit, alpha will be zero or close to it so we can lerp it
    float4 ssgi = SAMPLE_TEXTURE_2D_LOD(sampler_linear, SSGIResultTexture, texcoord, 0);
    irradiance = lerp(irradiance, ssgi, ssgi.a);
#endif

#ifdef RT_GI
    float4 ddgi = DDGISampleIrradiance(positionWS.xyz, normal, V) * DDGI_MULTIPLIER;
    // lerp to ddgi based on 1.0-ssgi alpha, so that if ssgi has a hit, it will be used, otherwise ddgi will be used
    irradiance = lerp(irradiance, ddgi, 1.0 - irradiance.a);
#endif

    irradiance.rgb *= irradiance.a;
    irradiance.a = 1.0; // set alpha to 1 now that we're finished lerping between GI methods.

    const float NdotV = max(0.0001, dot(N, V));
    const float3 F0 = CalculateF0(albedo.rgb, metalness);
    const float3 F = CalculateFresnelTerm(F0, perceptualRoughness, NdotV);
    const float3 dfg = CalculateDFG(F, perceptualRoughness, NdotV);
    const float3 E = CalculateE(F0, dfg);
    float3 Fd = diffuse_color.rgb * irradiance.rgb * (1.0 - E) * ao;

    float3 specular_ao = (float3)SpecularAO_Lagarde(NdotV, ao, roughness);

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
    float4 velocity = SAMPLE_TEXTURE_2D_LOD(sampler_linear, gbuffer_velocity_texture, texcoord, 0);
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
