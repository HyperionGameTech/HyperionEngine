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
    return m_params.thermalErosionIterations + m_params.hydraulicSmoothingIterations + 1u;
}

float TerrainGenerator::SampleAnalyticHeight(const Vec2f& worldXZ) const
{
    const TerrainGenerationParams& params = m_params;

    const float warpScale = params.warpStrength * params.warpFrequency;

    const TerrainNoiseSample warpX = TerrainFbm2DGrad(
        TerrainHashU32(params.seed ^ 0x51ED2701u),
        worldXZ.x * params.warpFrequency + 11.3f,
        worldXZ.y * params.warpFrequency + 7.7f,
        4);

    const TerrainNoiseSample warpZ = TerrainFbm2DGrad(
        TerrainHashU32(params.seed ^ 0x68BC21EBu),
        worldXZ.x * params.warpFrequency + 3.1f,
        worldXZ.y * params.warpFrequency + 17.9f,
        4);

    const Vec2f warped(
        worldXZ.x + warpX.value * params.warpStrength,
        worldXZ.y + warpZ.value * params.warpStrength);

    const auto unwarpGradient = [&](const Vec2f& gradient) -> Vec2f
    {
        return Vec2f(
            gradient.x * (1.0f + warpScale * warpX.gradient.x) + gradient.y * (warpScale * warpZ.gradient.x),
            gradient.x * (warpScale * warpX.gradient.y) + gradient.y * (1.0f + warpScale * warpZ.gradient.y));
    };

    const TerrainNoiseSample region = TerrainFbm2DGrad(
        params.seed ^ 0x77E1D2A4u,
        worldXZ.x * params.mountainRegionFrequency,
        worldXZ.y * params.mountainRegionFrequency,
        4);

    const float regionValue = region.value * 0.5f + 0.5f;
    const Vec2f regionGradient = region.gradient * (0.5f * params.mountainRegionFrequency);

    const float maskEdge0 = params.mountainRegionThreshold;
    const float maskEdge1 = params.mountainRegionThreshold + params.mountainRegionFalloff;

    const float mountainMask = TerrainSmoothStep(maskEdge0, maskEdge1, regionValue);
    const Vec2f mountainMaskGradient = regionGradient * TerrainSmoothStepDerivative(maskEdge0, maskEdge1, regionValue);

    // rolling hills
    const TerrainNoiseSample hills = TerrainFbm2DGrad(
        params.seed ^ 0x11F0A3E9u,
        warped.x * params.baseFrequency,
        warped.y * params.baseFrequency,
        params.baseOctaves);

    const float hillsHeight = hills.value * params.baseAmplitude;
    const Vec2f hillsGradient = unwarpGradient(hills.gradient * (params.baseFrequency * params.baseAmplitude));

    // ridged mountain multifractal
    const TerrainNoiseSample ridge = TerrainRidged2DGrad(
        params.seed ^ 0x22B7C9F5u,
        warped.x * params.mountainFrequency,
        warped.y * params.mountainFrequency,
        params.mountainOctaves,
        2.02f,
        params.mountainGain);

    const float ridgeBase = MathUtil::Max(ridge.value, 1e-4f);
    const float ridgeValue = MathUtil::Pow(ridgeBase, params.mountainSharpness);

    const Vec2f ridgeGradient = unwarpGradient(ridge.gradient
        * (params.mountainFrequency * params.mountainSharpness * MathUtil::Pow(ridgeBase, params.mountainSharpness - 1.0f)));

    const float hillsWeight = 1.0f - 0.5f * mountainMask;

    const float height = hillsHeight * hillsWeight
        + ridgeValue * mountainMask * params.mountainAmplitude;

    const Vec2f gradient = hillsGradient * hillsWeight
        - mountainMaskGradient * (0.5f * hillsHeight)
        + (ridgeGradient * mountainMask + mountainMaskGradient * ridgeValue) * params.mountainAmplitude;

    if (params.gullyOctaves == 0 || params.gullyDepth <= 0.0f)
    {
        return height;
    }

    // gullies only form where there is slope for water to run down
    const float slopeFade = TerrainSmoothStep(0.1f, 0.6f, gradient.Length());

    if (slopeFade <= 0.0f)
    {
        return height;
    }

    float amplitudeSum = 0.0f;

    for (uint32 octave = 0; octave < params.gullyOctaves; octave++)
    {
        amplitudeSum += MathUtil::Pow(params.gullyGain, float(octave));
    }

    float gullies = 0.0f;
    float amplitude = 1.0f / MathUtil::Max(amplitudeSum, 1e-6f);
    float frequency = params.gullyFrequency;

    Vec2f flowGradient = gradient;

    for (uint32 octave = 0; octave < params.gullyOctaves; octave++)
    {
        const float flowLength = flowGradient.Length();
        const Vec2f flowDirection = flowLength > 1e-6f ? flowGradient / flowLength : Vec2f(0.0f, 1.0f);

        const TerrainNoiseSample gully = TerrainGully2D(
            TerrainHashOctaveSeed(params.seed ^ 0x33D2E7B1u, octave),
            worldXZ.x * frequency,
            worldXZ.y * frequency,
            flowDirection);

        gullies += gully.value * amplitude;

        // finer octaves follow the slope including the coarser gullies, so they branch off them
        flowGradient += gully.gradient * (frequency * amplitude * params.gullyDepth);

        amplitude *= params.gullyGain;
        frequency *= 2.0f;
    }

    // biased toward carving so ridge crests and peaks keep their silhouette
    return height + (gullies - 0.5f) * params.gullyDepth * slopeFade;
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

    const uint32 margin = GetErosionMargin();
    const uint32 paddedSize = cellSize + margin * 2u;

    for (uint32 z = 0; z < cellSize; z++)
    {
        for (uint32 x = 0; x < cellSize; x++)
        {
            outHeights[size_t(z) * cellSize + x] = paddedHeights[size_t(z + margin) * paddedSize + (x + margin)];
        }
    }
}

void TerrainGenerator::GenerateCellHeightsAndNormals(
    const Vec2f& cellWorldMinXZ,
    const Vec2f& scaleXZ,
    uint32 cellSize,
    Array<float>& outHeights,
    Array<Vec3f>& outNormals) const
{
    outHeights.Resize(size_t(cellSize) * size_t(cellSize));
    outNormals.Resize(size_t(cellSize) * size_t(cellSize));

    Array<float> paddedHeights;
    GeneratePaddedCellHeights(cellWorldMinXZ, scaleXZ, cellSize, paddedHeights);

    const uint32 margin = GetErosionMargin();
    const uint32 paddedSize = cellSize + margin * 2u;

    const auto paddedHeightAt = [&](int32 x, int32 z) -> float
    {
        return paddedHeights[size_t(z + int32(margin)) * paddedSize + size_t(x + int32(margin))];
    };

    for (uint32 z = 0; z < cellSize; z++)
    {
        for (uint32 x = 0; x < cellSize; x++)
        {
            const size_t index = size_t(z) * cellSize + x;

            outHeights[index] = paddedHeightAt(int32(x), int32(z));

            outNormals[index] = ComputeGridNormal(
                paddedHeightAt(int32(x) - 1, int32(z)),
                paddedHeightAt(int32(x) + 1, int32(z)),
                paddedHeightAt(int32(x), int32(z) - 1),
                paddedHeightAt(int32(x), int32(z) + 1));
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
    Span<const Vec3f> normals,
    const Vec2f& cellWorldMinXZ,
    const Vec2f& scaleXZ,
    uint32 cellSize,
    Span<ubyte> outWeights) const
{
    const TerrainGenerationParams& params = m_params;

    const size_t vertexCount = size_t(cellSize) * size_t(cellSize);

    Assert(outWeights.Size() >= vertexCount * 4u, "Splat weight buffer too small");
    Assert(heights.Size() >= vertexCount && normals.Size() >= vertexCount, "Splat synthesis input too small");

    if (outWeights.Size() < vertexCount * 4u || heights.Size() < vertexCount || normals.Size() < vertexCount)
    {
        return;
    }

    const float snowLine = MathUtil::Max(params.mountainAmplitude, 1.0f) * params.snowLineFraction;

    for (uint32 z = 0; z < cellSize; z++)
    {
        for (uint32 x = 0; x < cellSize; x++)
        {
            const size_t index = size_t(z) * cellSize + x;
            const float height = heights[index];

            // local mesh normal -> world normal under the cell's (scale.x, 1, scale.z) transform
            const Vec3f& localNormal = normals[index];

            const float worldNormalX = localNormal.x / scaleXZ.x;
            const float worldNormalZ = localNormal.z / scaleXZ.y;

            const float normalY = localNormal.y
                / MathUtil::Max(std::sqrt(worldNormalX * worldNormalX + localNormal.y * localNormal.y + worldNormalZ * worldNormalZ), 1e-6f);

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
    Array<float> previousHeights;
    previousHeights.Resize(paddedHeights.Size());

    const uint32 ring = ApplyThermalErosion(paddedHeights, previousHeights.ToSpan(), size, 1u, scaleXZ);

    ApplyHydraulicPasses(paddedHeights, previousHeights.ToSpan(), size, ring, scaleXZ);
}

uint32 TerrainGenerator::ApplyThermalErosion(Span<float> heights, Span<float> previousHeights, uint32 size, uint32 ring, const Vec2f& scaleXZ) const
{
    const TerrainGenerationParams& params = m_params;

    const float step = (scaleXZ.x + scaleXZ.y) * 0.5f;
    const float talus = std::tan(params.talusAngle * MathUtil::pi<float> / 180.0f) * step;

    constexpr int32 NeighborOffsets[8][2] = {
        { -1, -1 }, { 0, -1 }, { 1, -1 },
        { -1,  0 },            { 1,  0 },
        { -1,  1 }, { 0,  1 }, { 1,  1 }
    };

    constexpr float Diagonal = 1.41421356f;

    constexpr float NeighborDistances[8] = {
        Diagonal, 1.0f, Diagonal,
        1.0f,           1.0f,
        Diagonal, 1.0f, Diagonal
    };

    // split across 8 neighbors so a spike can't overshoot below its surroundings in one iteration
    const float transferRate = params.thermalErosionRate * 0.125f;

    for (uint32 iteration = 0; iteration < params.thermalErosionIterations && ring * 2u < size; iteration++, ring++)
    {
        Memory::Copy(previousHeights.Data(), heights.Data(), heights.Size() * sizeof(float));

        for (int32 z = int32(ring); z < int32(size - ring); z++)
        {
            for (int32 x = int32(ring); x < int32(size - ring); x++)
            {
                const size_t index = size_t(z) * size + size_t(x);
                const float height = previousHeights[index];

                float change = 0.0f;

                for (uint32 n = 0; n < 8; n++)
                {
                    const size_t neighborIndex = size_t(z + NeighborOffsets[n][1]) * size + size_t(x + NeighborOffsets[n][0]);

                    const float difference = height - previousHeights[neighborIndex];
                    const float excess = MathUtil::Abs(difference) - talus * NeighborDistances[n];

                    if (excess <= 0.0f)
                    {
                        continue;
                    }

                    // the neighbor computes the exact opposite transfer, so material is conserved
                    // without this cell ever writing outside itself
                    change -= MathUtil::Sign(difference) * excess;
                }

                heights[index] = height + change * transferRate;
            }
        }
    }

    return ring;
}

uint32 TerrainGenerator::ApplyHydraulicPasses(Span<float> heights, Span<float> previousHeights, uint32 size, uint32 ring, const Vec2f& scaleXZ) const
{
    const TerrainGenerationParams& params = m_params;

    const float step = MathUtil::Max((scaleXZ.x + scaleXZ.y) * 0.5f, 1e-6f);

    // sediment fill on gentle slopes, channel carving on steep ones
    for (uint32 iteration = 0; iteration < params.hydraulicSmoothingIterations && ring * 2u < size; iteration++, ring++)
    {
        Memory::Copy(previousHeights.Data(), heights.Data(), heights.Size() * sizeof(float));

        for (int32 z = int32(ring); z < int32(size - ring); z++)
        {
            for (int32 x = int32(ring); x < int32(size - ring); x++)
            {
                const size_t index = size_t(z) * size + size_t(x);

                const float height = previousHeights[index];
                const float average = 0.25f
                    * (previousHeights[index - 1]
                        + previousHeights[index + 1]
                        + previousHeights[index - size]
                        + previousHeights[index + size]);

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

    return ring;
}

#pragma endregion TerrainGenerator

} // namespace Hyperion
