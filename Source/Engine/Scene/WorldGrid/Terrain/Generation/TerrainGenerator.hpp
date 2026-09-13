/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Containers/FlatMap.hpp>
#include <Core/Utilities/Span.hpp>

#include <Core/Memory/SharedPtr.hpp>
#include <Core/Threading/AtomicVar.hpp>
#include <Core/Threading/Mutex.hpp>

#include <Core/Math/Vector2.hpp>
#include <Core/Math/Vector3.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

HYP_STRUCT()
struct TerrainGenerationParams
{
    HYP_STRUCT_BODY(TerrainGenerationParams);

    HYP_FIELD(Property = "Seed")
    uint32 seed = 0;

    // rolling hills
    HYP_FIELD(Property = "BaseAmplitude")
    float baseAmplitude = 25.0f;

    HYP_FIELD(Property = "BaseFrequency")
    float baseFrequency = 1.0f / 400.0f;

    HYP_FIELD(Property = "BaseOctaves")
    uint32 baseOctaves = 6;

    // mountain regions
    HYP_FIELD(Property = "MountainRegionFrequency")
    float mountainRegionFrequency = 1.0f / 2200.0f;

    HYP_FIELD(Property = "MountainRegionThreshold")
    float mountainRegionThreshold = 0.2f;

    HYP_FIELD(Property = "MountainRegionFalloff")
    float mountainRegionFalloff = 0.35f;

    // mountain massifs - kept smooth, erosion carves the ridges and valleys
    HYP_FIELD(Property = "MountainAmplitude")
    float mountainAmplitude = 300.0f;

    HYP_FIELD(Property = "MountainFrequency")
    float mountainFrequency = 1.0f / 700.0f;

    HYP_FIELD(Property = "MountainOctaves")
    uint32 mountainOctaves = 6;

    HYP_FIELD(Property = "MountainSharpness")
    float mountainSharpness = 1.3f;

    HYP_FIELD(Property = "MountainRidgeWeight")
    float mountainRidgeWeight = 0.4f;

    // domain warping
    HYP_FIELD(Property = "WarpStrength")
    float warpStrength = 80.0f;

    HYP_FIELD(Property = "WarpFrequency")
    float warpFrequency = 1.0f / 1100.0f;

    // hydraulic erosion, simulated over overlapping world-aligned regions
    HYP_FIELD(Property = "ErosionIterations")
    uint32 erosionIterations = 60;

    HYP_FIELD(Property = "ErosionSpacing")
    float erosionSpacing = 2.0f;

    HYP_FIELD(Property = "ErosionRegionSize")
    uint32 erosionRegionSize = 384;

    HYP_FIELD(Property = "ErosionRegionBlend")
    uint32 erosionRegionBlend = 48;

    HYP_FIELD(Property = "ErosionRegionApron")
    uint32 erosionRegionApron = 96;

    HYP_FIELD(Property = "ErosionRoutingInterval")
    uint32 erosionRoutingInterval = 4;

    HYP_FIELD(Property = "Erodibility")
    float erodibility = 0.06f;

    HYP_FIELD(Property = "HillslopeDiffusion")
    float hillslopeDiffusion = 0.05f;

    HYP_FIELD(Property = "TalusAngle")
    float talusAngle = 50.0f;

    // auto splat
    HYP_FIELD(Property = "AutoPaintSplats")
    bool autoPaintSplats = true;

    HYP_FIELD(Property = "SnowLineFraction")
    float snowLineFraction = 0.55f;

    HYP_FORCE_INLINE bool operator==(const TerrainGenerationParams& other) const = default;
    HYP_FORCE_INLINE bool operator!=(const TerrainGenerationParams& other) const = default;
};

class ENGINE_API TerrainGenerator
{
public:
    ///rings of extra samples around a cell in GeneratePaddedCellHeights, so border normals see real neighbors
    static constexpr uint32 CellPadding = 1;

    TerrainGenerator();
    TerrainGenerator(const TerrainGenerator& other) = delete;
    TerrainGenerator& operator=(const TerrainGenerator& other) = delete;
    virtual ~TerrainGenerator();

    void Configure(const TerrainGenerationParams& params);

    HYP_FORCE_INLINE const TerrainGenerationParams& GetParams() const
    {
        return m_params;
    }

    HYP_FORCE_INLINE float GetMaxHeightEstimate() const
    {
        return m_params.baseAmplitude + m_params.mountainAmplitude;
    }

    static HYP_FORCE_INLINE Vec3f ComputeGridNormal(float heightLeft, float heightRight, float heightDown, float heightUp)
    {
        const Vec3f tangentX(2.0f, heightRight - heightLeft, 0.0f);
        const Vec3f tangentZ(0.0f, heightUp - heightDown, 2.0f);

        return tangentZ.Cross(tangentX).Normalized();
    }

    ///identifies everything that affects generated heights for a cell; saved heights with a different fingerprint are stale
    uint64 ComputeFingerprint(uint32 cellSize, const Vec2f& scaleXZ) const;

    ///uneroded input terrain
    virtual float SampleBaseHeight(const Vec2f& worldXZ) const;

    void GenerateCellHeights(
        const Vec2f& cellWorldMinXZ,
        const Vec2f& scaleXZ,
        uint32 cellSize,
        Array<float>& outHeights) const;

    ///splits padded heights into the cell's heights plus local-space grid normals that see across the border
    static void ExtractCellHeightsAndNormals(
        Span<const float> paddedHeights,
        uint32 cellSize,
        Array<float>& outHeights,
        Array<Vec3f>& outNormals);

    Array<Vec2i> CollectRegionsForArea(const Vec2f& areaMinXZ, const Vec2f& areaMaxXZ) const;

    bool TryBeginRegionBuild(const Vec2i& regionCoord) const;
    void BuildQueuedRegion(const Vec2i& regionCoord) const;

    ///(cellSize + 2 * CellPadding)^2 eroded heights
    void GeneratePaddedCellHeights(
        const Vec2f& cellWorldMinXZ,
        const Vec2f& scaleXZ,
        uint32 cellSize,
        Array<float>& outPaddedHeights) const;

    void SynthesizeSplatWeights(
        Span<const float> heights,
        Span<const Vec3f> normals,
        const Vec2f& cellWorldMinXZ,
        const Vec2f& scaleXZ,
        uint32 cellSize,
        Span<ubyte> outWeights) const;

protected:
    TerrainGenerationParams m_params;

private:
    struct ErosionRegion;
    struct ErosionRegionEntry;

    SharedPtr<const ErosionRegion> GetOrBuildErosionRegion(const Vec2i& regionCoord) const;
    SharedPtr<const ErosionRegion> BuildErosionRegion(const Vec2i& regionCoord) const;

    mutable Mutex m_erosionRegionsMutex;
    mutable FlatMap<Vec2i, SharedPtr<ErosionRegionEntry>> m_erosionRegions;
    mutable uint64 m_erosionRegionUseCounter = 0;
};

} // namespace Hyperion
