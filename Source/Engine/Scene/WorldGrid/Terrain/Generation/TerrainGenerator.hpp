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

    // dirt follows drainage channels with at least 2^this many upstream erosion samples.
    // Raise this to cut the number of channels - it is the only knob that should be used for that.
    HYP_FIELD(Property = "ChannelFlowLog2")
    float channelFlowLog2 = 11.0f;

    // how many more log2 steps of flow it takes to reach bare channel bed. Keep this narrow: the shader's
    // height blend is a switch, not a gradient, so a layer left sitting at 0.05-0.40 weight renders as
    // isolated specks rather than a faint bed. A wide range puts most of a channel's length in that band.
    HYP_FIELD(Property = "ChannelFlowRange")
    float channelFlowRange = 1.0f;

    // rubble where erosion cut this much deeper than its surroundings, in world units
    HYP_FIELD(Property = "RubbleIncisionDepth")
    float rubbleIncisionDepth = 2.5f;

    // rubble where hillslope transport piled up this much material in a hollow, in world units
    HYP_FIELD(Property = "RubbleDepositionDepth")
    float rubbleDepositionDepth = 2.0f;

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

    HYP_FORCE_INLINE void Cancel()
    {
        m_cancelled.Set(true, MemoryOrder::RELEASE);
    }

    HYP_FORCE_INLINE bool IsCancelled() const
    {
        return m_cancelled.Get(MemoryOrder::ACQUIRE);
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

    ///appends the erosion regions GeneratePaddedCellHeights samples for the cell (none if erosion is disabled)
    void CollectRegionsForCell(
        const Vec2f& cellWorldMinXZ,
        const Vec2f& scaleXZ,
        uint32 cellSize,
        Array<Vec2i, StreamingTempAllocator>& outRegionCoords) const;

    bool TryBeginRegionBuild(const Vec2i& regionCoord) const;
    void BuildQueuedRegion(const Vec2i& regionCoord) const;

    ///(cellSize + 2 * CellPadding)^2 eroded heights, plus cellSize^2 TerrainErosionMasks if \p outErosionMasks is given
    void GeneratePaddedCellHeights(
        const Vec2f& cellWorldMinXZ,
        const Vec2f& scaleXZ,
        uint32 cellSize,
        Array<float>& outPaddedHeights,
        Array<ubyte>* outErosionMasks = nullptr) const;

    ///\p erosionMasks may be empty (e.g. cells sculpted before masks were saved) - layers then follow slope and noise only
    void SynthesizeSplatWeights(
        Span<const float> paddedHeights,
        Span<const ubyte> erosionMasks,
        const Vec2f& cellWorldMinXZ,
        const Vec2f& scaleXZ,
        uint32 cellSize,
        Span<ubyte> outWeights) const;

protected:
    TerrainGenerationParams m_params;

private:
    struct ErosionRegion;
    struct ErosionRegionEntry;

    SharedPtr<ErosionRegionEntry> FindOrAddErosionRegionEntry(const Vec2i& regionCoord) const;
    SharedPtr<const ErosionRegion> EnsureErosionRegionBuilt(ErosionRegionEntry& entry, const Vec2i& regionCoord) const;
    SharedPtr<const ErosionRegion> GetOrBuildErosionRegion(const Vec2i& regionCoord) const;
    SharedPtr<const ErosionRegion> BuildErosionRegion(const Vec2i& regionCoord) const;

    mutable Mutex m_erosionRegionsMutex;
    mutable FlatMap<Vec2i, SharedPtr<ErosionRegionEntry>> m_erosionRegions;
    mutable uint64 m_erosionRegionUseCounter = 0;

    AtomicVar<bool> m_cancelled { false };
};

} // namespace Hyperion
