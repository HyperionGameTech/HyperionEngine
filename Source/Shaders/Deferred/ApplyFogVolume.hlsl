#include "../include/Defines.hlsli"

PERMUTE(CLUSTERED_LIGHTS)

STATIC(MAX_CLUSTERED_SHADOW_MAPS, 16);
STATIC(MAX_FOG_LIGHTS, 4);
STATIC(TILE_Z_BINS, 16);
STATIC(TILE_SIZE, 32);

// Looks good, crushes performance with any number of lights > 3 or so
// #define POINT_LIGHT_FOG

#define FOG_VOLUME_USE_SDF


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

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

DECLARE_SAMPLER(FogVolume, SamplerLinear) SamplerState SamplerLinear;
DECLARE_SAMPLER(FogVolume, SamplerNearest) SamplerState SamplerNearest;

#define texture_sampler SamplerLinear
#define sampler_linear SamplerLinear
#define sampler_nearest SamplerNearest
#define HYP_SAMPLER_NEAREST SamplerNearest
#define HYP_SAMPLER_LINEAR SamplerLinear

DECLARE_SRV(FogVolume, DepthTexture) Texture2D DepthTexture;

#include "../include/Scene.hlsli"
#include "../include/Material.hlsli"
#include "../include/Entity.hlsli"
#include "../include/Packing.hlsli"
#include "../include/Shared.hlsli"
#include "../include/Gbuffer.hlsli"
#include "../include/EnvProbes.hlsli"

DECLARE_SRV_DYNAMIC(FogVolume, CamerasBuffer) StructuredBuffer<Camera> _cameras_buffer;
#define camera _cameras_buffer[0]

DECLARE_SRV(FogVolume, ShadowMapsTextureArray) Texture2DArray<float> shadow_maps;
DECLARE_SRV(FogVolume, PointLightShadowMapsTextureArray) TextureCubeArray point_shadow_maps;

DECLARE_SRV(FogVolume, EnvProbesBuffer) StructuredBuffer<EnvProbe> EnvProbesBuffer;
DECLARE_SRV(FogVolume, LightsBuffer) StructuredBuffer<Light> LightsBuffer;
DECLARE_SRV(FogVolume, ClusterGridBuffer) ByteAddressBuffer ClusterGridBuffer;
DECLARE_SRV(FogVolume, ClusterIndexBuffer) ByteAddressBuffer ClusterIndexBuffer;

DECLARE_SRV(FogVolume, ShadowMapIndexBuffer) ByteAddressBuffer LightToShadowMapIndex;

#include "./ClusteredShading.hlsli"

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

/// Blue noise
DECLARE_SRV(FogVolume, BlueNoiseBuffer) StructuredBuffer<int4> BlueNoiseBuffer;

#include "../include/BlueNoise.hlsli"

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

DECLARE_SRV(FogVolume, DataMap) Texture3D<float> DataMap;
DECLARE_SRV(FogVolume, NoiseMap) Texture3D<float> NoiseMap;

#ifdef CLUSTERED_LIGHTS

DECLARE_BUFFER_DYNAMIC(FogVolume, FogVolumeConstants) cbuffer FogVolumeConstants
{
    FogVolume fogVolume;
    
    Light directionalLight;

    float4x4 shadowViewMat;

    float4 atlasU;
    float4 atlasV;
    
    float4 atlasScaleX;
    float4 atlasScaleY;
    
    uint4 atlasSlice;
    
    float4 cascadeScaleX;
    float4 cascadeScaleY;
    float4 cascadeScaleZ;
    
    float4 cascadeOffsetX;
    float4 cascadeOffsetY;
    float4 cascadeOffsetZ;

    ShadowMap shadowMaps[MAX_CLUSTERED_SHADOW_MAPS];

    int2 screenDimensions;
    float stepSize;
    uint maxSteps;
    uint frameCounter;
};

uint GetShadowMapIndexForLight(uint lightIndex)
{
    return LightToShadowMapIndex.Load(lightIndex * sizeof(uint));
}

#else // !CLUSTERED_LIGHTS

DECLARE_BUFFER_DYNAMIC(FogVolume, FogVolumeConstants) cbuffer FogVolumeConstants
{
    FogVolume fogVolume;

    Light directionalLight;

    float4x4 shadowViewMat;

    float4 atlasU;
    float4 atlasV;
    
    float4 atlasScaleX;
    float4 atlasScaleY;
    
    uint4 atlasSlice;
    
    float4 cascadeScaleX;
    float4 cascadeScaleY;
    float4 cascadeScaleZ;
    
    float4 cascadeOffsetX;
    float4 cascadeOffsetY;
    float4 cascadeOffsetZ;

    Light fogLights[MAX_FOG_LIGHTS];
    ShadowMap fogLightShadowMaps[MAX_FOG_LIGHTS];

    int2 screenDimensions;
    float stepSize;
    uint maxSteps;
    uint frameCounter;
};

#endif // CLUSTERED_LIGHTS

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
    return SAMPLE_TEXTURE_3D_LOD(texture_sampler, NoiseMap, uvw, 0).r;
}

float GetDirectionalLightCSMShadow(float3 currentPos, float2 screenSpaceUV)
{
    float4 positionLS = mul(shadowViewMat, float4(currentPos, 1.0));
    positionLS /= positionLS.w;

    float4 uvX = positionLS.x * cascadeScaleX + cascadeOffsetX;
    float4 uvY = positionLS.y * cascadeScaleY + cascadeOffsetY;
    float4 uvZ = positionLS.z * cascadeScaleZ + cascadeOffsetZ;

    float4 distX = abs(uvX - 0.5);
    float4 distY = abs(uvY - 0.5);
    float4 distZ = abs(uvZ - 0.5);

    float4 maxDist = max(distX, max(distY, distZ));
    float4 insideMask = step(maxDist, (float4)0.5);

    int cascadeIndex = 4 - (int)dot(insideMask, (float4)1.0);
    cascadeIndex = min(cascadeIndex, 3);

    float4 shadowMapCoord;
    shadowMapCoord.x = uvX[cascadeIndex];
    shadowMapCoord.y = uvY[cascadeIndex];
    shadowMapCoord.z = uvZ[cascadeIndex];
    shadowMapCoord.w = (float)atlasSlice[cascadeIndex];

    float2 atlasUV = float2(atlasU[cascadeIndex], atlasV[cascadeIndex]);
    float2 atlasScale = float2(atlasScaleX[cascadeIndex], atlasScaleY[cascadeIndex]);

    return GetShadowCSM(shadowMapCoord, atlasUV, atlasScale, currentPos, screenSpaceUV, camera.dimensions.xy, 1.0);
}

float4 RayMarch(float3 rayOrigin, float3 rayDir, float tNear, float tFar,
    float2 screenSpaceUV
#ifdef CLUSTERED_LIGHTS
    , uint clusterIndexOffset, uint numClusteredLights
#endif
)
{
    float t = tNear;

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

    bool hasDirectionalLight = (directionalLight.type == HYP_LIGHT_TYPE_DIRECTIONAL);

    for (int i = 0; i < maxSteps; i++)
    {
        if (t >= tFar || transmittance < 0.001)
        {
            break;
        }

        float3 currentPos = rayOrigin + rayDir * t;
        float3 uvw = WorldToTexCoord(currentPos, fogVolume.aabbMin.xyz, fogVolume.aabbMax.xyz);

#ifdef FOG_VOLUME_USE_SDF
        float sdf = SAMPLE_TEXTURE_3D_LOD(texture_sampler, DataMap, uvw, 0).r;

        if (sdf < -0.001)
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

        if (hasDirectionalLight)
        {
            float3 lightDir = normalize(-directionalLight.position_intensity.xyz);

            float cosTheta = dot(lightDir, rayDir);
            float phase = HenyeyGreenstein(phaseG, cosTheta);

            float shadow = 1.0;

            if ((directionalLight.flags & LF_SHADOW_CASTER) != 0)
            {
                shadow = GetDirectionalLightCSMShadow(currentPos, screenSpaceUV);
            }

            stepLightEnergy += directionalLight.color.rgb * directionalLight.position_intensity.w * phase * shadow;
        }

#ifdef POINT_LIGHT_FOG

#ifdef CLUSTERED_LIGHTS
        for (uint ci = 0; ci < numClusteredLights; ++ci)
        {
            uint lightIndex = Cluster_LoadLightIndex(clusterIndexOffset, ci);
            Light light = LightsBuffer[lightIndex];

            float3 lightDir;
            float phase;
            float shadow = 1.0;
            float attenuation = 1.0;

            switch (light.type)
            {
                case HYP_LIGHT_TYPE_POINT:
                {
                    float3 worldToLight = currentPos - light.position_intensity.xyz;

                    lightDir = normalize(-worldToLight);

                    float cosTheta = dot(lightDir, rayDir);
                    phase = HenyeyGreenstein(phaseG, cosTheta);

                    const float2 radiusFalloff = float2(f16tof32(light.radiusFalloffPacked), f16tof32(light.radiusFalloffPacked >> 16));
                    const float radius = radiusFalloff.x;

                    if ((light.flags & LF_SHADOW_CASTER) != 0)
                    {
                        uint shadowMapIndex = GetShadowMapIndexForLight(lightIndex);

                        if (shadowMapIndex < MAX_CLUSTERED_SHADOW_MAPS)
                        {
                            ShadowMap shadowMapData = shadowMaps[shadowMapIndex];
                            shadow = GetPointShadow(shadowMapData, light.flags, worldToLight, 0.0);
                        }
                    }

                    attenuation = GetSquareFalloffAttenuation(currentPos, light.position_intensity.xyz, radius);
                    break;
                }
                case HYP_LIGHT_TYPE_SPOT:
                {
                    float3 worldToLight = currentPos - light.position_intensity.xyz;

                    lightDir = normalize(-worldToLight);

                    float cosTheta = dot(lightDir, rayDir);
                    phase = HenyeyGreenstein(phaseG, cosTheta);

                    const float2 radiusFalloff = float2(f16tof32(light.radiusFalloffPacked), f16tof32(light.radiusFalloffPacked >> 16));
                    const float radius = radiusFalloff.x;

                    attenuation = GetSquareFalloffAttenuation(currentPos, light.position_intensity.xyz, radius);

                    float theta = max(dot(-lightDir, normalize(light.normal.xyz)), 0.0);
                    float2 spot_angles = light.area_size.xy;

                    attenuation *= saturate((theta - spot_angles[0]) / (spot_angles[1] - spot_angles[0])) * step(spot_angles[0], theta);
                    break;
                }
                default: continue;
            }

            stepLightEnergy += light.color.rgb * light.position_intensity.w * attenuation * phase * shadow;
        }
#else // !CLUSTERED_LIGHTS
        for (uint lightIndex = 0u; lightIndex < fogVolume.numBoundLights; lightIndex++)
        {
            Light light = fogLights[lightIndex];
            ShadowMap shadowMap = fogLightShadowMaps[lightIndex];

            float3 lightDir;
            float phase;
            float shadow = 1.0;
            float attenuation = 1.0;

            switch (light.type)
            {
                case HYP_LIGHT_TYPE_POINT:
                {
                    float3 worldToLight = currentPos - light.position_intensity.xyz;

                    lightDir = normalize(-worldToLight);

                    float cosTheta = dot(lightDir, rayDir);
                    phase = HenyeyGreenstein(phaseG, cosTheta);

                    const float2 radiusFalloff = float2(f16tof32(light.radiusFalloffPacked), f16tof32(light.radiusFalloffPacked >> 16));
                    const float radius = radiusFalloff.x;

                    shadow = GetPointShadowStandard(shadowMap, worldToLight, 0.0);

                    attenuation = GetSquareFalloffAttenuation(currentPos, light.position_intensity.xyz, radius);
                    break;
                }
                default: continue;
            }

            stepLightEnergy += light.color.rgb * light.position_intensity.w * attenuation * phase * shadow;
        }
#endif // CLUSTERED_LIGHTS

#endif // POINT_LIGHT_FOG

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

float4 PSMain(PSInput input) : SV_TARGET
{
    float2 screenSpaceUV = (input.positionNdc.xy / input.positionNdc.w) * 0.5 + 0.5;
    screenSpaceUV.y = 1.0 - screenSpaceUV.y;

    float sceneDepth = SAMPLE_TEXTURE_2D_LOD(SamplerNearest, DepthTexture, screenSpaceUV, 0).r;
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

#define NUM_TEMPORAL_SAMPLES 32

    int2 coord = int2(screenSpaceUV * screenDimensions);

    const float noise = SampleBlueNoise(coord.x, coord.y, int(frameCounter % NUM_TEMPORAL_SAMPLES), NUM_TEMPORAL_SAMPLES);
    static const float s_goldenRatio = 0.61803398875;

    float temporalNoise = fract(noise + (frameCounter * s_goldenRatio));

    tNear += temporalNoise * stepSize;

#ifdef CLUSTERED_LIGHTS
    float viewSpaceZ = positionVS.z;

    uint2 viewportExtent = camera.dimensions.xy;
    uint2 viewportPixelCoord = uint2(screenSpaceUV * (float2)viewportExtent);

    uint gridIndex = Cluster_GetGridIndex(
        viewportExtent, viewportPixelCoord,
        viewSpaceZ,
        camera.near, camera.far);

    uint2 clusterData = ClusterGridBuffer.Load2(gridIndex * sizeof(uint2));
    uint clusterIndexOffset = clusterData.x;
    uint numClusteredLights = (clusterData.y & 0xFFFFu);

    float4 fogColor = RayMarch(camera.position.xyz, rayDir, tNear, tFar,
        screenSpaceUV, clusterIndexOffset, numClusteredLights);
#else
    float4 fogColor = RayMarch(camera.position.xyz, rayDir, tNear, tFar, screenSpaceUV);
#endif

    return fogColor;
}

#endif // PIXEL_SHADER
