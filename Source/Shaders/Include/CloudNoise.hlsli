#ifndef HYP_CLOUD_NOISE
#define HYP_CLOUD_NOISE

// Tileable noise for clouds. Every function takes a lattice period per axis; positions offset by a whole
// period give the same value, so wrapped offsets and texture tiles never show a seam.

// pcg3d - Jarzynski & Olano, "Hash Functions for GPU Rendering" (2020)
uint3 CloudNoiseHash(uint3 value)
{
    value = value * 1664525u + 1013904223u;

    value.x += value.y * value.z;
    value.y += value.z * value.x;
    value.z += value.x * value.y;

    value ^= value >> 16u;

    value.x += value.y * value.z;
    value.y += value.z * value.x;
    value.z += value.x * value.y;

    return value;
}

int3 WrapLatticeCell(int3 cell, int3 period)
{
    return ((cell % period) + period) % period;
}

float3 GetLatticeGradient(int3 cell, uint seed)
{
    const uint3 hashed = CloudNoiseHash(uint3(cell) ^ uint3(seed, seed * 0x9E3779B9u, seed * 0x85EBCA6Bu));
    const float3 gradient = float3(hashed) / 4294967295.0 * 2.0 - 1.0;

    return normalize(gradient + 1e-5);
}

float GetGradientCornerContribution(int3 cell, int3 cornerOffset, float3 fraction, int3 period, uint seed)
{
    const float3 gradient = GetLatticeGradient(WrapLatticeCell(cell + cornerOffset, period), seed);

    return dot(gradient, fraction - float3(cornerOffset));
}

// Perlin gradient noise, roughly in [-0.87, 0.87]
float PeriodicGradientNoise(float3 position, int3 period, uint seed)
{
    const float3 cellFloor = floor(position);
    const int3 cell = int3(cellFloor);
    const float3 fraction = position - cellFloor;

    const float3 fade = fraction * fraction * fraction * (fraction * (fraction * 6.0 - 15.0) + 10.0);

    const float corner000 = GetGradientCornerContribution(cell, int3(0, 0, 0), fraction, period, seed);
    const float corner100 = GetGradientCornerContribution(cell, int3(1, 0, 0), fraction, period, seed);
    const float corner010 = GetGradientCornerContribution(cell, int3(0, 1, 0), fraction, period, seed);
    const float corner110 = GetGradientCornerContribution(cell, int3(1, 1, 0), fraction, period, seed);
    const float corner001 = GetGradientCornerContribution(cell, int3(0, 0, 1), fraction, period, seed);
    const float corner101 = GetGradientCornerContribution(cell, int3(1, 0, 1), fraction, period, seed);
    const float corner011 = GetGradientCornerContribution(cell, int3(0, 1, 1), fraction, period, seed);
    const float corner111 = GetGradientCornerContribution(cell, int3(1, 1, 1), fraction, period, seed);

    const float bottom = lerp(lerp(corner000, corner100, fade.x), lerp(corner010, corner110, fade.x), fade.y);
    const float top = lerp(lerp(corner001, corner101, fade.x), lerp(corner011, corner111, fade.x), fade.y);

    return lerp(bottom, top, fade.z);
}

// Each octave doubles both frequency and period, so the sum keeps the base period. Normalized by total amplitude
float PeriodicGradientFbm(float3 position, int3 period, uint seed, uint octaves)
{
    float sum = 0.0;
    float amplitude = 0.5;
    float totalAmplitude = 0.0;

    float3 octavePosition = position;
    int3 octavePeriod = period;

    for (uint octave = 0; octave < octaves; octave++)
    {
        sum += PeriodicGradientNoise(octavePosition, octavePeriod, seed + octave) * amplitude;
        totalAmplitude += amplitude;

        amplitude *= 0.5;
        octavePosition *= 2.0;
        octavePeriod *= 2;
    }

    return sum / max(totalAmplitude, 1e-5);
}

// Distance from position to the nearest feature point (one per lattice cell), in cell units - roughly [0, 1]
float PeriodicWorleyDistance(float3 position, int3 period, uint seed)
{
    const float3 cellFloor = floor(position);
    const int3 cell = int3(cellFloor);
    const float3 fraction = position - cellFloor;

    float nearestDistanceSquared = 8.0;

    for (int z = -1; z <= 1; z++)
    {
        for (int y = -1; y <= 1; y++)
        {
            for (int x = -1; x <= 1; x++)
            {
                const int3 neighbourOffset = int3(x, y, z);
                const int3 neighbourCell = WrapLatticeCell(cell + neighbourOffset, period);

                const uint3 hashed = CloudNoiseHash(uint3(neighbourCell) ^ uint3(seed, seed * 0x9E3779B9u, seed * 0x85EBCA6Bu));
                const float3 featurePoint = float3(hashed) / 4294967295.0;

                const float3 toFeature = float3(neighbourOffset) + featurePoint - fraction;

                nearestDistanceSquared = min(nearestDistanceSquared, dot(toFeature, toFeature));
            }
        }
    }

    return sqrt(nearestDistanceSquared);
}

// Inverted Worley (1 near feature points) summed over three octaves - puffy, cellular. Period is in cells for the first octave
float PeriodicWorleyFbm(float3 position, int period, uint seed)
{
    const float octave0 = 1.0 - saturate(PeriodicWorleyDistance(position, int3(period, period, period), seed));
    const float octave1 = 1.0 - saturate(PeriodicWorleyDistance(position * 2.0, int3(period, period, period) * 2, seed + 1u));
    const float octave2 = 1.0 - saturate(PeriodicWorleyDistance(position * 4.0, int3(period, period, period) * 4, seed + 2u));

    return octave0 * 0.625 + octave1 * 0.25 + octave2 * 0.125;
}

// Maps value from [oldMin, oldMax] to [newMin, newMax], unclamped
float RemapCloudValue(float value, float oldMin, float oldMax, float newMin, float newMax)
{
    return newMin + (value - oldMin) / max(oldMax - oldMin, 1e-5) * (newMax - newMin);
}

#endif // HYP_CLOUD_NOISE
