/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <Scene/WorldGrid/Terrain/Generation/TerrainGenerator.hpp>
#include <Scene/WorldGrid/Terrain/Generation/TerrainNoise.hpp>

#include <TerrainGenerator.generated.inl>

namespace Hyperion {

#pragma region TerrainGenerator

void TerrainGenerator::Configure(const TerrainGenerationParams& params)
{
    m_params = params;
}

uint32 TerrainGenerator::GetErosionMargin() const
{
    return MathUtil::Max(m_params.thermalErosionIterations, m_params.hydraulicSmoothingIterations) + 1u;
}

float TerrainGenerator::SampleAnalyticHeight(const Vec2f& worldXZ) const
{
    const TerrainGenerationParams& params = m_params;

    const Vec2f warped = TerrainDomainWarp(params.seed ^ 0xA53B4C7Du, worldXZ, params.warpFrequency, params.warpStrength);

    // low frequency mask deciding where mountain ranges rise
    float region = TerrainFbm2D(params.seed ^ 0x77E1D2A4u, worldXZ.x * params.mountainRegionFrequency, worldXZ.y * params.mountainRegionFrequency, 4);
    region = region * 0.5f + 0.5f;

    const float mountainMask = TerrainSmoothStep(
        params.mountainRegionThreshold,
        params.mountainRegionThreshold + params.mountainRegionFalloff,
        region);

    // rolling hills
    const float base = TerrainFbm2D(
        params.seed ^ 0x11F0A3E9u,
        warped.x * params.baseFrequency,
        warped.y * params.baseFrequency,
        params.baseOctaves)
        * params.baseAmplitude;

    // ridged mountain multifractal
    float ridge = TerrainRidged2D(
        params.seed ^ 0x22B7C9F5u,
        warped.x * params.mountainFrequency,
        warped.y * params.mountainFrequency,
        params.mountainOctaves);

    ridge = MathUtil::Pow(ridge, params.mountainSharpness);

    // terracing - flatten mountain steps for a layered, eroded strata look
    if (params.mountainPlateau > 0.0f)
    {
        constexpr uint32 plateauSteps = 5;

        const float stepped = std::floor(ridge * float(plateauSteps)) / float(plateauSteps);
        const float steppedSmooth = stepped + (TerrainSmoothStep(0.0f, 1.0f, ridge * float(plateauSteps) - stepped * float(plateauSteps)) / float(plateauSteps));

        ridge = MathUtil::Lerp(ridge, steppedSmooth, params.mountainPlateau);
    }

    // winding gullies carved into mountain slopes
    const float channels = TerrainRidged2D(
        params.seed ^ 0x33D2E7B1u,
        warped.x * params.gullyFrequency,
        warped.y * params.gullyFrequency,
        3,
        2.1f);

    const float mountains = ridge * mountainMask;

    return base
        + mountains * params.mountainAmplitude
        - channels * channels * mountains * params.mountainAmplitude * params.gullyDepth;
}

void TerrainGenerator::GenerateCellHeights(
    const Vec2f& cellWorldMinXZ,
    const Vec2f& scaleXZ,
    uint32 cellSize,
    Array<float>& outHeights) const
{
    outHeights.Resize(size_t(cellSize) * size_t(cellSize));

    Array<float> paddedHeights;
    GeneratePaddedCellHeights(cellWorldMinXZ, scaleXZ, cellSize, paddedHeights);

    const uint32 paddedSize = cellSize + GetErosionMargin() * 2u;

    for (uint32 z = 0; z < cellSize; z++)
    {
        for (uint32 x = 0; x < cellSize; x++)
        {
            outHeights[size_t(z) * cellSize + x] = paddedHeights[size_t(z + GetErosionMargin()) * paddedSize + (x + GetErosionMargin())];
        }
    }
}

void TerrainGenerator::GeneratePaddedCellHeights(
    const Vec2f& cellWorldMinXZ,
    const Vec2f& scaleXZ,
    uint32 cellSize,
    Array<float>& outPaddedHeights) const
{
    const uint32 margin = GetErosionMargin();
    const uint32 paddedSize = cellSize + margin * 2u;

    outPaddedHeights.Resize(size_t(paddedSize) * size_t(paddedSize));

    for (uint32 pz = 0; pz < paddedSize; pz++)
    {
        const int32 lz = int32(pz) - int32(margin);

        for (uint32 px = 0; px < paddedSize; px++)
        {
            const int32 lx = int32(px) - int32(margin);

            const Vec2f worldXZ = cellWorldMinXZ + Vec2f(float(lx), float(lz)) * scaleXZ;

            outPaddedHeights[size_t(pz) * paddedSize + px] = SampleAnalyticHeight(worldXZ);
        }
    }

    ApplyErosion(outPaddedHeights, paddedSize, scaleXZ);
}

void TerrainGenerator::SynthesizeSplatWeights(
    Span<const float> heights,
    const Vec2f& cellWorldMinXZ,
    const Vec2f& scaleXZ,
    uint32 cellSize,
    Span<ubyte> outWeights) const
{
    const TerrainGenerationParams& params = m_params;

    Assert(outWeights.Size() >= size_t(cellSize) * size_t(cellSize) * 4u,
        "Splat weight buffer too small");

    if (outWeights.Size() < size_t(cellSize) * size_t(cellSize) * 4u)
    {
        return;
    }

    const float step = (scaleXZ.x + scaleXZ.y) * 0.5f;
    const float snowLine = MathUtil::Max(params.mountainAmplitude, 1.0f) * params.snowLineFraction;

    for (uint32 z = 0; z < cellSize; z++)
    {
        for (uint32 x = 0; x < cellSize; x++)
        {
            const size_t index = size_t(z) * cellSize + x;
            const float height = heights[index];

            // central differences, clamped at cell borders (one-sided there)
            const float heightL = heights[size_t(z) * cellSize + MathUtil::Max(x, 1u) - 1u];
            const float heightR = heights[size_t(z) * cellSize + MathUtil::Min(x + 1u, cellSize - 1u)];
            const float heightD = heights[size_t(MathUtil::Max(z, 1u) - 1u) * cellSize + x];
            const float heightU = heights[size_t(MathUtil::Min(z + 1u, cellSize - 1u)) * cellSize + x];

            const float dhdx = (heightR - heightL) / (2.0f * step);
            const float dhdz = (heightU - heightD) / (2.0f * step);

            const float normalY = 1.0f / std::sqrt(1.0f + dhdx * dhdx + dhdz * dhdz);
            const float slope = 1.0f - normalY;

            const Vec2f worldXZ = cellWorldMinXZ + Vec2f(float(x), float(z)) * scaleXZ;

            const float breakup = TerrainFbm2D(params.seed ^ 0x44C3A921u, worldXZ.x * 0.02f, worldXZ.y * 0.02f, 3) * 0.5f + 0.5f;
            const float patches = TerrainFbm2D(params.seed ^ 0x55E8F13Bu, worldXZ.x * 0.06f, worldXZ.y * 0.06f, 3) * 0.5f + 0.5f;

            // rock takes over on steep slopes, with a noisy boundary
            const float rockStart = 0.30f + breakup * 0.15f;
            const float rock = TerrainSmoothStep(rockStart, rockStart + 0.2f, slope);

            // snow caps on high ground that isn't too steep to hold snow
            const float snowLineJittered = snowLine * (0.85f + breakup * 0.3f);
            const float snow = TerrainSmoothStep(snowLineJittered - snowLine * 0.12f, snowLineJittered + snowLine * 0.12f, height)
                * TerrainSmoothStep(0.55f, 0.35f, slope);

            // dirt patches on shallow ground
            const float dirt = TerrainSmoothStep(0.55f, 0.75f, patches)
                * (1.0f - rock)
                * (1.0f - snow)
                * 0.65f;

            const float grass = MathUtil::Max(1.0f - rock - snow - dirt, 0.0f);

            const float weights[4] = { grass, rock, dirt, snow };

            ubyte* outSplat = outWeights.Data() + index * 4u;

            for (uint32 layer = 0; layer < 4; layer++)
            {
                outSplat[layer] = ubyte(MathUtil::Clamp(weights[layer], 0.0f, 1.0f) * 255.0f);
            }
        }
    }
}

void TerrainGenerator::ApplyErosion(Span<float> paddedHeights, uint32 size, const Vec2f& scaleXZ) const
{
    const uint32 margin = GetErosionMargin();

    ApplyThermalErosion(paddedHeights, size, margin, scaleXZ);
    ApplyHydraulicPasses(paddedHeights, size, margin, scaleXZ);
}

void TerrainGenerator::ApplyThermalErosion(Span<float> heights, uint32 size, uint32 margin, const Vec2f& scaleXZ) const
{
    const TerrainGenerationParams& params = m_params;

    if (params.thermalErosionIterations == 0)
    {
        return;
    }

    const float step = (scaleXZ.x + scaleXZ.y) * 0.5f;
    const float talus = std::tan(params.talusAngle * MathUtil::pi<float> / 180.0f) * step;

    constexpr int32 NeighborOffsets[8][2] = {
        { -1, -1 }, { 0, -1 }, { 1, -1 },
        { -1,  0 },            { 1,  0 },
        { -1,  1 }, { 0,  1 }, { 1,  1 }
    };

    // each iteration's result only depends on data one ring further out, so the updated
    // range shrinks inward - identical to what any neighboring cell would compute.
    int32 ring = int32(margin);

    for (uint32 iteration = 0; iteration < params.thermalErosionIterations && ring > 1; iteration++, ring--)
    {
        for (int32 z = ring; z < int32(size) - ring; z++)
        {
            for (int32 x = ring; x < int32(size) - ring; x++)
            {
                const size_t index = size_t(z) * size + size_t(x);
                const float height = heights[index];

                float deltas[8];
                float deltaTotal = 0.0f;

                for (uint32 n = 0; n < 8; n++)
                {
                    const size_t neighborIndex = size_t(z + NeighborOffsets[n][1]) * size + size_t(x + NeighborOffsets[n][0]);

                    deltas[n] = height - heights[neighborIndex];

                    if (deltas[n] > talus)
                    {
                        deltaTotal += deltas[n];
                    }
                }

                if (deltaTotal <= 0.0f)
                {
                    continue;
                }

                for (uint32 n = 0; n < 8; n++)
                {
                    if (deltas[n] <= talus)
                    {
                        continue;
                    }

                    float move = params.thermalErosionRate * talus * (deltas[n] / deltaTotal);
                    move = MathUtil::Min(move, deltas[n] * 0.5f);

                    heights[index] -= move;
                    heights[size_t(z + NeighborOffsets[n][1]) * size + size_t(x + NeighborOffsets[n][0])] += move;
                }
            }
        }
    }
}

void TerrainGenerator::ApplyHydraulicPasses(Span<float> heights, uint32 size, uint32 margin, const Vec2f& scaleXZ) const
{
    const TerrainGenerationParams& params = m_params;

    if (params.hydraulicSmoothingIterations == 0)
    {
        return;
    }

    const float step = MathUtil::Max((scaleXZ.x + scaleXZ.y) * 0.5f, 1e-6f);

    // sediment fill on gentle slopes, channel carving on steep ones - both purely local,
    // keeping the pass seamless across cells.
    int32 ring = int32(margin);

    for (uint32 iteration = 0; iteration < params.hydraulicSmoothingIterations && ring > 1; iteration++, ring--)
    {
        for (int32 z = ring; z < int32(size) - ring; z++)
        {
            for (int32 x = ring; x < int32(size) - ring; x++)
            {
                const size_t index = size_t(z) * size + size_t(x);

                const float height = heights[index];
                const float average = 0.25f
                    * (heights[index - 1]
                        + heights[index + 1]
                        + heights[index - size]
                        + heights[index + size]);

                const float delta = height - average;
                const float slope = MathUtil::Abs(delta) / step;

                if (slope > params.carveThreshold)
                {
                    // deepen flow channels
                    heights[index] = height - MathUtil::Sign(delta) * (slope - params.carveThreshold) * step * params.carveStrength;
                }
                else
                {
                    // settle sediment into depressions
                    heights[index] = height - delta * params.depositStrength * (1.0f - slope / params.carveThreshold);
                }
            }
        }
    }
}

#pragma endregion TerrainGenerator

} // namespace Hyperion
