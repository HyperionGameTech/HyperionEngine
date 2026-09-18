/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <Scene/WorldGrid/Terrain/Generation/TerrainGenerator.hpp>
#include <Scene/WorldGrid/Terrain/Generation/TerrainNoise.hpp>
#include <Scene/WorldGrid/Terrain/Generation/TerrainErosion.hpp>

#include <Core/HashCode.hpp>

#include <cmath>

#include <TerrainGenerator.generated.inl>

namespace Hyperion {

#pragma region Helpers

// ~3 MB per region (heights + erosion masks) - generous budget keeps revisited areas from paying a rebuild
static constexpr uint32 MaxCachedErosionRegions = 64;

struct ErosionLayout
{
    float spacing = 1.0f;
    int32 stride = 1;
    int32 blend = 2;
    int32 apron = 4;
    int32 size = 1;
};

static ErosionLayout GetErosionLayout(const TerrainGenerationParams& params)
{
    ErosionLayout layout;
    layout.spacing = MathUtil::Max(params.erosionSpacing, 0.01f);
    layout.blend = int32(MathUtil::Max(params.erosionRegionBlend & ~1u, 2u));
    layout.stride = int32(MathUtil::Max(params.erosionRegionSize, uint32(layout.blend) * 2u));
    layout.apron = int32(MathUtil::Max(params.erosionRegionApron, 4u));
    layout.size = layout.stride + layout.blend + layout.apron * 2;

    return layout;
}

struct ErosionAxisBlend
{
    int32 regions[2];
    float weights[2];
};

// regions tile the world with a stride; neighbors overlap by the blend band, where their weights cross-fade and sum to one
static ErosionAxisBlend ComputeErosionAxisBlend(float sampleCoord, const ErosionLayout& layout)
{
    const int32 region = int32(std::floor(sampleCoord / float(layout.stride)));
    const float offset = sampleCoord - float(region) * float(layout.stride);
    const float halfBlend = float(layout.blend) * 0.5f;

    if (offset < halfBlend)
    {
        const float fade = TerrainSmoothStep(-halfBlend, halfBlend, offset);

        return ErosionAxisBlend { { region - 1, region }, { 1.0f - fade, fade } };
    }

    if (offset > float(layout.stride) - halfBlend)
    {
        const float fade = TerrainSmoothStep(-halfBlend, halfBlend, offset - float(layout.stride));

        return ErosionAxisBlend { { region, region + 1 }, { 1.0f - fade, fade } };
    }

    return ErosionAxisBlend { { region, region }, { 1.0f, 0.0f } };
}

struct ErosionRegionRange
{
    int32 minX = 0;
    int32 maxX = -1;
    int32 minZ = 0;
    int32 maxZ = -1;
};

// every region sampled by a cell's padded heights
static ErosionRegionRange ComputePaddedCellRegionRange(const ErosionLayout& layout, const Vec2f& cellWorldMinXZ, const Vec2f& scaleXZ, uint32 cellSize)
{
    const Vec2f worldCornerA = cellWorldMinXZ - Vec2f(float(TerrainGenerator::CellPadding)) * scaleXZ;
    const Vec2f worldCornerB = cellWorldMinXZ + Vec2f(float(cellSize + TerrainGenerator::CellPadding - 1)) * scaleXZ;

    return ErosionRegionRange {
        .minX = ComputeErosionAxisBlend(MathUtil::Min(worldCornerA.x, worldCornerB.x) / layout.spacing, layout).regions[0],
        .maxX = ComputeErosionAxisBlend(MathUtil::Max(worldCornerA.x, worldCornerB.x) / layout.spacing, layout).regions[1],
        .minZ = ComputeErosionAxisBlend(MathUtil::Min(worldCornerA.y, worldCornerB.y) / layout.spacing, layout).regions[0],
        .maxZ = ComputeErosionAxisBlend(MathUtil::Max(worldCornerA.y, worldCornerB.y) / layout.spacing, layout).regions[1]
    };
}

HYP_FORCE_INLINE static float CatmullRom(float p0, float p1, float p2, float p3, float t)
{
    return p1 + 0.5f * t * (p2 - p0 + t * (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3 + t * (3.0f * (p1 - p2) + p3 - p0)));
}

#pragma endregion Helpers

#pragma region TerrainGenerator

struct TerrainGenerator::ErosionRegion
{
    int32 originX = 0;
    int32 originZ = 0;
    uint32 size = 0;
    Array<float> heights;
    Array<ubyte> erosionMasks;

    ///adds the bilinearly sampled masks, scaled by \p weight, to \p outChannels
    void AccumulateErosionMasks(float sampleX, float sampleZ, float weight, float outChannels[TerrainErosionMasks::NumChannels]) const
    {
        const float localX = sampleX - float(originX);
        const float localZ = sampleZ - float(originZ);

        const int32 cellX = MathUtil::Clamp(int32(std::floor(localX)), 0, int32(size) - 2);
        const int32 cellZ = MathUtil::Clamp(int32(std::floor(localZ)), 0, int32(size) - 2);

        const float fractionX = MathUtil::Clamp(localX - float(cellX), 0.0f, 1.0f);
        const float fractionZ = MathUtil::Clamp(localZ - float(cellZ), 0.0f, 1.0f);

        const ubyte* topLeft = erosionMasks.Data() + (size_t(cellZ) * size + size_t(cellX)) * TerrainErosionMasks::NumChannels;
        const ubyte* topRight = topLeft + TerrainErosionMasks::NumChannels;
        const ubyte* bottomLeft = topLeft + size_t(size) * TerrainErosionMasks::NumChannels;
        const ubyte* bottomRight = bottomLeft + TerrainErosionMasks::NumChannels;

        for (uint32 channel = 0; channel < TerrainErosionMasks::NumChannels; channel++)
        {
            const float top = float(topLeft[channel]) + (float(topRight[channel]) - float(topLeft[channel])) * fractionX;
            const float bottom = float(bottomLeft[channel]) + (float(bottomRight[channel]) - float(bottomLeft[channel])) * fractionX;

            outChannels[channel] += weight * (top + (bottom - top) * fractionZ);
        }
    }

    float SampleBicubic(float sampleX, float sampleZ) const
    {
        const float localX = sampleX - float(originX);
        const float localZ = sampleZ - float(originZ);

        const int32 cellX = int32(std::floor(localX));
        const int32 cellZ = int32(std::floor(localZ));

        AssertDebug(cellX >= 1 && cellZ >= 1 && cellX + 2 < int32(size) && cellZ + 2 < int32(size));

        const float fractionX = localX - float(cellX);
        const float fractionZ = localZ - float(cellZ);

        float rows[4];

        for (int32 row = 0; row < 4; row++)
        {
            const float* rowHeights = heights.Data() + size_t(cellZ - 1 + row) * size + size_t(cellX - 1);

            rows[row] = CatmullRom(rowHeights[0], rowHeights[1], rowHeights[2], rowHeights[3], fractionX);
        }

        return CatmullRom(rows[0], rows[1], rows[2], rows[3], fractionZ);
    }
};

struct TerrainGenerator::ErosionRegionEntry
{
    Mutex buildMutex;
    SharedPtr<const ErosionRegion> region;
    // set while an async build is queued/pending, so duplicate requests don't spawn duplicate builds
    AtomicVar<uint8> buildQueued { 0 };
    // set once region has been assigned - safe to check without holding buildMutex
    AtomicVar<uint8> built { 0 };
    // threads building or waiting on region - a queued build skips a claimed region instead of blocking its worker
    AtomicVar<uint32> numBuildClaims { 0 };
    uint64 lastUse = 0;
};

TerrainGenerator::TerrainGenerator() = default;

TerrainGenerator::~TerrainGenerator() = default;

void TerrainGenerator::Configure(const TerrainGenerationParams& params)
{
    Mutex::Guard guard(m_erosionRegionsMutex);

    m_params = params;
    m_erosionRegions.Clear();
}

uint64 TerrainGenerator::ComputeFingerprint(uint32 cellSize, const Vec2f& scaleXZ) const
{
    // bump when the generation or erosion algorithms change so previously saved heights regenerate
    // 2: erosion masks are saved with the heights
    static constexpr uint32 GeneratorVersion = 2;

    const TerrainGenerationParams& params = m_params;

    // every param that affects heights must be listed here
    HashCode hashCode;
    hashCode.Add(GeneratorVersion);
    hashCode.Add(cellSize);
    hashCode.Add(scaleXZ.x);
    hashCode.Add(scaleXZ.y);
    hashCode.Add(CellPadding);
    hashCode.Add(params.seed);
    hashCode.Add(params.baseAmplitude);
    hashCode.Add(params.baseFrequency);
    hashCode.Add(params.baseOctaves);
    hashCode.Add(params.mountainRegionFrequency);
    hashCode.Add(params.mountainRegionThreshold);
    hashCode.Add(params.mountainRegionFalloff);
    hashCode.Add(params.mountainAmplitude);
    hashCode.Add(params.mountainFrequency);
    hashCode.Add(params.mountainOctaves);
    hashCode.Add(params.mountainSharpness);
    hashCode.Add(params.mountainRidgeWeight);
    hashCode.Add(params.warpStrength);
    hashCode.Add(params.warpFrequency);
    hashCode.Add(params.erosionIterations);
    hashCode.Add(params.erosionSpacing);
    hashCode.Add(params.erosionRegionSize);
    hashCode.Add(params.erosionRegionBlend);
    hashCode.Add(params.erosionRegionApron);
    hashCode.Add(params.erosionRoutingInterval);
    hashCode.Add(params.erodibility);
    hashCode.Add(params.hillslopeDiffusion);
    hashCode.Add(params.talusAngle);

    return uint64(hashCode.Value());
}

float TerrainGenerator::SampleBaseHeight(const Vec2f& worldXZ) const
{
    const TerrainGenerationParams& params = m_params;

    const Vec2f warped(
        worldXZ.x + TerrainFbm2D(params.seed ^ 0x51ED2701u, worldXZ.x * params.warpFrequency + 11.3f, worldXZ.y * params.warpFrequency + 7.7f, 3) * params.warpStrength,
        worldXZ.y + TerrainFbm2D(params.seed ^ 0x68BC21EBu, worldXZ.x * params.warpFrequency + 3.1f, worldXZ.y * params.warpFrequency + 17.9f, 3) * params.warpStrength);

    const float region = TerrainFbm2D(params.seed ^ 0x77E1D2A4u, worldXZ.x * params.mountainRegionFrequency, worldXZ.y * params.mountainRegionFrequency, 3) * 0.5f + 0.5f;

    const float mountainMask = TerrainSmoothStep(
        params.mountainRegionThreshold,
        params.mountainRegionThreshold + params.mountainRegionFalloff,
        region);

    const float hills = TerrainFbm2D(
        params.seed ^ 0x11F0A3E9u,
        warped.x * params.baseFrequency,
        warped.y * params.baseFrequency,
        params.baseOctaves)
        * params.baseAmplitude;

    if (mountainMask <= 0.0f)
    {
        return hills;
    }

    const float massif = TerrainFbm2D(
        params.seed ^ 0x3A1F77C3u,
        warped.x * params.mountainFrequency * 0.6f,
        warped.y * params.mountainFrequency * 0.6f,
        params.mountainOctaves)
        * 0.5f + 0.5f;

    const float ridge = TerrainRidged2D(
        params.seed ^ 0x22B7C9F5u,
        warped.x * params.mountainFrequency,
        warped.y * params.mountainFrequency,
        4,
        2.02f,
        0.4f);

    const float mountains = MathUtil::Pow(TerrainSmoothStep(0.2f, 0.85f, massif), params.mountainSharpness)
        * params.mountainAmplitude
        * (1.0f - params.mountainRidgeWeight + params.mountainRidgeWeight * ridge);

    return hills + mountains * mountainMask;
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

    const uint32 paddedSize = cellSize + CellPadding * 2u;

    for (uint32 z = 0; z < cellSize; z++)
    {
        for (uint32 x = 0; x < cellSize; x++)
        {
            outHeights[size_t(z) * cellSize + x] = paddedHeights[size_t(z + CellPadding) * paddedSize + (x + CellPadding)];
        }
    }
}

void TerrainGenerator::ExtractCellHeightsAndNormals(
    Span<const float> paddedHeights,
    uint32 cellSize,
    Array<float>& outHeights,
    Array<Vec3f>& outNormals)
{
    outHeights.Resize(size_t(cellSize) * size_t(cellSize));
    outNormals.Resize(size_t(cellSize) * size_t(cellSize));

    const uint32 paddedSize = cellSize + CellPadding * 2u;

    Assert(paddedHeights.Size() == size_t(paddedSize) * size_t(paddedSize), "Padded heights have unexpected size");

    const auto paddedHeightAt = [&](int32 x, int32 z) -> float
    {
        return paddedHeights[size_t(z + int32(CellPadding)) * paddedSize + size_t(x + int32(CellPadding))];
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
    Array<float>& outPaddedHeights,
    Array<ubyte>* outErosionMasks) const
{
    HYP_SCOPE;

    const uint32 paddedSize = cellSize + CellPadding * 2u;

    outPaddedHeights.Resize(size_t(paddedSize) * size_t(paddedSize));

    if (outErosionMasks)
    {
        outErosionMasks->Resize(size_t(cellSize) * size_t(cellSize) * TerrainErosionMasks::NumChannels);
    }

    const auto worldPositionAt = [&](uint32 px, uint32 pz) -> Vec2f
    {
        return cellWorldMinXZ + Vec2f(float(int32(px) - int32(CellPadding)), float(int32(pz) - int32(CellPadding))) * scaleXZ;
    };

    if (m_params.erosionIterations == 0)
    {
        for (uint32 pz = 0; pz < paddedSize; pz++)
        {
            for (uint32 px = 0; px < paddedSize; px++)
            {
                outPaddedHeights[size_t(pz) * paddedSize + px] = SampleBaseHeight(worldPositionAt(px, pz));
            }
        }

        if (outErosionMasks)
        {
            TerrainErosionMasks::FillNeutral(*outErosionMasks);
        }

        return;
    }

    const ErosionLayout layout = GetErosionLayout(m_params);

    // resolve every region the cell touches up front, so sampling doesn't lock per vertex
    const ErosionRegionRange regionRange = ComputePaddedCellRegionRange(layout, cellWorldMinXZ, scaleXZ, cellSize);

    const int32 regionMinX = regionRange.minX;
    const int32 regionMaxX = regionRange.maxX;
    const int32 regionMinZ = regionRange.minZ;
    const int32 regionMaxZ = regionRange.maxZ;

    const int32 regionsWide = regionMaxX - regionMinX + 1;
    const int32 regionsDeep = regionMaxZ - regionMinZ + 1;

    Array<SharedPtr<const ErosionRegion>> regions;
    regions.Resize(size_t(regionsWide) * size_t(regionsDeep));

    for (int32 regionZ = regionMinZ; regionZ <= regionMaxZ; regionZ++)
    {
        for (int32 regionX = regionMinX; regionX <= regionMaxX; regionX++)
        {
            regions[size_t(regionZ - regionMinZ) * size_t(regionsWide) + size_t(regionX - regionMinX)] = GetOrBuildErosionRegion(Vec2i(regionX, regionZ));
        }
    }

    for (uint32 pz = 0; pz < paddedSize; pz++)
    {
        for (uint32 px = 0; px < paddedSize; px++)
        {
            const Vec2f worldXZ = worldPositionAt(px, pz);

            const float sampleX = worldXZ.x / layout.spacing;
            const float sampleZ = worldXZ.y / layout.spacing;

            const ErosionAxisBlend blendX = ComputeErosionAxisBlend(sampleX, layout);
            const ErosionAxisBlend blendZ = ComputeErosionAxisBlend(sampleZ, layout);

            const bool isCellSample = outErosionMasks != nullptr
                && px >= CellPadding && px < cellSize + CellPadding
                && pz >= CellPadding && pz < cellSize + CellPadding;

            float height = 0.0f;
            float maskChannels[TerrainErosionMasks::NumChannels] = {};

            for (uint32 bz = 0; bz < 2; bz++)
            {
                for (uint32 bx = 0; bx < 2; bx++)
                {
                    const float weight = blendX.weights[bx] * blendZ.weights[bz];

                    if (weight <= 0.0f)
                    {
                        continue;
                    }

                    const ErosionRegion& region = *regions[size_t(blendZ.regions[bz] - regionMinZ) * size_t(regionsWide) + size_t(blendX.regions[bx] - regionMinX)];

                    height += weight * region.SampleBicubic(sampleX, sampleZ);

                    if (isCellSample)
                    {
                        region.AccumulateErosionMasks(sampleX, sampleZ, weight, maskChannels);
                    }
                }
            }

            outPaddedHeights[size_t(pz) * paddedSize + px] = height;

            if (isCellSample)
            {
                ubyte* masks = outErosionMasks->Data() + (size_t(pz - CellPadding) * cellSize + size_t(px - CellPadding)) * TerrainErosionMasks::NumChannels;

                for (uint32 channel = 0; channel < TerrainErosionMasks::NumChannels; channel++)
                {
                    masks[channel] = ubyte(MathUtil::Clamp(maskChannels[channel] + 0.5f, 0.0f, 255.0f));
                }
            }
        }
    }
}

void TerrainGenerator::CollectRegionsForCell(
    const Vec2f& cellWorldMinXZ,
    const Vec2f& scaleXZ,
    uint32 cellSize,
    Array<Vec2i, StreamingTempAllocator>& outRegionCoords) const
{
    if (m_params.erosionIterations == 0)
    {
        return;
    }

    const ErosionRegionRange regionRange = ComputePaddedCellRegionRange(GetErosionLayout(m_params), cellWorldMinXZ, scaleXZ, cellSize);

    for (int32 regionZ = regionRange.minZ; regionZ <= regionRange.maxZ; regionZ++)
    {
        for (int32 regionX = regionRange.minX; regionX <= regionRange.maxX; regionX++)
        {
            outRegionCoords.PushBack(Vec2i(regionX, regionZ));
        }
    }
}

bool TerrainGenerator::TryBeginRegionBuild(const Vec2i& regionCoord) const
{
    SharedPtr<ErosionRegionEntry> entry;

    {
        Mutex::Guard guard(m_erosionRegionsMutex);

        auto it = m_erosionRegions.Find(regionCoord);

        if (it != m_erosionRegions.End())
        {
            entry = it->second;
        }
        else
        {
            entry = MakeShared<ErosionRegionEntry>();
            m_erosionRegions.Set(regionCoord, entry);
        }

        if (entry->built.Get(MemoryOrder::ACQUIRE))
        {
            entry->lastUse = ++m_erosionRegionUseCounter;

            return false;
        }

        uint8 expected = 0;

        if (!entry->buildQueued.CompareExchangeStrong(expected, uint8(1), MemoryOrder::ACQUIRE_RELEASE))
        {
            // a build is already queued
            return false;
        }

        entry->lastUse = ++m_erosionRegionUseCounter;
    }

    return true;
}

void TerrainGenerator::BuildQueuedRegion(const Vec2i& regionCoord) const
{
    if (!IsCancelled())
    {
        const SharedPtr<ErosionRegionEntry> entry = FindOrAddErosionRegionEntry(regionCoord);

        uint32 expectedClaims = 0;

        // a claimed region is already being built (or awaited) by a thread that will build it if needed
        if (!entry->built.Get(MemoryOrder::ACQUIRE) && entry->numBuildClaims.CompareExchangeStrong(expectedClaims, uint32(1), MemoryOrder::ACQUIRE_RELEASE))
        {
            EnsureErosionRegionBuilt(*entry, regionCoord);

            entry->numBuildClaims.Decrement(1, MemoryOrder::RELEASE);
        }
    }

    {
        Mutex::Guard guard(m_erosionRegionsMutex);

        auto it = m_erosionRegions.Find(regionCoord);

        if (it != m_erosionRegions.End())
        {
            it->second->buildQueued.Set(0, MemoryOrder::RELEASE);
        }
    }
}

SharedPtr<TerrainGenerator::ErosionRegionEntry> TerrainGenerator::FindOrAddErosionRegionEntry(const Vec2i& regionCoord) const
{
    Mutex::Guard guard(m_erosionRegionsMutex);

    SharedPtr<ErosionRegionEntry> entry;

    auto it = m_erosionRegions.Find(regionCoord);

    if (it == m_erosionRegions.End())
    {
        if (m_erosionRegions.Size() >= MaxCachedErosionRegions)
        {
            // cells hold their own references, so evicting a region in use is safe
            auto leastRecentIt = m_erosionRegions.Begin();

            for (auto candidateIt = m_erosionRegions.Begin(); candidateIt != m_erosionRegions.End(); ++candidateIt)
            {
                if (candidateIt->second->lastUse < leastRecentIt->second->lastUse)
                {
                    leastRecentIt = candidateIt;
                }
            }

            m_erosionRegions.Erase(leastRecentIt);
        }

        entry = MakeShared<ErosionRegionEntry>();
        m_erosionRegions.Set(regionCoord, entry);
    }
    else
    {
        entry = it->second;
    }

    entry->lastUse = ++m_erosionRegionUseCounter;

    return entry;
}

SharedPtr<const TerrainGenerator::ErosionRegion> TerrainGenerator::EnsureErosionRegionBuilt(ErosionRegionEntry& entry, const Vec2i& regionCoord) const
{
    // concurrent requests for the same region wait here for the first build instead of duplicating it
    Mutex::Guard buildGuard(entry.buildMutex);

    if (!entry.region)
    {
        entry.region = BuildErosionRegion(regionCoord);
        entry.built.Set(1, MemoryOrder::RELEASE);
    }

    return entry.region;
}

SharedPtr<const TerrainGenerator::ErosionRegion> TerrainGenerator::GetOrBuildErosionRegion(const Vec2i& regionCoord) const
{
    const SharedPtr<ErosionRegionEntry> entry = FindOrAddErosionRegionEntry(regionCoord);

    entry->numBuildClaims.Increment(1, MemoryOrder::RELEASE);

    SharedPtr<const ErosionRegion> region = EnsureErosionRegionBuilt(*entry, regionCoord);

    entry->numBuildClaims.Decrement(1, MemoryOrder::RELEASE);

    return region;
}

SharedPtr<const TerrainGenerator::ErosionRegion> TerrainGenerator::BuildErosionRegion(const Vec2i& regionCoord) const
{
    HYP_SCOPE;

    const TerrainGenerationParams& params = m_params;
    const ErosionLayout layout = GetErosionLayout(params);

    SharedPtr<ErosionRegion> region = MakeShared<ErosionRegion>();
    region->originX = regionCoord.x * layout.stride - layout.blend / 2 - layout.apron;
    region->originZ = regionCoord.y * layout.stride - layout.blend / 2 - layout.apron;
    region->size = uint32(layout.size);
    region->heights.Resize(size_t(layout.size) * size_t(layout.size));

    for (int32 z = 0; z < layout.size; z++)
    {
        for (int32 x = 0; x < layout.size; x++)
        {
            const Vec2f worldXZ = Vec2f(float(region->originX + x), float(region->originZ + z)) * layout.spacing;

            region->heights[size_t(z) * size_t(layout.size) + size_t(x)] = SampleBaseHeight(worldXZ);
        }
    }

    TerrainErosionSettings settings;
    settings.seed = params.seed;
    settings.spacing = layout.spacing;
    settings.iterations = params.erosionIterations;
    settings.routingInterval = params.erosionRoutingInterval;
    settings.erodibility = params.erodibility;
    settings.diffusion = params.hillslopeDiffusion;
    settings.talusSlope = std::tan(params.talusAngle * MathUtil::pi<float> / 180.0f);

    const Array<float> baseHeights = region->heights;

    Array<float> upstreamSamples;
    upstreamSamples.Resize(region->heights.Size());

    Array<float> depositedDepth;
    depositedDepth.Resize(region->heights.Size());

    TerrainErosionOutputs outputs;
    outputs.upstreamSamples = upstreamSamples.ToSpan();
    outputs.depositedDepth = depositedDepth.ToSpan();

    TerrainErodeHeightfield(region->heights.ToSpan(), region->size, region->originX, region->originZ, settings, outputs);

    region->erosionMasks.Resize(region->heights.Size() * TerrainErosionMasks::NumChannels);

    TerrainBuildErosionMasks(baseHeights, region->heights, upstreamSamples, depositedDepth, region->size, layout.spacing, region->erosionMasks);

    return region;
}

void TerrainGenerator::SynthesizeSplatWeights(
    Span<const float> paddedHeights,
    Span<const ubyte> erosionMasks,
    const Vec2f& cellWorldMinXZ,
    const Vec2f& scaleXZ,
    uint32 cellSize,
    Span<ubyte> outWeights) const
{
    HYP_SCOPE;

    const TerrainGenerationParams& params = m_params;

    const size_t vertexCount = size_t(cellSize) * size_t(cellSize);
    const uint32 paddedSize = cellSize + CellPadding * 2u;

    Assert(outWeights.Size() >= vertexCount * 4u, "Splat weight buffer too small");
    Assert(paddedHeights.Size() == size_t(paddedSize) * size_t(paddedSize), "Padded heights have unexpected size");

    if (outWeights.Size() < vertexCount * 4u || paddedHeights.Size() != size_t(paddedSize) * size_t(paddedSize))
    {
        return;
    }

    const bool hasErosionMasks = erosionMasks.Size() == vertexCount * TerrainErosionMasks::NumChannels;

    Array<float> heights;
    Array<Vec3f> normals;
    ExtractCellHeightsAndNormals(paddedHeights, cellSize, heights, normals);

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

            float flowLog2 = 0.0f;
            float incisionDepth = 0.0f;
            float depositionDepth = 0.0f;
            float concavity = 0.0f;

            if (hasErosionMasks)
            {
                const ubyte* masks = erosionMasks.Data() + index * TerrainErosionMasks::NumChannels;

                flowLog2 = TerrainErosionMasks::DecodeFlowLog2(masks[TerrainErosionMasks::FlowChannel]);
                incisionDepth = TerrainErosionMasks::DecodeDepth(masks[TerrainErosionMasks::IncisionChannel]);
                depositionDepth = TerrainErosionMasks::DecodeDepth(masks[TerrainErosionMasks::DepositionChannel]);
                concavity = TerrainErosionMasks::DecodeConcavity(masks[TerrainErosionMasks::ConcavityChannel]);
            }

            // bare rock takes over on steep slopes, with a noisy boundary
            const float rockStart = 0.30f + breakup * 0.15f;
            const float steepRock = TerrainSmoothStep(rockStart, rockStart + 0.2f, slope);

            // water keeps drainage channels down to dirt, in proportion to how much flow they carry -
            // a trunk gets bare bed, a headwater capillary barely shows
            const float channel = TerrainSmoothStep(
                params.channelFlowLog2,
                params.channelFlowLog2 + MathUtil::Max(params.channelFlowRange, 0.01f),
                flowLog2);

            // scree on the banks erosion cut into, and rubble piled up in hollows and at the foot of slopes
            const float incisionRubble = TerrainSmoothStep(params.rubbleIncisionDepth * 0.5f, params.rubbleIncisionDepth * 1.5f, incisionDepth)
                * TerrainSmoothStep(0.03f, 0.15f, slope);

            const float depositionRubble = TerrainSmoothStep(params.rubbleDepositionDepth * 0.5f, params.rubbleDepositionDepth * 1.5f, depositionDepth)
                * TerrainSmoothStep(0.0f, 0.08f, concavity);

            const float rawRubble = MathUtil::Max(incisionRubble, depositionRubble)
                * (0.6f + 0.4f * patches)
                * (1.0f - channel);

            // Both rubble terms are a product of two smoothsteps, so on gentle ground they settle around
            // 0.2-0.4 over wide areas. Terrain.hlsl's height blend is a switch: a layer left at that weight
            // shows only where its height map pokes through, which reads as grey specks rather than scree.
            // Clear the low end so rubble is either a patch or absent.
            const float rubble = TerrainSmoothStep(0.30f, 0.60f, rawRubble);

            const float rock = MathUtil::Max(steepRock, rubble);

            // snow caps on high ground that isn't too steep to hold snow.
            // TerrainSmoothStep can't take reversed edges - its denominator clamps - so the slope falloff is inverted explicitly.
            const float snowLineJittered = snowLine * (0.85f + breakup * 0.3f);
            const float snow = TerrainSmoothStep(snowLineJittered - snowLine * 0.12f, snowLineJittered + snowLine * 0.12f, height)
                * (1.0f - TerrainSmoothStep(0.35f, 0.55f, slope));

            // dirt in channels and in sparse noisy patches on shallow ground - grass keeps the convex, dry ground that's left
            const float patchDirt = TerrainSmoothStep(0.65f, 0.8f, patches) * 0.5f * (1.0f - rock);

            const float dirt = MathUtil::Max(channel * (1.0f - steepRock), patchDirt)
                * (1.0f - snow);

            const float grass = MathUtil::Max(1.0f - rock - snow - dirt, 0.0f);

            const float weights[4] = { grass, rock, dirt, snow };

            ubyte* outSplat = outWeights.Data() + index * 4u;

            for (uint32 layer = 0; layer < 4; layer++)
            {
                outSplat[layer] = ubyte(MathUtil::Clamp(weights[layer], 0.0f, 1.0f) * 255.0f + 0.5f);
            }
        }
    }
}

#pragma endregion TerrainGenerator

} // namespace Hyperion
