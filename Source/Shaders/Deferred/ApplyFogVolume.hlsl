#include "../include/Defines.hlsli"

PERMUTE(CLUSTERED_LIGHTS)
PERMUTE(FOG_VOLUME_USE_SDF)

STATIC(MAX_CLUSTERED_SHADOW_MAPS, 16);
STATIC(MAX_FOG_LIGHTS, 4);
STATIC(TILE_Z_BINS, 16);
STATIC(TILE_SIZE, 32);

// Looks good, crushes performance with any number of lights > 3 or so
// #define POINT_LIGHT_FOG

// #define FOG_VOLUME_USE_SDF


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

DECLARE_SRV(FogVolume, DataMap) Texture3D<float4> DataMap;
DECLARE_SRV(FogVolume, NoiseMap) Texture3D<float> NoiseMap;

#include "../include/Clouds.hlsli"

DECLARE_SRV(FogVolume, CloudWeatherMapTexture) Texture2DArray CloudWeatherMapTexture;

// ambient in-scattering comes from Glimmer's irradiance where it reaches, so fog under a canopy dims with everything else
#include "../Glimmer/GlimmerApply.hlsli"
DECLARE_SRV(FogVolume, CloudShadowMapTexture) Texture2D CloudShadowMapTexture;

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

    CloudVolume cloudVolume;
    CloudWeatherMap cloudWeatherMap;
    CloudShadowMap cloudShadowMap;

    ShadowMap shadowMaps[MAX_CLUSTERED_SHADOW_MAPS];

    int2 screenDimensions;
    float minStepSize; // steps are the ray's length inside the volume over as many as fit, but no shorter than this
    uint maxSteps;
    uint frameCounter;
    uint3 _tailPad;

    GlimmerApply glimmer;
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

    CloudVolume cloudVolume;
    CloudWeatherMap cloudWeatherMap;
    CloudShadowMap cloudShadowMap;

    Light fogLights[MAX_FOG_LIGHTS];
    ShadowMap fogLightShadowMaps[MAX_FOG_LIGHTS];

    int2 screenDimensions;
    float minStepSize; // steps are the ray's length inside the volume over as many as fit, but no shorter than this
    uint maxSteps;
    uint frameCounter;
    uint3 _tailPad;

    GlimmerApply glimmer;
};

#endif // CLUSTERED_LIGHTS

#define GLIMMER_APPLY_WITH_SAMPLING
#include "../Glimmer/GlimmerApply.hlsli"

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

    return (1.0 / (4.0 * HYP_FMATH_PI)) * (num / max(denom, HYP_FMATH_EPSILON));
}

float GetDirectionalLightCSMShadow(float3 currentPos)
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

    int cascadeIndex = 3;
    cascadeIndex = select(insideMask.z > 0.5, 2, cascadeIndex);
    cascadeIndex = select(insideMask.y > 0.5, 1, cascadeIndex);
    cascadeIndex = select(insideMask.x > 0.5, 0, cascadeIndex);

    float4 shadowMapCoord;
    shadowMapCoord.x = uvX[cascadeIndex];
    shadowMapCoord.y = uvY[cascadeIndex];
    shadowMapCoord.z = uvZ[cascadeIndex];
    shadowMapCoord.w = (float)atlasSlice[cascadeIndex];

    float2 atlasUV = float2(atlasU[cascadeIndex], atlasV[cascadeIndex]);
    float2 atlasScale = float2(atlasScaleX[cascadeIndex], atlasScaleY[cascadeIndex]);

    return GetShadowCSM(shadowMapCoord, atlasUV, atlasScale);
}


float GetFogDensity(float3 uvw)
{
    return SAMPLE_TEXTURE_3D_LOD(texture_sampler, NoiseMap, uvw, 0).r;
}

// where there's no Glimmer to sample
#define FOG_FALLBACK_AMBIENT float3(0.12, 0.12, 0.12)

// ambient changes over metres, not steps
#define FOG_AMBIENT_INTERVAL 4

// Mean radiance arriving at P from all directions, which is what isotropic in-scattering sees. Glimmer's irradiance is L1, so
// averaging it over two opposite normals leaves just its constant band.
float3 GetFogAmbient(float3 P)
{
    const float4 up = EvaluateGlimmer(glimmer, P, float3(0.0, 1.0, 0.0));
    const float4 down = EvaluateGlimmer(glimmer, P, float3(0.0, -1.0, 0.0));

    const float weight = saturate(min(up.a, down.a));

    return lerp(FOG_FALLBACK_AMBIENT, 0.5 * (up.rgb + down.rgb), weight);
}

float4 RayMarch(float3 rayOrigin, float3 rayDir, float tNear, float tFar,
    float2 screenSpaceUV, float jitter
#ifdef CLUSTERED_LIGHTS
    , uint clusterIndexOffset, uint numClusteredLights
#endif
)
{
    const float density = fogVolume.medium.x;
    const float phaseForward = fogVolume.medium.y;
    const float phaseBackward = fogVolume.medium.z;
    const float phaseBlend = fogVolume.medium.w;
    const float3 albedo = fogVolume.lighting.rgb;
    const float ambientIntensity = fogVolume.lighting.a;
    const float sunIntensity = fogVolume.shape.x;
    const float edgeFade = fogVolume.shape.y;

    // the whole ray inside the volume in as many steps as fit, so big volumes aren't cut short and small ones don't oversample;
    // the jitter (and the temporal resolve after) turns the banding of long steps into noise that averages out
    const float rayLength = tFar - tNear;
    const uint numSteps = clamp(uint(ceil(rayLength / max(minStepSize, 1e-3))), 4u, max(maxSteps, 4u));
    const float stepSize = rayLength / float(numSteps);

    float transmittance = 1.0;
    float3 accumulatedColor = (float3)0.0;

    // the sun's direction, phase and cloud shadow barely change along one ray through a fog volume
    float3 sunRadiance = (float3)0.0;
    float3 lightDir = float3(0.0, 1.0, 0.0);

    const bool hasDirectionalLight = directionalLight.type == HYP_LIGHT_TYPE_DIRECTIONAL;
    const bool castsShadows = hasDirectionalLight && (directionalLight.flags & LF_SHADOW_CASTER) != 0;

    if (hasDirectionalLight)
    {
        lightDir = normalize(-directionalLight.position_intensity.xyz);

        const float cosTheta = dot(lightDir, rayDir);
        const float phase = lerp(HenyeyGreenstein(phaseForward, cosTheta), HenyeyGreenstein(phaseBackward, cosTheta), phaseBlend);

        const float3 midPoint = rayOrigin + rayDir * (tNear + 0.5 * rayLength);
        const float cloudShadow = GetCloudShadow(CloudWeatherMapTexture, CloudShadowMapTexture, SamplerLinear, cloudVolume, cloudWeatherMap, cloudShadowMap, midPoint, -lightDir);

        sunRadiance = directionalLight.color.rgb * directionalLight.atmosphere_tint.rgb * directionalLight.position_intensity.w * phase * cloudShadow * sunIntensity;
    }

    uint3 dataMapDimension;
    DataMap.GetDimensions(dataMapDimension.x, dataMapDimension.y, dataMapDimension.z);

    int2 pixelCoord = clamp((int2)(screenSpaceUV * (float2)screenDimensions), (int2)0, screenDimensions - 1);
    int temporalSampleIndex = int(frameCounter % 32u);

    static const float s_dataMapJitterScale = 0.35;

    float3 dataMapJitter = (float3(
        SampleBlueNoise(pixelCoord.x, pixelCoord.y, temporalSampleIndex, 33),
        SampleBlueNoise(pixelCoord.x, pixelCoord.y, temporalSampleIndex, 34),
        SampleBlueNoise(pixelCoord.x, pixelCoord.y, temporalSampleIndex, 35)) - 0.5)
        * (s_dataMapJitterScale / float3(dataMapDimension));

    const float cameraFadeDistance = max(0.5, length(fogVolume.aabbMax.xyz - fogVolume.aabbMin.xyz) * 0.15);

    float3 ambient = (float3)0.0;
    float t = tNear + jitter * stepSize;

    for (uint i = 0; i < numSteps; i++)
    {
        if (transmittance < 0.001)
        {
            break;
        }

        const float3 currentPos = rayOrigin + rayDir * t;
        const float3 uvw = WorldToTexCoord(currentPos, fogVolume.aabbMin.xyz, fogVolume.aabbMax.xyz);

        // soft walls rather than the box's hard faces
        const float3 distanceToFaces = min(currentPos - fogVolume.aabbMin.xyz, fogVolume.aabbMax.xyz - currentPos);
        const float wallFade = edgeFade > 0.0 ? smoothstep(0.0, edgeFade, min(distanceToFaces.x, min(distanceToFaces.y, distanceToFaces.z))) : 1.0;

        const float extinction = density * GetFogDensity(uvw) * wallFade;

        if (extinction < 1e-5)
        {
            t += stepSize;
            continue;
        }

        if (i % FOG_AMBIENT_INTERVAL == 0u || all(ambient == 0.0))
        {
            ambient = GetFogAmbient(currentPos) * ambientIntensity;
        }

        const float cameraFade = smoothstep(0.0, cameraFadeDistance, t);

        const float4 dataMapSample = SAMPLE_TEXTURE_3D_LOD(texture_sampler, DataMap, clamp(uvw + dataMapJitter, 0.0, 1.0), 0);
        const float3 bakedPointLight = dataMapSample.rgb * dataMapSample.a * cameraFade;

        float3 stepLightEnergy = ambient + bakedPointLight;

        if (hasDirectionalLight)
        {
            stepLightEnergy += sunRadiance * (castsShadows ? GetDirectionalLightCSMShadow(currentPos) : 1.0);
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
                    phase = HenyeyGreenstein(phaseForward, cosTheta);

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
                    phase = HenyeyGreenstein(phaseForward, cosTheta);

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
                    phase = HenyeyGreenstein(phaseForward, cosTheta);

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

        // lighting held constant over the step, integrated analytically so long steps keep their energy
        const float stepTransmittance = exp(-extinction * stepSize);

        accumulatedColor += stepLightEnergy * albedo * (1.0 - stepTransmittance) * transmittance;
        transmittance *= stepTransmittance;

        t += stepSize;
    }

    return float4(max((float3)0.0, accumulatedColor), 1.0 - transmittance);
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

    float temporalNoise = frac(noise + (frameCounter * s_goldenRatio));

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
        screenSpaceUV, temporalNoise, clusterIndexOffset, numClusteredLights);
#else
    float4 fogColor = RayMarch(camera.position.xyz, rayDir, tNear, tFar, screenSpaceUV, temporalNoise);
#endif

    return fogColor;
}

#endif // PIXEL_SHADER
