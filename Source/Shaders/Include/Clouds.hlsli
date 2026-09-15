#ifndef HYP_CLOUDS
#define HYP_CLOUDS

// matches CloudVolumeShaderData in Rendering/RenderProxy.hpp
struct CloudVolumeParams
{
    float coverage;
    float cloudTypeBias;
    float densityMultiplier;
    float detailErosion;

    float baseAltitude;
    float layerThickness;
    float hazeDistance;
    uint seed;

    float weatherScale;
    float shapeNoiseScale;
    float detailNoiseScale;
    float evolutionTime;

    float2 windDirection;
    float shadowStrength;
    float shadowSoftness;

    float2 weatherWindOffset;
    float2 shapeWindOffset;

    float2 detailWindOffset;
    uint enabled;
    float cloudSize;
};

// matches EffectVolumeShaderData, with CloudVolumeShaderData packed at the start of params
struct CloudVolume
{
    float4 aabbMin;
    float4 aabbMax;
    CloudVolumeParams params;
    float4 _unusedParams[8];
};

// matches CloudWeatherMapShaderData in Rendering/Clouds/CloudResources.hpp
struct CloudWeatherMap
{
    float2 origin;
    float worldExtent;
    uint dimensions;

    float2 noiseOrigin;
    uint weatherNoisePeriodCells;
    uint evolutionNoisePeriodCells;

    float2 cellNoiseOrigin;
    uint cellNoisePeriodCells;
    float cellWorldSize;

    float evolutionSecondsPerCell;
    uint debugShadows;
    uint previousKeyframeSlice;
    uint nextKeyframeSlice;

    float keyframeBlend;
    float generateEvolutionTime;
    uint generateSlice;
    float _pad0;
};

// matches CloudShadowMapShaderData in Rendering/Clouds/CloudResources.hpp
struct CloudShadowMap
{
    // world XZ of the map's min corner, on the y = 0 plane
    float2 origin;
    float worldExtent;
    uint dimensions;

    // xyz = the sun direction the map is built for
    float4 directionToSun;

    uint isValid;

    // only meaningful for the dispatch filling rows [generateRowStart, generateRowStart + generateRowCount)
    uint generateRowStart;
    uint generateRowCount;
    uint _pad0;
};

// how dark a fully covered, full density patch of sky makes the ground: exp(-scale)
static const float CloudShadowOpticalDepthScale = 6.0;

// log2(CloudResources::WeatherMapOriginSnapTexels) - coarser mips shift when the map recenters, which reads as a pop
static const float CloudWeatherMapMaxStableLod = 4.0;

float2 GetCloudWeatherMapUV(CloudWeatherMap weatherMap, float2 cloudSpaceXZ)
{
    return (cloudSpaceXZ - weatherMap.origin) / weatherMap.worldExtent;
}

// Weather (r = coverage, g = cloud type, b = density) crossfaded between the two evolution keyframes
float4 SampleCloudWeatherMap(Texture2DArray weatherMapTexture, SamplerState weatherMapSampler, CloudWeatherMap weatherMap, float2 uv, float lod)
{
    const float4 previousWeather = weatherMapTexture.SampleLevel(weatherMapSampler, float3(uv, float(weatherMap.previousKeyframeSlice)), lod);
    const float4 nextWeather = weatherMapTexture.SampleLevel(weatherMapSampler, float3(uv, float(weatherMap.nextKeyframeSlice)), lod);

    return lerp(previousWeather, nextWeather, weatherMap.keyframeBlend);
}

// Transmittance from the weather map alone: flat coverage projected along the sun. Used past the cloud shadow map's edge
float GetWeatherMapCloudTransmittance(
    Texture2DArray weatherMapTexture,
    SamplerState weatherMapSampler,
    CloudVolumeParams params,
    CloudWeatherMap weatherMap,
    float3 worldPosition,
    float3 directionToLight)
{
    // clamp grazing sun angles so the projected lookup stays inside the map
    const float lightHeight = max(directionToLight.y, 0.1);
    const float layerMidAltitude = params.baseAltitude + params.layerThickness * 0.5;
    const float distanceToLayer = max(layerMidAltitude - worldPosition.y, 0.0) / lightHeight;

    const float2 cloudSpaceXZ = worldPosition.xz + directionToLight.xz * distanceToLayer + params.weatherWindOffset;
    const float2 uv = GetCloudWeatherMapUV(weatherMap, cloudSpaceXZ);

    // there is no weather data past the map's edge, so fade out before reaching it
    const float2 distanceToEdge = min(uv, 1.0 - uv);
    const float edgeFade = saturate(min(distanceToEdge.x, distanceToEdge.y) * 16.0);

    const float4 weather = SampleCloudWeatherMap(weatherMapTexture, weatherMapSampler, weatherMap, uv, min(params.shadowSoftness, CloudWeatherMapMaxStableLod));
    const float opticalDepth = weather.r * weather.b * params.densityMultiplier * CloudShadowOpticalDepthScale;

    return lerp(1.0, exp(-opticalDepth), edgeFade);
}

// Fraction of sunlight reaching worldPosition through the cloud layer. The cloud shadow map (marched through the actual
// cloud density) is used where it covers, blending out to the flat weather map projection toward its edge
float GetCloudShadow(
    Texture2DArray weatherMapTexture,
    Texture2D shadowMapTexture,
    SamplerState cloudSampler,
    CloudVolume cloudVolume,
    CloudWeatherMap weatherMap,
    CloudShadowMap shadowMap,
    float3 worldPosition,
    float3 directionToLight)
{
    const CloudVolumeParams params = cloudVolume.params;

    [branch]
    if (params.enabled == 0 || params.shadowStrength <= 0.0 || directionToLight.y <= 0.0)
    {
        return 1.0;
    }

    float transmittance = 1.0;
    float shadowMapWeight = 0.0;

    [branch]
    if (shadowMap.isValid != 0)
    {
        // rays toward the sun are parallel, so sliding the point along one onto the map's y = 0 plane is exact below the clouds
        const float lightHeight = max(directionToLight.y, 0.05);
        const float2 mapXZ = worldPosition.xz - directionToLight.xz * (worldPosition.y / lightHeight);
        const float2 uv = (mapXZ - shadowMap.origin) / shadowMap.worldExtent;

        const float2 distanceToEdge = min(uv, 1.0 - uv);
        shadowMapWeight = saturate(min(distanceToEdge.x, distanceToEdge.y) * 10.0);

        [branch]
        if (shadowMapWeight > 0.0)
        {
            // small box blur, radius in texels set by ShadowSoftness
            const float blurRadius = params.shadowSoftness * 2.0 / float(shadowMap.dimensions);

            float shadowMapTransmittance = shadowMapTexture.SampleLevel(cloudSampler, uv, 0.0).r;
            shadowMapTransmittance += shadowMapTexture.SampleLevel(cloudSampler, uv + float2(blurRadius, blurRadius), 0.0).r;
            shadowMapTransmittance += shadowMapTexture.SampleLevel(cloudSampler, uv + float2(-blurRadius, blurRadius), 0.0).r;
            shadowMapTransmittance += shadowMapTexture.SampleLevel(cloudSampler, uv + float2(blurRadius, -blurRadius), 0.0).r;
            shadowMapTransmittance += shadowMapTexture.SampleLevel(cloudSampler, uv + float2(-blurRadius, -blurRadius), 0.0).r;

            transmittance = shadowMapTransmittance * 0.2;
        }
    }

    [branch]
    if (shadowMapWeight < 1.0)
    {
        const float weatherMapTransmittance = GetWeatherMapCloudTransmittance(weatherMapTexture, cloudSampler, params, weatherMap, worldPosition, directionToLight);

        transmittance = lerp(weatherMapTransmittance, transmittance, shadowMapWeight);
    }

    const float sunElevationFade = saturate(directionToLight.y * 4.0);

    return lerp(1.0, transmittance, params.shadowStrength * sunElevationFade);
}

#endif // HYP_CLOUDS
