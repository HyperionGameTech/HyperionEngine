#ifndef HYP_CLOUD_DENSITY
#define HYP_CLOUD_DENSITY

#define CLOUD_EXTINCTION 0.04
#define CLOUD_TOP_WIND_SKEW 600.0

float GetHeightGradient(float heightFraction, float cloudType)
{
    const float stratus = saturate(RemapCloudValue(heightFraction, 0.0, 0.1, 0.0, 1.0)) * saturate(RemapCloudValue(heightFraction, 0.2, 0.3, 1.0, 0.0));
    const float stratocumulus = saturate(RemapCloudValue(heightFraction, 0.0, 0.2, 0.0, 1.0)) * saturate(RemapCloudValue(heightFraction, 0.35, 0.6, 1.0, 0.0));
    const float cumulus = saturate(RemapCloudValue(heightFraction, 0.0, 0.1, 0.0, 1.0)) * saturate(RemapCloudValue(heightFraction, 0.65, 1.0, 1.0, 0.0));

    return cloudType < 0.5
        ? lerp(stratus, stratocumulus, cloudType * 2.0)
        : lerp(stratocumulus, cumulus, cloudType * 2.0 - 1.0);
}

float SampleCloudDensity(
    float3 position,
    float heightFraction,
    bool detail)
{
    const CloudVolumeParams params = cloudVolume.params;

    if (heightFraction < 0.0 || heightFraction > 1.0)
    {
        return 0.0;
    }

    const float2 weatherUV = GetCloudWeatherMapUV(cloudWeatherMap, position.xz + params.weatherWindOffset);

    if (any(weatherUV < 0.0) || any(weatherUV > 1.0))
    {
        return 0.0;
    }

    const float4 weather = SampleCloudWeatherMap(CloudWeatherMapTexture, SamplerLinear, cloudWeatherMap, weatherUV, 0.0);
    const float coverage = weather.r;

    if (coverage <= 0.001)
    {
        return 0.0;
    }

    const float3 windDirection = float3(params.windDirection.x, 0.0, params.windDirection.y);
    const float3 skewedPosition = position + windDirection * (heightFraction * CLOUD_TOP_WIND_SKEW);

    const float3 shapeUVW = (skewedPosition + float3(params.shapeWindOffset.x, 0.0, params.shapeWindOffset.y)) / params.shapeNoiseScale;
    const float4 shapeNoise = CloudShapeNoiseTexture.SampleLevel(SamplerLinear, shapeUVW, 0.0);

    const float shapeFbm = dot(shapeNoise.gba, float3(0.625, 0.25, 0.125));
    const float baseShape = saturate(RemapCloudValue(shapeNoise.r, shapeFbm - 1.0, 1.0, 0.0, 1.0));

    const float heightGradient = GetHeightGradient(heightFraction, weather.g);

    float density = saturate(RemapCloudValue(baseShape * heightGradient, 1.0 - coverage, 1.0, 0.0, 1.0)) * coverage;

    if (density <= 0.0)
    {
        return 0.0;
    }

    if (detail)
    {
        const float3 detailUVW = (skewedPosition + float3(params.detailWindOffset.x, 0.0, params.detailWindOffset.y)) / params.detailNoiseScale;
        const float3 detailNoise = CloudDetailNoiseTexture.SampleLevel(SamplerLinear, detailUVW, 0.0).rgb;

        const float detailFbm = dot(detailNoise, float3(0.625, 0.25, 0.125));

        // wispy toward the base, billowy toward the top
        const float detailModifier = lerp(detailFbm, 1.0 - detailFbm, saturate(heightFraction * 10.0));

        density = saturate(RemapCloudValue(density, detailModifier * params.detailErosion, 1.0, 0.0, 1.0));
    }

    return density * weather.b * params.densityMultiplier;
}

#endif // HYP_CLOUD_DENSITY
