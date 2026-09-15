#include "../include/Defines.hlsli"
#include "../include/Clouds.hlsli"
#include "../include/CloudNoise.hlsli"

DECLARE_UAV(Clouds, OutWeatherMap) RWTexture2DArray<float4> OutWeatherMap;

DECLARE_BUFFER_DYNAMIC(Clouds, CloudWeatherMapConstants) cbuffer CloudWeatherMapConstants
{
    CloudVolume cloudVolume;
    CloudWeatherMap weatherMap;
};

static const uint RegionalOctaves = 3;
static const uint WarpOctaves = 2;
static const uint CellOctaves = 4;
static const uint DetailOctaves = 3;

// in weather cells - warping bends regional blobs into clusters and streaks
static const float WarpStrength = 0.6;

// how far regional weather pushes local coverage away from the Coverage setting
static const float RegionalCoverageVariation = 0.35;

// width of the soft edge between clear sky and cloud, in normalized noise units
static const float CoverageEdgeWidth = 0.12;

// fbm sums are narrow around zero; this spreads them over most of [0, 1] so thresholds behave like area fractions
float NormalizeFbm(float value)
{
    return saturate(value * 1.6 + 0.5);
}

[numthreads(8, 8, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    if (dispatchThreadId.x >= weatherMap.dimensions || dispatchThreadId.y >= weatherMap.dimensions)
    {
        return;
    }

    const CloudVolumeParams params = cloudVolume.params;

    const float2 uv = (float2(dispatchThreadId.xy) + 0.5) / float(weatherMap.dimensions);
    const float2 worldOffset = uv * weatherMap.worldExtent;

    const float noiseTime = weatherMap.generateEvolutionTime / weatherMap.evolutionSecondsPerCell;
    const int timePeriod = int(weatherMap.evolutionNoisePeriodCells);

    // regional weather: where it is cloudier or clearer, and which kind of cloud forms there
    const float2 weatherXZ = weatherMap.noiseOrigin + worldOffset / params.weatherScale;
    const int weatherPeriod = int(weatherMap.weatherNoisePeriodCells);
    const int3 weatherLatticePeriod = int3(weatherPeriod, weatherPeriod, timePeriod);

    const float2 warp = float2(
        PeriodicGradientFbm(float3(weatherXZ, noiseTime), weatherLatticePeriod, params.seed + 101u, WarpOctaves),
        PeriodicGradientFbm(float3(weatherXZ + float2(5.2, 1.3), noiseTime), weatherLatticePeriod, params.seed + 211u, WarpOctaves)) * WarpStrength;

    const float regionalNoise = NormalizeFbm(PeriodicGradientFbm(float3(weatherXZ + warp, noiseTime), weatherLatticePeriod, params.seed, RegionalOctaves));

    // scaling by 1/4 keeps the period whole
    const float typeNoise = NormalizeFbm(PeriodicGradientFbm(
        float3(weatherXZ * 0.25, noiseTime),
        int3(weatherPeriod / 4, weatherPeriod / 4, timePeriod),
        params.seed + 307u,
        DetailOctaves));

    // individual clouds, on their own lattice sized to repeat with the weather noise
    const float2 cellXZ = weatherMap.cellNoiseOrigin + worldOffset / weatherMap.cellWorldSize;
    const int cellPeriod = int(weatherMap.cellNoisePeriodCells);

    const float cellNoise = NormalizeFbm(PeriodicGradientFbm(
        float3(cellXZ, noiseTime * 4.0),
        int3(cellPeriod, cellPeriod, timePeriod * 4),
        params.seed + 503u,
        CellOctaves));

    const float densityNoise = NormalizeFbm(PeriodicGradientFbm(
        float3(cellXZ * 0.5, noiseTime * 2.0),
        int3(cellPeriod / 2, cellPeriod / 2, timePeriod * 2),
        params.seed + 401u,
        DetailOctaves));

    const float localCoverage = saturate(params.coverage + (regionalNoise - 0.5) * 2.0 * RegionalCoverageVariation);
    const float coverageThreshold = 1.0 - localCoverage;

    const float coverage = saturate((cellNoise - coverageThreshold) / CoverageEdgeWidth) * step(0.001, localCoverage);

    const float cloudType = saturate(params.cloudTypeBias + (typeNoise - 0.5) * 0.8);
    const float density = saturate(0.6 + (densityNoise - 0.5) * 0.8);

    OutWeatherMap[uint3(dispatchThreadId.xy, weatherMap.generateSlice)] = float4(coverage, cloudType, density, 1.0);
}
