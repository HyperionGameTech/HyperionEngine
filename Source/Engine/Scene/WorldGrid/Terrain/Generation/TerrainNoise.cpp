/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <Scene/WorldGrid/Terrain/Generation/TerrainNoise.hpp>

#include <cmath>

namespace Hyperion {

namespace {

constexpr uint32 GradientCount = 8;

constexpr float GradientDirections[GradientCount][2] = {
    { 1.0f, 1.0f },
    { -1.0f, 1.0f },
    { 1.0f, -1.0f },
    { -1.0f, -1.0f },
    { 1.0f, 0.0f },
    { -1.0f, 0.0f },
    { 0.0f, 1.0f },
    { 0.0f, -1.0f }
};

HYP_FORCE_INLINE float SimplexCorner(uint32 hash, float dx, float dy)
{
    const float t = 0.5f - dx * dx - dy * dy;

    if (t <= 0.0f)
    {
        return 0.0f;
    }

    const uint32 gradientIndex = hash & (GradientCount - 1u);
    const float dot = GradientDirections[gradientIndex][0] * dx + GradientDirections[gradientIndex][1] * dy;

    const float t2 = t * t;

    return t2 * t2 * dot;
}

} // namespace

float TerrainSimplex2D(uint32 seed, float x, float y)
{
    constexpr float F2 = 0.36602540378443865f; // (sqrt(3) - 1) / 2
    constexpr float G2 = 0.21132486540518713f; // (3 - sqrt(3)) / 6

    const float s = (x + y) * F2;
    const int32 i = int32(std::floor(x + s));
    const int32 j = int32(std::floor(y + s));

    const float t = float(i + j) * G2;
    const float x0 = x - (float(i) - t);
    const float y0 = y - (float(j) - t);

    const int32 i1 = x0 > y0 ? 1 : 0;
    const int32 j1 = x0 > y0 ? 0 : 1;

    const float x1 = x0 - float(i1) + G2;
    const float y1 = y0 - float(j1) + G2;
    const float x2 = x0 - 1.0f + 2.0f * G2;
    const float y2 = y0 - 1.0f + 2.0f * G2;

    float n = 0.0f;
    n += SimplexCorner(TerrainHashCoord(seed, i, j), x0, y0);
    n += SimplexCorner(TerrainHashCoord(seed, i + i1, j + j1), x1, y1);
    n += SimplexCorner(TerrainHashCoord(seed, i + 1, j + 1), x2, y2);

    return 70.0f * n;
}

float TerrainFbm2D(uint32 seed, float x, float y, uint32 octaves, float lacunarity, float gain)
{
    float sum = 0.0f;
    float amplitude = 1.0f;
    float frequency = 1.0f;
    float norm = 0.0f;

    for (uint32 octave = 0; octave < octaves; octave++)
    {
        sum += amplitude * TerrainSimplex2D(TerrainHashOctaveSeed(seed, octave), x * frequency, y * frequency);
        norm += amplitude;

        amplitude *= gain;
        frequency *= lacunarity;
    }

    return sum / MathUtil::Max(norm, 1e-6f);
}

float TerrainRidged2D(uint32 seed, float x, float y, uint32 octaves, float lacunarity, float gain)
{
    float sum = 0.0f;
    float amplitude = 1.0f;
    float frequency = 1.0f;
    float norm = 0.0f;
    float weight = 1.0f;

    for (uint32 octave = 0; octave < octaves; octave++)
    {
        float signal = 1.0f - MathUtil::Abs(TerrainSimplex2D(TerrainHashOctaveSeed(seed, octave, 1u), x * frequency, y * frequency));
        signal *= signal;
        signal *= weight;

        weight = MathUtil::Clamp(signal * 2.0f, 0.0f, 1.0f);

        sum += signal * amplitude;
        norm += amplitude;

        amplitude *= gain;
        frequency *= lacunarity;
    }

    return sum / MathUtil::Max(norm, 1e-6f);
}

} // namespace Hyperion
