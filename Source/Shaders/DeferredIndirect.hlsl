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

#if SSGI_ENABLED
DECLARE_SRV(DeferredPass, SSGIResultTexture) Texture2D SSGIResultTexture;
#endif // SSGI_ENABLED

#if SSR_ENABLED
DECLARE_SRV(DeferredPass, SSRResultTexture) Texture2D SSRResultTexture;
#endif // SSR_ENABLED

#if RT_REFLECTIONS
DECLARE_SRV(DeferredPass, RTRadianceResultTexture) Texture2D RTRadianceResultTexture;
#endif // RT_REFLECTIONS

#include "./include/gbuffer.inc"
#include "./include/material.inc"

#include "./include/scene.inc"

DECLARE_BUFFER(DeferredPass, WorldsBuffer) cbuffer WorldsBuffer
{
    WorldShaderData world_shader_data;
};

#include "./include/PhysicalCamera.inc"

#if RT_GI
DECLARE_SRV(DeferredPass, DDGIIrradianceTexture) Texture2D probe_irradiance;
DECLARE_SRV(DeferredPass, DDGIDepthTexture) Texture2D probe_depth;

#include "include/rt/probe/probe_uniforms.inc"

DECLARE_BUFFER(DeferredPass, DDGIConstants) cbuffer DDGI
{
    DDGIConstants ddgiConstants;
};

#include "include/rt/probe/SampleDDGI.hlsli"

#endif // RT_GI

#include "./include/env_probe.inc"

#if ENV_PROBE_CUBEMAP
DECLARE_SRV(DeferredPass, EnvProbesTexture) TextureCubeArray envProbesTexture;
#else // !ENV_PROBE_CUBEMAP
DECLARE_SRV(DeferredPass, EnvProbesTexture) Texture2DArray envProbesTexture;
#endif // ENV_PROBE_CUBEMAP

#define HYP_DEFERRED_NO_REFRACTION

#include "./deferred/DeferredLighting.hlsli"
#include "./deferred/ClusteredShading.hlsli"

#define DDGI_MULTIPLIER 1.0

DECLARE_BUFFER_DYNAMIC(DeferredPass, CBuffer) cbuffer CBuffer
{
    Camera camera;
};

float4 CalculateEnvProbeReflections(
    float3 positionVS, float3 positionWS, float3 N, float3 V, float3 R,
    float roughness, float perceptualRoughness,
    float2 texcoord, uint2 gbufferDimensions)
{
    const uint2 pixelCoord = uint2(texcoord * max(0, int2(gbufferDimensions) - 1));
    
    const uint gridIndex = Cluster_GetGridIndex(
        gbufferDimensions, pixelCoord,
        positionVS.z,
        camera.near, camera.far);

    const uint2 clusterData = ClusterGridBuffer[gridIndex];

    const uint clusterIndexOffset = clusterData.x;
    
    const uint numLights = (clusterData.y & 0xFFFF);
    const uint numEnvProbes = (clusterData.y >> 16) & 0xFFFF;
    
    float4 reflections = (float4)0;

    for (uint i = 0; i < numEnvProbes && reflections.a < 1.0; ++i)
    {
        const uint envProbeIndex = Cluster_LoadEnvProbeIndex(clusterIndexOffset, numLights, i);

        EnvProbe currentEnvProbe = EnvProbesBuffer.Load(envProbeIndex);
        
        const float numMips = 7.0; // assuming 128x128 cubemap size for reflection probes
        const float lod = perceptualRoughness * numMips;
        
        const float3 aabbMin = currentEnvProbe.aabb_min.xyz;
        const float3 aabbMax = currentEnvProbe.aabb_max.xyz;
        
        float4 currentReflections = (float4)0;

        // pre-convoluted from baking util
        ApplyReflectionProbe(
            currentEnvProbe.texture_index,
            currentEnvProbe.world_position.xyz,
            aabbMin,
            aabbMax,
            positionWS,
            R,
            lod,
            currentReflections);

        const float3 aabbExtent = aabbMax - aabbMin;

        const float3 blend = aabbExtent * 0.1;
        const float3 distToMin = (positionWS.xyz - aabbMin) / blend;
        const float3 distToMax = (aabbMax - positionWS.xyz) / blend;
        const float minBlend = min(distToMin.x, min(distToMin.y, min(distToMin.z, min(distToMax.x, min(distToMax.y, distToMax.z)))));

        float weight = smoothstep(0.0, 1.0, minBlend);
        currentReflections *= weight;

        reflections += currentReflections * (1.0 - reflections.a);
    }
    
    return reflections;
}

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

    const float perceptualRoughness = sqrt(roughness);
    
    reflections = CalculateEnvProbeReflections(
        positionVS.xyz, positionWS.xyz,
        N, V, R,
        roughness, perceptualRoughness,
        texcoord, gbufferDimensions);

#if SSR_ENABLED
    float4 ssrResult = SAMPLE_TEXTURE_2D_LOD(sampler_linear, SSRResultTexture, texcoord, 0);
    reflections = reflections * (1.0 - ssrResult.a) + ssrResult * ssrResult.a;
#endif // SSR_ENABLED

#if RT_REFLECTIONS
    CalculateRayTracingReflection(texcoord, reflections);
#endif // RT_REFLECTIONS

#if 0 // SH probes
    // Blend in SH probes
    // this will need to be reworked using tiling and per-tile linked lists or something similar.
    float3 blendedSH = (float3)0.0;
    float totalWeight = 0.0;

    for (uint probeIndex = 0; probeIndex < numFallbackProbes; probeIndex++)
    {
        const EnvProbe probe = fallbackEnvProbes[probeIndex];

        const float3 aabbMin = probe.aabb_min.xyz;
        const float3 aabbMax = probe.aabb_max.xyz;
        const float3 aabbExtent = aabbMax - aabbMin;

        const float3 blendZone = aabbExtent * 0.1;
        const float3 distToMin = (positionWS.xyz - aabbMin) / blendZone;
        const float3 distToMax = (aabbMax - positionWS.xyz) / blendZone;
        const float minBlend = min(distToMin.x, min(distToMin.y, min(distToMin.z,
                               min(distToMax.x, min(distToMax.y, distToMax.z)))));

        float weight = smoothstep(0.0, 1.0, minBlend);

        blendedSH += EnvProbeSH(probe, N, /* order */ 2) * weight;
        totalWeight += weight;
    }

    // start with fallback EnvProbe spherical harmonics.
    // alpha is zero so we can prioritize other GI methods if available, and lerp to the fallback SH if not.
    if (totalWeight > 0.0)
    {
        irradiance = float4(blendedSH / totalWeight, 0.0);
    }
    else if (numFallbackProbes > 0)
    {
        irradiance = float4(EnvProbeSH(fallbackEnvProbes[0], N, /* order */ 2), 0.0);
    }
#endif

#if SSGI_ENABLED
    // Blend ssgi result into irradiance - if no hit, alpha will be zero or close to it so we can lerp it
    float4 ssgi = SAMPLE_TEXTURE_2D_LOD(sampler_linear, SSGIResultTexture, texcoord, 0);
    irradiance = lerp(irradiance, ssgi, ssgi.a);
#endif

#if RT_GI
    float3 ddgi = DDGISampleIrradiance(positionWS.xyz, normal, V).rgb * DDGI_MULTIPLIER;
    // lerp to ddgi based on 1.0-ssgi alpha, so that if ssgi has a hit, it will be used, otherwise ddgi will be used
    irradiance.rgb = lerp(irradiance.rgb, ddgi, 1.0 - irradiance.a);
#endif

    irradiance.a = 1.0; // set alpha to 1 now that we're finished lerping between GI methods.

    const float NdotV = max(0.0001, dot(N, V));
    const float3 F0 = CalculateF0(albedo.rgb, metalness);
    const float3 F = CalculateFresnelTerm(F0, roughness, NdotV);
    const float3 dfg = CalculateDFG(F, roughness, NdotV);
    const float3 E = CalculateE(F0, dfg);
    float3 Fd = diffuse_color.rgb * irradiance.rgb * (1.0 - E) * ao;

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
