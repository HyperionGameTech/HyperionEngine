#include "../include/Defines.hlsli"

STATIC(MAX_LIGHTS, 4);


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
    float4 positionNdc : TEXCOORD0;
};

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "../include/Scene.hlsli"
#include "../include/Shared.hlsli"

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "./FogVolume.inl"

DECLARE_SRV_DYNAMIC(FogVolume, CamerasBuffer) StructuredBuffer<Camera> _cameras_buffer;
#define camera _cameras_buffer[0]

DECLARE_BUFFER_DYNAMIC(FogVolume, FogVolumeConstants) cbuffer FogVolumeConstants
{
    FogVolume fogVolume;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;

    float4 position = mul(fogVolume.transformMatrix, float4(input.a_position, 1.0));
    output.position = position.xyz / position.w;

    float4x4 jitterMat = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };
    jitterMat[0][3] += camera.jitter.x;
    jitterMat[1][3] += camera.jitter.y;

    output.positionNdc = mul(jitterMat, mul(camera.viewProjMat, float4(output.position, 1.0)));

    output.position_cs = output.positionNdc;

    return output;
}

#endif // VERTEX_SHADER

#ifdef PIXEL_SHADER

struct PSInput
{
    float4 position_cs : SV_POSITION;
    float3 position : POSITION;
    float4 positionNdc : TEXCOORD0;
};

struct PSOutput
{
    float4 gbuffer_albedo : SV_Target0;
    float4 gbuffer_normals : SV_Target1;
    uint4 gbuffer_material : SV_Target2;
    float2 gbuffer_velocity : SV_Target3;
};

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

DECLARE_SAMPLER(FogVolume, SamplerLinear) SamplerState SamplerLinear;
DECLARE_SAMPLER(FogVolume, SamplerNearest) SamplerState SamplerNearest;

#define texture_sampler SamplerLinear
#define sampler_linear SamplerLinear
#define sampler_nearest SamplerNearest
#define HYP_SAMPLER_NEAREST SamplerNearest
#define HYP_SAMPLER_LINEAR SamplerLinear

DECLARE_SRV(FogVolume, DepthPyramidTexture) Texture2D DepthPyramidTexture;

#include "../include/Scene.hlsli"
#include "../include/Material.hlsli"
#include "../include/Entity.hlsli"
#include "../include/Packing.hlsli"
#include "../include/Shared.hlsli"

#include "../include/Gbuffer.hlsli"

DECLARE_SRV_DYNAMIC(FogVolume, CamerasBuffer) StructuredBuffer<Camera> _cameras_buffer;
#define camera _cameras_buffer[0]

DECLARE_SRV(FogVolume, ShadowMapsTextureArray) Texture2DArray<float> shadow_maps;
DECLARE_SRV(FogVolume, PointLightShadowMapsTextureArray) TextureCubeArray point_shadow_maps;

#include "../include/BRDF.hlsli"

#define HYP_DEFERRED_NO_REFRACTION
#define HYP_DEFERRED_NO_ENV_PROBE

#include "./DeferredLighting.hlsli"

#undef HYP_DEFERRED_NO_REFRACTION
#undef HYP_DEFERRED_NO_ENV_PROBE

#include "../include/Shadows.hlsli"

#ifndef CURRENT_MATERIAL
#define CURRENT_MATERIAL material
#endif

#include "./FogVolume.inl"

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

DECLARE_SRV(FogVolume, NoiseMap) Texture3D<float> NoiseMap;
DECLARE_BUFFER_DYNAMIC(FogVolume, FogVolumeConstants) cbuffer FogVolumeConstants
{
    FogVolume fogVolume;
    Light lights[MAX_LIGHTS];
    ShadowMap shadowMaps[MAX_LIGHTS];
};

float2 RayBoxIntersect(float3 rayOrigin, float3 rayDir, float3 boxMin, float3 boxMax)
{
    float3 invDir = 1.0 / rayDir;
    float3 tMin = (boxMin - rayOrigin) * invDir;
    float3 tMax = (boxMax - rayOrigin) * invDir;

    float3 t1 = min(tMin, tMax);
    float3 t2 = max(tMin, tMax);

    float tNear = max(max(t1.x, t1.y), t1.z);
    float tFar = min(min(t2.x, t2.y), t2.z);

    return float2(tNear, tFar);
}

float3 WorldToLocal(float3 positionWS, float3 aabbMin, float3 aabbMax)
{
    return (positionWS - aabbMin) / (aabbMax - aabbMin);
}

float3 LocalToTexCoord(float3 positionLS)
{
    return clamp(positionLS, float3(0.0, 0.0, 0.0), float3(1.0, 1.0, 1.0));
}

float3 WorldToTexCoord(float3 positionWS, float3 aabbMin, float3 aabbMax)
{
    return clamp(LocalToTexCoord(WorldToLocal(positionWS, aabbMin, aabbMax)), float3(0.0, 0.0, 0.0), float3(1.0, 1.0, 1.0));
}

float HenyeyGreenstein(float g, float cosTheta)
{
    float g2 = g * g;

    float denom = 1.0 + g2 - 2.0 * g * cosTheta;
    denom = pow(denom, 1.5);

    float num = 1.0 - g2;

    return (1.0 / (4.0 * HYP_FMATH_PI)) * (num / max(denom, 0.0001));
}

float GetFogDensity(float3 uvw)
{
    return SAMPLE_TEXTURE_3D(texture_sampler, NoiseMap, uvw).r;
}

float4 RayMarch(float3 rayOrigin, float3 rayDir, float tNear, float tFar, float stepSize)
{
    float t = tNear;
    const int maxSteps = 128;

    float transmittance = 1.0;
    float3 accumulatedColor = float3(0.0, 0.0, 0.0);

    // @TODO Make these configurable
    const float DensityScale = 0.3;
    const float Scattering = 0.2 * DensityScale;
    const float Absorption = 0.2 * DensityScale;
    const float Extinction = Scattering + Absorption;
    const float phaseG = 0.8;

    const float3 AmbientLight = float3(0.12, 0.12, 0.12);

    float3 materialColor = float3(1.0, 1.0, 1.0);

    float3 albedo = (Extinction > 1e-6) ? float3(Scattering / Extinction, Scattering / Extinction, Scattering / Extinction) : float3(0.0, 0.0, 0.0);

    for (int i = 0; i < maxSteps; i++)
    {
        if (t >= tFar || transmittance < 0.001)
        {
            break;
        }

        float3 currentPos = rayOrigin + rayDir * t;
        float3 uvw = WorldToTexCoord(currentPos, fogVolume.aabbMin.xyz, fogVolume.aabbMax.xyz);

#if FOG_VOLUME_USE_SDF
        float sdf = SAMPLE_TEXTURE_3D(texture_sampler, DataMap, uvw).r;

        if (sdf < -0.01)
        {
            break;
        }
#endif

        float noise = GetFogDensity(uvw);
        float localDensity = noise * DensityScale;

        if (localDensity < 0.001)
        {
            t += stepSize;
            continue;
        }

        float3 stepLightEnergy = AmbientLight;

        for (uint lightIndex = 0u; lightIndex < fogVolume.numBoundLights; lightIndex++)
        {
            Light light = lights[lightIndex];
            ShadowMap shadowMap = shadowMaps[lightIndex];

            float3 lightDir;
            float phase;

            float shadow;
            float attenuation = 1.0;


            switch (light.type)
            {
                case HYP_LIGHT_TYPE_POINT:
                {
                    uint flags = light.flags;
                    float3 worldToLight = currentPos - light.position_intensity.xyz;

                    lightDir = normalize(-worldToLight);

                    float cosTheta = dot(lightDir, rayDir);
                    phase = HenyeyGreenstein(phaseG, cosTheta);

                    const float2 radiusFalloff = float2(f16tof32(light.radiusFalloffPacked), f16tof32(light.radiusFalloffPacked >> 16));
                    const float radius = radiusFalloff.x;
                    const float falloff = radiusFalloff.y;

                    shadow = GetPointShadowStandard(shadowMap, worldToLight, 0.0);

                    attenuation = GetSquareFalloffAttenuation(currentPos, light.position_intensity.xyz, radius);
                    break;
                }
                case HYP_LIGHT_TYPE_DIRECTIONAL:
                {
                    lightDir = normalize(-light.position_intensity.xyz);

                    float cosTheta = dot(lightDir, rayDir);
                    phase = HenyeyGreenstein(phaseG, cosTheta);

                    shadow = GetShadowStandard(shadowMap, currentPos, float2(0.0, 0.0), 1.0);

                    break;
                }
                default: break;
            }

            stepLightEnergy += light.color.rgb * light.position_intensity.w * attenuation * phase * shadow;
        }

        float stepExtinction = Extinction * localDensity * stepSize;
        float stepTransmittance = exp(-stepExtinction);

        float3 stepRadiance = stepLightEnergy * albedo * (1.0 - stepTransmittance);

        accumulatedColor += stepRadiance * transmittance;
        transmittance *= stepTransmittance;

        t += stepSize;
    }

    float alpha = 1.0 - transmittance;
    return float4(max(float3(0.0, 0.0, 0.0), accumulatedColor), alpha);
}

PSOutput PSMain(PSInput input)
{
    PSOutput output;

    float2 screenSpaceUV = (input.positionNdc.xy / input.positionNdc.w) * 0.5 + 0.5;
    screenSpaceUV.y = 1.0 - screenSpaceUV.y;

    float sceneDepth = SAMPLE_TEXTURE_2D_LOD(SamplerNearest, DepthPyramidTexture, screenSpaceUV, 0).r;
    float4 positionVS = ReconstructViewSpacePositionFromDepth(camera.invProjMat, screenSpaceUV, sceneDepth);
    float linearDepth = length(positionVS.xyz);

    float4 positionWS = mul(camera.invViewMat, positionVS);
    positionWS /= positionWS.w;

    float3 rayDir = normalize(positionWS.xyz - camera.position.xyz);

    float2 boxHits = RayBoxIntersect(camera.position.xyz, rayDir, fogVolume.aabbMin.xyz, fogVolume.aabbMax.xyz);
    float tNear = boxHits.x;
    float tFar = boxHits.y;

    if (tFar < 0.0 || tNear > tFar)
    {
        discard;
    }

    tFar = min(tFar, linearDepth);
    tNear = max(tNear, 0.0);

    if (tNear >= tFar)
    {
        discard;
    }

    float4 fogColor = RayMarch(camera.position.xyz, rayDir, tNear, tFar, 0.25);

    output.gbuffer_albedo = fogColor;
    output.gbuffer_normals = float4(0.0, 0.0, 0.0, 0.0);
    output.gbuffer_material = uint4(0, 0, 0, 0);
    output.gbuffer_velocity = float2(0.0, 0.0);

    return output;
}

#endif // PIXEL_SHADER
