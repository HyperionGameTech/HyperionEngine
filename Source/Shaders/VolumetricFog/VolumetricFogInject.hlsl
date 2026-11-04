#include "../Include/Defines.hlsli"

STATIC(MAX_CLUSTERED_SHADOW_MAPS, 16);
STATIC(TILE_Z_BINS, 16);
STATIC(TILE_SIZE, 32);
STATIC(MAX_VOLUMETRIC_FOG_VOLUMES, 8);

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../Include/Scene.hlsli"
#include "../Include/Shared.hlsli"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "../Include/Clouds.hlsli"
#include "../Include/Noise.hlsli"
#include "VolumetricFogCommon.hlsli"

DECLARE_SAMPLER(VolumetricFog, SamplerLinear) SamplerState SamplerLinear;
DECLARE_SAMPLER(VolumetricFog, SamplerNearest) SamplerState SamplerNearest;
DECLARE_SAMPLER(VolumetricFog, SamplerRepeat) SamplerState SamplerRepeat;

#define HYP_SAMPLER_LINEAR SamplerLinear
#define HYP_SAMPLER_NEAREST SamplerNearest

DECLARE_UAV(VolumetricFog, OutLitTexture) RWTexture3D<float4> OutLitTexture;

DECLARE_SRV(VolumetricFog, NoiseTexture) Texture3D<float4> NoiseTexture;
DECLARE_SRV(VolumetricFog, AmbientTexture) Texture3D<float4> AmbientTexture;

DECLARE_SRV(VolumetricFog, WorldsBuffer) StructuredBuffer<WorldShaderData> _worlds_buffer;
#define world_shader_data _worlds_buffer[0]

DECLARE_SRV(VolumetricFog, ShadowMapsTextureArray) Texture2DArray<float> shadow_maps;
DECLARE_SRV(VolumetricFog, PointLightShadowMapsTextureArray) TextureCubeArray point_shadow_maps;

DECLARE_SRV(VolumetricFog, LightsBuffer) StructuredBuffer<Light> LightsBuffer;
DECLARE_SRV(VolumetricFog, ClusterGridBuffer) ByteAddressBuffer ClusterGridBuffer;
DECLARE_SRV(VolumetricFog, ClusterIndexBuffer) ByteAddressBuffer ClusterIndexBuffer;
DECLARE_SRV(VolumetricFog, ShadowMapIndexBuffer) ByteAddressBuffer LightToShadowMapIndex;

DECLARE_SRV(VolumetricFog, CloudWeatherMapTexture) Texture2DArray CloudWeatherMapTexture;
DECLARE_SRV(VolumetricFog, CloudShadowMapTexture) Texture2D CloudShadowMapTexture;

#include "../Deferred/ClusteredShading.hlsli"
#include "../Include/Shadows.hlsli"

static const float HeightWindBoost = 1.0;
static const float HeightWindRange = 60.0;

DECLARE_BUFFER_DYNAMIC(VolumetricFog, VolumetricFogConstants) cbuffer VolumetricFogConstants
{
    Camera camera;
    VolumetricFogGrid grid;

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

    VolumetricFogVolume fogVolumes[MAX_VOLUMETRIC_FOG_VOLUMES];
};

// sunlight crossing the height fog toward the cell is integrated over at most this far
static const float SunOpticalDepthDistance = 200.0;

// metres per second at a wind strength of 1
static const float FogWindSpeed = 12.0;

static const float LightMinDistance = 0.5;

struct ParticipatingMedium
{
    float extinction;
    float3 scattering;
    float3 emission;

    // weighted by scattering, so mixed media blend their phase and lighting scales
    float phaseForward;
    float phaseBackward;
    float phaseBlend;
    float sunScale;
    float ambientScale;
};

struct FogNoise
{
    float density;  // averages 1
    float billow;   // 0 to 1, large and slow
};

uint GetShadowMapIndexForLight(uint lightIndex)
{
    return LightToShadowMapIndex.Load(lightIndex * sizeof(uint));
}

float GetSunShadow(float3 positionWS)
{
    float4 positionLS = mul(shadowViewMat, float4(positionWS, 1.0));
    positionLS /= positionLS.w;

    const float4 uvX = positionLS.x * cascadeScaleX + cascadeOffsetX;
    const float4 uvY = positionLS.y * cascadeScaleY + cascadeOffsetY;
    const float4 uvZ = positionLS.z * cascadeScaleZ + cascadeOffsetZ;

    const float4 maxDist = max(abs(uvX - 0.5), max(abs(uvY - 0.5), abs(uvZ - 0.5)));
    const float4 insideMask = step(maxDist, (float4)0.5);

    if (dot(insideMask, (float4)1.0) < 0.5)
    {
        return 1.0;
    }

    int cascadeIndex = 3;
    cascadeIndex = select(insideMask.z > 0.5, 2, cascadeIndex);
    cascadeIndex = select(insideMask.y > 0.5, 1, cascadeIndex);
    cascadeIndex = select(insideMask.x > 0.5, 0, cascadeIndex);

    const float4 shadowMapCoord = float4(uvX[cascadeIndex], uvY[cascadeIndex], uvZ[cascadeIndex], (float)atlasSlice[cascadeIndex]);

    return GetShadowCSM(shadowMapCoord,
        float2(atlasU[cascadeIndex], atlasV[cascadeIndex]),
        float2(atlasScaleX[cascadeIndex], atlasScaleY[cascadeIndex]));
}

float GetHeightFogDensity(float height)
{
    const float density = world_shader_data.height_fog_params.x;
    const float heightFalloff = world_shader_data.height_fog_params.y;
    const float baseHeight = world_shader_data.height_fog_params.z;

    return density * exp(clamp(-heightFalloff * (height - baseHeight), -60.0, 60.0));
}

float GetHeightFogOpticalDepthToSun(float height, float densityAtHeight, float directionToSunY)
{
    const float heightFalloff = world_shader_data.height_fog_params.y;
    const float heightChange = max(heightFalloff * directionToSunY * SunOpticalDepthDistance, -60.0);
    const float integralFactor = abs(heightChange) > 1e-4 ? (1.0 - exp(-heightChange)) / heightChange : 1.0;

    return densityAtHeight * SunOpticalDepthDistance * integralFactor;
}

FogNoise GetFogNoise(float3 positionWS, float fogHeight)
{
    const float noiseStrength = world_shader_data.volumetric_fog_params.y;
    const float inverseScale = world_shader_data.volumetric_fog_params.z;
    const float time = world_shader_data.game_time;

    const float3 windDirection = float3(world_shader_data.wind_params.x, 0.0, world_shader_data.wind_params.y);
    const float windSpeed = world_shader_data.wind_params.z * FogWindSpeed;

    const float heightAboveBase = fogHeight - world_shader_data.height_fog_params.z;
    const float windScale = 1.0 + HeightWindBoost * saturate(heightAboveBase / HeightWindRange);

    const float3 billowUVW = (positionWS - windDirection * (windSpeed * 0.4 * time)) * (inverseScale * 0.18) + float3(0.0, time * 0.004, 0.0);

    FogNoise result;
    result.billow = NoiseTexture.SampleLevel(SamplerRepeat, billowUVW, 0).r;
    result.density = 1.0;

    [branch]
    if (noiseStrength <= 0.0)
    {
        return result;
    }

    // a slower field pushes the wisps around, so they curl and stretch instead of sliding past in one piece
    const float3 warpUVW = (positionWS - windDirection * (windSpeed * 0.7 * time)) * (inverseScale * 0.5) + 0.17;
    const float3 warp = NoiseTexture.SampleLevel(SamplerRepeat, warpUVW, 0).rgb * 2.0 - 1.0;

    const float3 wispsUVW = (positionWS - windDirection * (windSpeed * windScale * time) - float3(0.0, time * 0.15, 0.0)) * inverseScale + warp * 0.35;
    const float3 detailUVW = (positionWS - windDirection * (windSpeed * windScale * 1.6 * time) + float3(0.0, time * 0.25, 0.0)) * (inverseScale * 2.7) + warp * 0.6 + 0.31;

    const float wisps = NoiseTexture.SampleLevel(SamplerRepeat, wispsUVW, 0).r;
    const float detail = NoiseTexture.SampleLevel(SamplerRepeat, detailUVW, 0).g;

    const float noise = (wisps * 0.7 + detail * 0.3) * lerp(0.5, 1.5, result.billow);

    result.density = max(lerp(1.0, noise * 2.0, noiseStrength), 0.0);

    return result;
}

float GetLayerTopFade(float fogHeight, float billow)
{
    const float layerTop = world_shader_data.volumetric_fog_layer.y;

    if (layerTop <= 0.0)
    {
        return 1.0;
    }

    const float softness = world_shader_data.volumetric_fog_layer.z;

    // billows push the top of the bank up and down, so its edge rolls instead of being a flat ceiling
    const float topHeight = world_shader_data.height_fog_params.z + layerTop + (billow - 0.5) * softness * 3.0;

    return saturate((topHeight - fogHeight) / softness);
}

float GetFogCoverage(float3 positionWS)
{
    const float coverage = world_shader_data.volumetric_fog_params.w;

    if (coverage >= 0.999)
    {
        return 1.0;
    }

    const float2 coverageUV = positionWS.xz * world_shader_data.volumetric_fog_emissive.w;
    const float coverageNoise = NoiseTexture.SampleLevel(SamplerRepeat, float3(coverageUV, 0.5), 0).b;

    const float edge = lerp(-0.2, 1.0, 1.0 - coverage);

    return saturate((coverageNoise - edge) / 0.2);
}

void AddParticipatingMedium(
    inout ParticipatingMedium pm,
    float extinction,
    float3 albedo,
    float phaseForward,
    float phaseBackward,
    float phaseBlend,
    float sunScale,
    float ambientScale)
{
    if (extinction <= 0.0)
    {
        return;
    }

    const float3 scattering = extinction * albedo;
    const float weight = max(dot(scattering, (float3)(1.0 / 3.0)), 1e-6);
    const float totalWeight = max(dot(pm.scattering, (float3)(1.0 / 3.0)), 0.0) + weight;
    const float blend = weight / totalWeight;

    pm.phaseForward = lerp(pm.phaseForward, phaseForward, blend);
    pm.phaseBackward = lerp(pm.phaseBackward, phaseBackward, blend);
    pm.phaseBlend = lerp(pm.phaseBlend, phaseBlend, blend);
    pm.sunScale = lerp(pm.sunScale, sunScale, blend);
    pm.ambientScale = lerp(pm.ambientScale, ambientScale, blend);

    pm.extinction += extinction;
    pm.scattering += scattering;
}

ParticipatingMedium GetFogMedium(
    float3 positionWS,
    float depth,
    float fogHeight)
{
    ParticipatingMedium pm = (ParticipatingMedium)0;
    pm.sunScale = 1.0;
    pm.ambientScale = 1.0;

    const float3 windDirection = float3(world_shader_data.wind_params.x, 0.0, world_shader_data.wind_params.y);
    const float3 windOffset = windDirection * (world_shader_data.wind_params.z * FogWindSpeed * world_shader_data.game_time);

    const FogNoise noise = GetFogNoise(positionWS, fogHeight);

    const float startDistance = world_shader_data.height_fog_params.w;

    if ((world_shader_data.environment_flags & WORLD_ENVIRONMENT_FLAG_VOLUMETRIC_FOG) != 0 && depth >= startDistance)
    {
        const float coverage = GetFogCoverage(positionWS - windOffset * 0.5);

        const float sunAnisotropy = world_shader_data.fog_phase_params.x;
        const float3 albedo = world_shader_data.volumetric_fog_params.x * world_shader_data.volumetric_fog_tint.rgb;

        const float heightFogExtinction = GetHeightFogDensity(fogHeight) * noise.density * coverage * GetLayerTopFade(fogHeight, noise.billow);

        AddParticipatingMedium(pm, heightFogExtinction, albedo, sunAnisotropy, 0.0, 0.0, 1.0, 1.0);

        pm.emission = world_shader_data.volumetric_fog_emissive.rgb * heightFogExtinction;

        const float aerialPerspectiveDistance = world_shader_data.atmosphere_fog_params.x;

        if (aerialPerspectiveDistance > 0.0)
        {
            AddParticipatingMedium(pm, 1.0 / aerialPerspectiveDistance, (float3)1.0, sunAnisotropy, 0.0, 0.0, 1.0, 1.0);
        }
    }

    const uint numFogVolumes = min(uint(grid.params.y), uint(MAX_VOLUMETRIC_FOG_VOLUMES));

    for (uint volumeIndex = 0; volumeIndex < numFogVolumes; volumeIndex++)
    {
        const VolumetricFogVolume volume = fogVolumes[volumeIndex];

        if (any(positionWS < volume.aabbMin.xyz) || any(positionWS > volume.aabbMax.xyz))
        {
            continue;
        }

        const float3 distanceToFaces = min(positionWS - volume.aabbMin.xyz, volume.aabbMax.xyz - positionWS);
        const float edgeFade = volume.aabbMin.w > 0.0
            ? smoothstep(0.0, volume.aabbMin.w, min(distanceToFaces.x, min(distanceToFaces.y, distanceToFaces.z)))
            : 1.0;

        AddParticipatingMedium(pm, volume.aabbMax.w * noise.density * edgeFade, volume.lighting.rgb,
            volume.phase.x, volume.phase.y, volume.phase.z,
            volume.phase.w, volume.lighting.a);
    }

    return pm;
}

// octave i scatters a^i as much, sees b^i of the extinction toward the light and has c^i of the anisotropy (a = b = c = 0.5)
float GetMultipleScattering(float g, float cosTheta, float opticalDepthToLight, uint octaves)
{
    float result = 0.0;

    float scatterScale = 1.0;
    float extinctionScale = 1.0;
    float anisotropy = g;

    for (uint octave = 0; octave < octaves; octave++)
    {
        result += scatterScale * VolumetricFogPhase(anisotropy, cosTheta) * exp(-extinctionScale * opticalDepthToLight);

        scatterScale *= 0.5;
        extinctionScale *= 0.5;
        anisotropy *= 0.5;
    }

    return result;
}

float GetMediumPhase(ParticipatingMedium pm, float cosTheta, float opticalDepthToLight, uint octaves)
{
    const float forward = GetMultipleScattering(pm.phaseForward, cosTheta, opticalDepthToLight, octaves);

    if (pm.phaseBlend <= 0.0)
    {
        return forward;
    }

    return lerp(forward, GetMultipleScattering(pm.phaseBackward, cosTheta, opticalDepthToLight, octaves), pm.phaseBlend);
}

float GetLocalLightAttenuation(float3 positionWS, float3 lightPosition, float radius)
{
    const float3 toLight = lightPosition - positionWS;
    const float distanceSq = max(dot(toLight, toLight), LightMinDistance * LightMinDistance);

    const float distanceOverRadiusSq = distanceSq / max(radius * radius, 1e-4);

    float window = saturate(1.0 - distanceOverRadiusSq * distanceOverRadiusSq);
    window *= window;

    return window / distanceSq;
}

float3 GetLocalLighting(
    float3 positionWS,
    float3 rayDirection,
    float viewSpaceZ,
    float2 uv,
    ParticipatingMedium pm,
    uint octaves)
{
    const uint2 viewportExtent = camera.dimensions.xy;
    const uint2 viewportPixelCoord = min(uint2(uv * float2(viewportExtent)), viewportExtent - 1u);

    const uint gridIndex = Cluster_GetGridIndex(viewportExtent, viewportPixelCoord, viewSpaceZ, camera.near, camera.far);
    const uint2 clusterData = ClusterGridBuffer.Load2(gridIndex * sizeof(uint2));

    const uint clusterIndexOffset = clusterData.x;
    const uint numLights = clusterData.y & 0xFFFFu;

    float3 lighting = (float3)0.0;

    for (uint clusterLightIndex = 0; clusterLightIndex < numLights; clusterLightIndex++)
    {
        const uint lightIndex = Cluster_LoadLightIndex(clusterIndexOffset, clusterLightIndex);
        const Light light = LightsBuffer[lightIndex];

        if (light.type != HYP_LIGHT_TYPE_POINT && light.type != HYP_LIGHT_TYPE_SPOT)
        {
            continue;
        }

        const float volumetricIntensity = light.volumetric_params.x;

        if (volumetricIntensity <= 0.0)
        {
            continue;
        }

        const float3 lightPosition = light.position_intensity.xyz;
        const float3 worldToLight = positionWS - lightPosition;
        const float distanceToLight = length(worldToLight);
        const float3 directionToLight = -worldToLight / max(distanceToLight, 1e-4);

        const float radius = f16tof32(light.radiusFalloffPacked);

        float attenuation = GetLocalLightAttenuation(positionWS, lightPosition, radius);

        if (light.type == HYP_LIGHT_TYPE_SPOT)
        {
            const float theta = max(dot(-directionToLight, normalize(light.normal.xyz)), 0.0);
            const float2 spotAngles = light.area_size.xy;

            attenuation *= saturate((theta - spotAngles.x) / max(spotAngles.y - spotAngles.x, 1e-4)) * step(spotAngles.x, theta);
        }

        if (attenuation <= 0.0)
        {
            continue;
        }

        float shadow = 1.0;

        if ((light.flags & LF_SHADOW_CASTER) != 0)
        {
            const uint shadowMapIndex = GetShadowMapIndexForLight(lightIndex);

            if (shadowMapIndex < MAX_CLUSTERED_SHADOW_MAPS)
            {
                if (light.type == HYP_LIGHT_TYPE_SPOT)
                {
                    shadow = GetSpotShadowStandard(shadowMaps[shadowMapIndex], positionWS, directionToLight);
                }
                else
                {
                    shadow = GetPointShadowStandard(shadowMaps[shadowMapIndex], worldToLight, 1.0);
                }
            }
        }

        if (shadow <= 0.0)
        {
            continue;
        }

        const float opticalDepthToLight = pm.extinction * distanceToLight;
        const float phase = GetMediumPhase(pm, dot(rayDirection, directionToLight), opticalDepthToLight, octaves);

        lighting += light.color.rgb * light.position_intensity.w * volumetricIntensity * attenuation * shadow * phase;
    }

    return lighting;
}

float3 GetSunLighting(
    float3 positionWS,
    float fogHeight,
    float3 rayDirection,
    ParticipatingMedium pm,
    uint octaves)
{
    const float sunIntensity = directionalLight.position_intensity.w * directionalLight.volumetric_params.x;

    if (directionalLight.type != HYP_LIGHT_TYPE_DIRECTIONAL || sunIntensity <= 0.0)
    {
        return (float3)0.0;
    }

    const float3 directionToSun = normalize(directionalLight.position_intensity.xyz);

    const float horizonFade = saturate(directionToSun.y * 10.0 + 1.0);

    // no direct sun through an overcast sky
    const float overcastWeight = smoothstep(0.5, 1.0, world_shader_data.sky_light_params.w);

    const float visibility = horizonFade * (1.0 - overcastWeight);

    if (visibility <= 0.0)
    {
        return (float3)0.0;
    }

    float shadow = (directionalLight.flags & LF_SHADOW_CASTER) != 0 ? GetSunShadow(positionWS) : 1.0;

    if (shadow <= 0.0)
    {
        return (float3)0.0;
    }

    shadow *= GetCloudShadow(CloudWeatherMapTexture, CloudShadowMapTexture, SamplerLinear, cloudVolume, cloudWeatherMap, cloudShadowMap, positionWS, directionToSun);

    const float opticalDepthToSun = GetHeightFogOpticalDepthToSun(fogHeight, GetHeightFogDensity(fogHeight), directionToSun.y);
    const float phase = GetMediumPhase(pm, dot(rayDirection, directionToSun), opticalDepthToSun, octaves);

    return directionalLight.color.rgb * directionalLight.atmosphere_tint.rgb * sunIntensity
        * world_shader_data.atmosphere_fog_params.z
        * pm.sunScale * visibility * shadow * phase;
}

[numthreads(8, 8, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint3 cell = dispatchThreadId;

    if (any(cell >= grid.dimensions.xyz))
    {
        return;
    }

    const float3 cellNoise = float3(
        InterleavedGradientNoise(float2(cell.xy) + float(cell.z) * float2(5.3, 7.1)),
        InterleavedGradientNoise(float2(cell.yx) + float(cell.z) * float2(3.7, 1.9) + 17.0),
        InterleavedGradientNoise(float2(cell.xy) + float(cell.z) * float2(2.3, 9.7) + 41.0));

    const float3 jitter = frac(grid.jitter.xyz + 0.5 + cellNoise) - 0.5;

    const float2 uv = (float2(cell.xy) + 0.5 + jitter.xy) / float2(grid.dimensions.xy);
    const float slice = float(cell.z) + 0.5 + jitter.z;
    const float depth = VolumetricFogSliceToDepth(grid, slice);

    const float3 rayDirection = VolumetricFogRayDirection(camera.invProjMat, camera.invViewMat, camera.position.xyz, uv);
    const float3 positionWS = VolumetricFogCellPosition(camera.viewProjMat, camera.position.xyz, rayDirection, depth);

    // rgb = ambient light, a = terrain height below the cell
    const float4 ambientSample = AmbientTexture.SampleLevel(SamplerLinear, saturate(float3(uv, slice * grid.depthParams.w)), 0);

    const float fogHeight = positionWS.y - world_shader_data.volumetric_fog_layer.x * ambientSample.a;

    const ParticipatingMedium pm = GetFogMedium(positionWS, depth, fogHeight);

    if (pm.extinction <= 1e-6)
    {
        OutLitTexture[cell] = (float4)0.0;

        return;
    }

    const uint octaves = clamp(uint(grid.params.x), 1u, 4u);

    const float viewSpaceZ = mul(camera.view, float4(positionWS, 1.0)).z;

    float3 radiance = GetSunLighting(positionWS, fogHeight, rayDirection, pm, octaves);
    radiance += GetLocalLighting(positionWS, rayDirection, viewSpaceZ, uv, pm, octaves);
    radiance += ambientSample.rgb * pm.ambientScale;

    const float3 inScatter = radiance * pm.scattering + pm.emission;

    OutLitTexture[cell] = float4(max(inScatter, (float3)0.0), pm.extinction);
}
