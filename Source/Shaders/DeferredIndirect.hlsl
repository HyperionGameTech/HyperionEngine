#include "./include/Defines.hlsli"

PERMUTE(SSGI_ENABLED);
PERMUTE(SSR_ENABLED);
PERMUTE(RT_GI);
PERMUTE(RT_REFLECTIONS);
PERMUTE(HBAO_ENABLED);
PERMUTE(REFLECTIONS_ONLY);

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

DECLARE_SRV(DeferredPass, GBufferAlbedoTexture) Texture2D GBufferAlbedoTexture;
DECLARE_SRV(DeferredPass, GBufferNormalsTexture) Texture2D GBufferNormalsTexture;
DECLARE_SRV(DeferredPass, GBufferMaterialTexture) Texture2D<uint> GBufferMaterialTexture;
DECLARE_SRV(DeferredPass, GBufferVelocityTexture) Texture2D GBufferVelocityTexture;

DECLARE_SRV(DeferredPass, GBufferMipChain) Texture2D GBufferMipChain;
DECLARE_SRV(DeferredPass, GBufferDepthTexture) Texture2D GBufferDepthTexture;

DECLARE_SRV(DeferredPass, SSAOResultTexture) Texture2D SSAOResultTexture;

#ifdef SSGI_ENABLED
DECLARE_SRV(DeferredPass, SSGIResultTexture) Texture2D SSGIResultTexture;
#endif // SSGI_ENABLED

#if defined(RT_REFLECTIONS) || defined(PATHTRACER)
DECLARE_SRV(DeferredPass, RTRadianceResultTexture) Texture2D RTRadianceResultTexture;
#endif // RT_REFLECTIONS

#include "./include/Gbuffer.hlsli"
#include "./include/Material.hlsli"

#include "./include/Scene.hlsli"

DECLARE_SRV(DeferredPass, WorldsBuffer) StructuredBuffer<WorldShaderData> _worlds_buffer;
#define world_shader_data _worlds_buffer[0]

#include "./include/PhysicalCamera.hlsli"

#ifdef RT_GI
DECLARE_SRV(DeferredPass, DDGIIrradianceTexture) Texture2D probe_irradiance;
DECLARE_SRV(DeferredPass, DDGIDepthTexture) Texture2D probe_depth;

#include "include/RayTracing/GlobalIllumination/ProbeUniforms.hlsli"

DECLARE_BUFFER(DeferredPass, DDGIConstants) cbuffer DDGI
{
    DDGIConstants ddgiConstants;
};

#include "include/RayTracing/GlobalIllumination/SampleDDGI.hlsli"

#endif // RT_GI

#include "./include/EnvProbes.hlsli"
#include "./include/Noise.hlsli"

DECLARE_SRV(DeferredPass, EnvProbesColorTexture) TextureCubeArray envProbesColorTexture;
DECLARE_SRV(DeferredPass, EnvProbesDepthTexture) TextureCubeArray<float2> envProbesDepthTexture;

#define HYP_DEFERRED_NO_REFRACTION

DECLARE_SRV(DeferredPass, EnvProbesBuffer) StructuredBuffer<EnvProbe> EnvProbesBuffer;
DECLARE_SRV(DeferredPass, LightsBuffer) StructuredBuffer<Light> LightsBuffer;
DECLARE_SRV(DeferredPass, ClusterGridBuffer) ByteAddressBuffer ClusterGridBuffer;
DECLARE_SRV(DeferredPass, ClusterIndexBuffer) ByteAddressBuffer ClusterIndexBuffer;

// Keep here even if unused; Having shadow maps here means the render pass won't need to be broken between
// the indirect and direct passes.
DECLARE_SRV(DeferredPass, ShadowMapsTextureArray) Texture2DArray<float> shadow_maps;
DECLARE_SRV(DeferredPass, PointLightShadowMapsTextureArray) TextureCubeArray point_shadow_maps;

#include "./deferred/ClusteredShading.hlsli"

// matches SkyVisibilityShaderData in Rendering/Passes/SkyVisibilityPass.hpp
struct SkyVisibilityCapture
{
    float4x4 viewProjMatrix;
    float4 params;
};

DECLARE_BUFFER_DYNAMIC(DeferredPass, CBuffer) cbuffer CBuffer
{
    Camera camera;
    EnvProbe skyProbe;
    SkyVisibilityCapture skyVisibilityCapture;
};

// top-down depth capture of everything that blocks the sky, from DynamicSkySystem
DECLARE_SRV(DeferredPass, SkyVisibilityTexture) Texture2D SkyVisibilityTexture;
DECLARE_SAMPLER(DeferredPass, SamplerShadow) SamplerComparisonState SamplerShadow;

static const float2 s_skyVisibilityKernel[8] = {
    float2(-0.94201624, -0.39906216),
    float2(0.94558609, -0.76890725),
    float2(-0.094184101, -0.92938870),
    float2(0.34495938, 0.29387760),
    float2(-0.91588581, 0.45771432),
    float2(-0.38277543, 0.27676845),
    float2(0.97484398, 0.75648379),
    float2(0.44323325, -0.97511554)
};

float EvaluateSkyVisibility(float3 positionWS, float3 N, float2 texcoord)
{
    const float strength = world_shader_data.sky_occlusion_params.x;

    if (strength <= 0.0 || skyVisibilityCapture.params.z == 0.0)
    {
        return 1.0;
    }

    const float worldExtent = skyVisibilityCapture.params.x;
    const float depthRange = skyVisibilityCapture.params.y;

    const float filterRadius = world_shader_data.sky_occlusion_params.y;
    const float baseBias = world_shader_data.sky_occlusion_params.z;

    // a sloped surface rises by this much across the filter, and would otherwise shadow itself
    const float slopeTangent = min(sqrt(saturate(1.0 - N.y * N.y)) / max(N.y, 0.15), 1.0) * saturate(N.y);

    const float3 samplePosition = positionWS + N * (baseBias + filterRadius * slopeTangent * 0.5);

    const float4 capturePosition = mul(skyVisibilityCapture.viewProjMatrix, float4(samplePosition, 1.0));
    const float3 captureNdc = capturePosition.xyz / capturePosition.w;

    if (any(abs(captureNdc.xy) > 1.0) || captureNdc.z < 0.0 || captureNdc.z > 1.0)
    {
        return 1.0;
    }

    const float2 captureUv = captureNdc.xy * float2(0.5, -0.5) + 0.5;
    const float sampleRadius = filterRadius / worldExtent;

    // smaller depth is closer to the capture camera, which looks straight down
    const float referenceDepth = captureNdc.z - (baseBias + filterRadius * slopeTangent) / depthRange;

    // rotated per pixel so the tap pattern reads as noise rather than as banding
    const float rotation = InterleavedGradientNoise(texcoord * float2(camera.dimensions.xy) - 0.5) * HYP_FMATH_TWO_PI;
    float rotationSin, rotationCos;
    sincos(rotation, rotationSin, rotationCos);

    const float2x2 rotationMatrix = float2x2(rotationCos, -rotationSin, rotationSin, rotationCos);

    uint2 captureDimensions;
    SkyVisibilityTexture.GetDimensions(captureDimensions.x, captureDimensions.y);

    // occluders fade in over this distance above the reference instead of flipping on a hard depth test
    const float softnessDepth = max(baseBias, 0.25) / depthRange;

    float openSum = 0.0;

    [unroll]
    for (uint i = 0; i < 8; i++)
    {
        const float2 offset = mul(s_skyVisibilityKernel[i], rotationMatrix) * sampleRadius;

        const float2 texelPosition = (captureUv + offset) * float2(captureDimensions) - 0.5;
        const float2 texelFraction = frac(texelPosition);
        const float2 gatherUv = (floor(texelPosition) + 1.0) / float2(captureDimensions);

        const float4 occluderDepths = SkyVisibilityTexture.GatherRed(sampler_nearest, gatherUv);
        const float4 open = saturate((occluderDepths - referenceDepth) / softnessDepth + 1.0);
        
        openSum += lerp(lerp(open.w, open.z, texelFraction.x), lerp(open.x, open.y, texelFraction.x), texelFraction.y);
    }

    // fade out at the edge of the capture so occlusion doesn't stop dead where the map ends
    const float2 edgeDistance = 1.0 - abs(captureNdc.xy);
    const float edgeFade = saturate(min(edgeDistance.x, edgeDistance.y) * 8.0);

    return lerp(1.0, openSum / 8.0, strength * edgeFade);
}

#ifdef REFLECTIONS_ONLY
#define HYP_DEFERRED_NO_PROBE_IRRADIANCE
#elif defined(SSR_ENABLED) && !defined(RT_REFLECTIONS)
#define HYP_DEFERRED_NO_PROBE_REFLECTIONS
#endif

#define DEFERRED_LIGHTING_HAS_SKY
#include "./deferred/DeferredLighting.hlsli"
#undef DEFERRED_LIGHTING_HAS_SKY

#undef HYP_DEFERRED_NO_PROBE_IRRADIANCE
#undef HYP_DEFERRED_NO_PROBE_REFLECTIONS

#define DDGI_MULTIPLIER 1.0

// #define DEBUG_NORMALS

PSOutput PSMain(PSInput input)
{
    PSOutput output;
    float3 result = (float3)0.0;

    float2 texcoord = input.texcoord;

    uint2 gbufferDimensions;
    GBufferAlbedoTexture.GetDimensions(gbufferDimensions.x, gbufferDimensions.y);

    const uint2 pixelCoord = uint2(texcoord * gbufferDimensions);

    float4 albedo = SAMPLE_TEXTURE_2D_LOD(sampler_nearest, GBufferAlbedoTexture, texcoord, 0);
    float4 normalSample = SAMPLE_TEXTURE_2D_LOD(sampler_nearest, GBufferNormalsTexture, texcoord, 0);
    float3 normal = GBufferUnpackNormal(normalSample);

    float depth = SAMPLE_TEXTURE_2D_LOD(sampler_nearest, GBufferDepthTexture, texcoord, 0).r;

    // texcoord y runs opposite to NDC y (see ReconstructViewSpacePositionFromDepth)
    float2 unjitteredTexcoord = texcoord - float2(camera.jitter.x, -camera.jitter.y) * 0.5;
    float4 positionVS = ReconstructViewSpacePositionFromDepth(camera.invProjMat, unjitteredTexcoord, depth);

    float4 positionWS = mul(camera.invViewMat, positionVS);
    positionWS /= positionWS.w;

    const uint materialBits = GBufferMaterialTexture.Load(int3(pixelCoord, 0));

    GBufferMaterialParams materialParams;
    GBufferUnpackMaterialParams(normalSample.x, materialBits >> 28u, materialParams);

    const float roughness = clamp(materialParams.roughness, 0.001, 0.999);
    const float metalness = materialParams.metalness;
    const uint mask = materialParams.mask;

    if ((mask & OBJECT_MASK_UNLIT) != 0)
    {
#ifdef REFLECTIONS_ONLY
        output.output_color = (float4)0.0;
#else
        output.output_color = float4(albedo.rgb, 1.0);
#endif

        return output;
    }

    const float perceptualRoughness = sqrt(roughness);

    float3 N = normalize(normal);
    float3 V = normalize(camera.position.xyz - positionWS.xyz);
    float3 R = normalize(reflect(-V, N));
    
    // material ambient occlusion rides in gbuffer albedo alpha; screen-space AO can only resolve a few pixels of radius,
    // so large surfaces like terrain depend on this to darken their own creases and hollows
    float ao = albedo.a;
    float4 irradiance = (float4)0.0;
    float4 reflections = (float4)0.0;

#if HBAO_ENABLED || SSAO_ENABLED
    const float4 ssao_data = SAMPLE_TEXTURE_2D_LOD(sampler_linear, SSAOResultTexture, texcoord, 0);
    ao *= ssao_data.r;
#endif

    const uint2 viewportExtent = camera.dimensions.xy;

    g_skyVisibility = EvaluateSkyVisibility(positionWS.xyz, N, texcoord);

    EvaluateEnvProbes(
        positionVS.xyz, positionWS.xyz,
        N, V, R,
        camera.near, camera.far,
        roughness, perceptualRoughness,
        texcoord, viewportExtent,
        mask,
        /* inout */ reflections,
        /* inout */ irradiance);

    reflections.a = saturate(reflections.a);

#ifdef RT_REFLECTIONS
    CalculateRayTracingReflection(texcoord, reflections);
#endif // RT_REFLECTIONS

#ifdef REFLECTIONS_ONLY
    output.output_color = float4(reflections.rgb * reflections.a, reflections.a);

    return output;
#endif // REFLECTIONS_ONLY

    irradiance.a = saturate(irradiance.a);

    // lightmapped pixels get diffuse GI from the lightmap pass, same masking as the probes
    const float lightmappedWeight = min(1.0, float(mask & OBJECT_MASK_LIGHTMAPPED));

#ifdef SSGI_ENABLED
    // Blend ssgi result into irradiance - if no hit, alpha will be zero or close to it so we can lerp it
    float4 ssgi = SAMPLE_TEXTURE_2D_LOD(sampler_linear, SSGIResultTexture, texcoord, 0);
    irradiance = lerp(irradiance, ssgi, ssgi.a * (1.0 - lightmappedWeight));
#else
    float4 ssgi = (float4)0.0;
#endif

#ifdef RT_GI
    float4 ddgi = DDGISampleIrradiance(positionWS.xyz, normal, V);
    ddgi.rgb *= DDGI_MULTIPLIER;
    // lerp to ddgi based on 1.0-ssgi alpha, so that if ssgi has a hit, it will be used, otherwise ddgi will be used.
    // ddgi alpha fades out past the last cascade, falling back to env probe irradiance.
    irradiance = lerp(irradiance, ddgi, (1.0 - ssgi.a) * ddgi.a * (1.0 - lightmappedWeight));
#endif

    const float NdotV = max(HYP_FMATH_EPSILON, dot(N, V));
    
    const float3 diffuse_color = CalculateDiffuseColor(albedo.rgb, metalness);
    const float3 F0 = CalculateF0(albedo.rgb, metalness);
    const float3 dfg = CalculateDFG(perceptualRoughness, NdotV);
    const float3 E = CalculateE(F0, dfg);
    float3 Fd = diffuse_color.rgb * irradiance.rgb * (1.0 - E) * ao;

#ifdef SSR_ENABLED
    result = Fd;
#else
    float3 specular_ao = (float3)SpecularAO_Lagarde(NdotV, ao, perceptualRoughness);

    const float3 energy_compensation = CalculateEnergyCompensation(F0, dfg);
    specular_ao *= energy_compensation;

    reflections.rgb *= specular_ao;
    // scale by alpha, so partially covered pixels don't receive the full reflection.
    float3 Fr = E * reflections.rgb * reflections.a;

    result = Fd + Fr;
#endif // SSR_ENABLED

#ifdef PATHTRACER
    result = CalculatePathTracing(texcoord).rgb;
#elif defined(DEBUG_REFLECTIONS)
    result = E * reflections.rgb * reflections.a;
#elif defined(DEBUG_IRRADIANCE)
    result = irradiance.rgb;
#elif defined(DEBUG_VELOCITY)
    float4 velocity = SAMPLE_TEXTURE_2D_LOD(sampler_linear, GBufferVelocityTexture, texcoord, 0);
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
