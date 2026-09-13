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

TerrainNoiseSample TerrainSimplex2DGrad(uint32 seed, float x, float y)
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

    const int32 cornerOffsets[3][2] = { { 0, 0 }, { i1, j1 }, { 1, 1 } };

    const float cornerDeltas[3][2] = {
        { x0, y0 },
        { x0 - float(i1) + G2, y0 - float(j1) + G2 },
        { x0 - 1.0f + 2.0f * G2, y0 - 1.0f + 2.0f * G2 }
    };

    TerrainNoiseSample result;

    for (uint32 corner = 0; corner < 3; corner++)
    {
        const float dx = cornerDeltas[corner][0];
        const float dy = cornerDeltas[corner][1];

        const float falloff = 0.5f - dx * dx - dy * dy;

        if (falloff <= 0.0f)
        {
            continue;
        }

        const uint32 hash = TerrainHashCoord(seed, i + cornerOffsets[corner][0], j + cornerOffsets[corner][1]);
        const uint32 gradientIndex = hash & (GradientCount - 1u);

        const float gradientX = GradientDirections[gradientIndex][0];
        const float gradientY = GradientDirections[gradientIndex][1];

        const float dot = gradientX * dx + gradientY * dy;

        const float falloff2 = falloff * falloff;
        const float falloff3 = falloff2 * falloff;
        const float falloff4 = falloff2 * falloff2;

        // d/dp [falloff^4 * dot] = falloff^4 * g - 8 * falloff^3 * dot * d
        result.value += falloff4 * dot;
        result.gradient.x += falloff4 * gradientX - 8.0f * falloff3 * dot * dx;
        result.gradient.y += falloff4 * gradientY - 8.0f * falloff3 * dot * dy;
    }

    result.value *= 70.0f;
    result.gradient *= 70.0f;

    return result;
}

TerrainNoiseSample TerrainFbm2DGrad(uint32 seed, float x, float y, uint32 octaves, float lacunarity, float gain)
{
    TerrainNoiseSample sum;
    float amplitude = 1.0f;
    float frequency = 1.0f;
    float norm = 0.0f;

    for (uint32 octave = 0; octave < octaves; octave++)
    {
        const TerrainNoiseSample octaveSample = TerrainSimplex2DGrad(TerrainHashOctaveSeed(seed, octave), x * frequency, y * frequency);

        sum.value += amplitude * octaveSample.value;
        sum.gradient += octaveSample.gradient * (amplitude * frequency);
        norm += amplitude;

        amplitude *= gain;
        frequency *= lacunarity;
    }

    norm = MathUtil::Max(norm, 1e-6f);

    sum.value /= norm;
    sum.gradient /= norm;

    return sum;
}

TerrainNoiseSample TerrainRidged2DGrad(uint32 seed, float x, float y, uint32 octaves, float lacunarity, float gain)
{
    TerrainNoiseSample sum;
    float amplitude = 1.0f;
    float frequency = 1.0f;
    float norm = 0.0f;

    float weight = 1.0f;
    Vec2f weightGradient;

    for (uint32 octave = 0; octave < octaves; octave++)
    {
        const TerrainNoiseSample octaveSample = TerrainSimplex2DGrad(TerrainHashOctaveSeed(seed, octave, 1u), x * frequency, y * frequency);

        const float crease = 1.0f - MathUtil::Abs(octaveSample.value);
        const Vec2f creaseGradient = octaveSample.gradient * (octaveSample.value >= 0.0f ? -frequency : frequency);

        // each octave is damped by the previous one so detail concentrates on the ridge lines
        const float signal = crease * crease * weight;
        const Vec2f signalGradient = creaseGradient * (2.0f * crease * weight) + weightGradient * (crease * crease);

        const float nextWeight = signal * 2.0f;

        if (nextWeight >= 1.0f)
        {
            weight = 1.0f;
            weightGradient = Vec2f::Zero();
        }
        else
        {
            weight = MathUtil::Max(nextWeight, 0.0f);
            weightGradient = signalGradient * 2.0f;
        }

        sum.value += signal * amplitude;
        sum.gradient += signalGradient * amplitude;
        norm += amplitude;

        amplitude *= gain;
        frequency *= lacunarity;
    }

    norm = MathUtil::Max(norm, 1e-6f);

    sum.value /= norm;
    sum.gradient /= norm;

    return sum;
}

TerrainNoiseSample TerrainGully2D(uint32 seed, float x, float y, const Vec2f& flowDirection)
{
    constexpr float TwoPi = 6.283185307179586f;
    constexpr float KernelSharpness = 2.5f;

    const float cellX = std::floor(x);
    const float cellY = std::floor(y);

    const float fractionX = x - cellX;
    const float fractionY = y - cellY;

    // the stripe phase advances across the flow, so crests and troughs run downhill
    const Vec2f across(-flowDirection.y, flowDirection.x);

    float valueSum = 0.0f;
    Vec2f gradientSum;
    float weightSum = 0.0f;

    for (int32 offsetY = -1; offsetY <= 2; offsetY++)
    {
        for (int32 offsetX = -1; offsetX <= 2; offsetX++)
        {
            const uint32 hash = TerrainHashCoord(seed, int32(cellX) + offsetX, int32(cellY) + offsetY);

            const float jitterX = float(hash & 0xFFFFu) / 65535.0f * 0.5f;
            const float jitterY = float((hash >> 16) & 0xFFFFu) / 65535.0f * 0.5f;

            const float toSampleX = fractionX - float(offsetX) - jitterX;
            const float toSampleY = fractionY - float(offsetY) - jitterY;

            const float weight = std::exp(-KernelSharpness * (toSampleX * toSampleX + toSampleY * toSampleY));
            const float phase = (toSampleX * across.x + toSampleY * across.y) * TwoPi;

            valueSum += std::cos(phase) * weight;
            gradientSum += across * (-std::sin(phase) * TwoPi * weight);
            weightSum += weight;
        }
    }

    weightSum = MathUtil::Max(weightSum, 1e-6f);

    TerrainNoiseSample result;
    result.value = valueSum / weightSum;
    result.gradient = gradientSum / weightSum;

    return result;
}

} // namespace Hyperion
