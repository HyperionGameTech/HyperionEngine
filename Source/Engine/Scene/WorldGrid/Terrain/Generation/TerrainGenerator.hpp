/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Utilities/Span.hpp>

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

    HYP_FIELD(Property = "BaseAmplitude")
    float baseAmplitude = 10.0f;

    HYP_FIELD(Property = "BaseFrequency")
    float baseFrequency = 1.0f / 260.0f;

    HYP_FIELD(Property = "BaseOctaves")
    uint32 baseOctaves = 4;

    // mountain regions
    HYP_FIELD(Property = "MountainRegionFrequency")
    float mountainRegionFrequency = 1.0f / 1800.0f;

    HYP_FIELD(Property = "MountainRegionThreshold")
    float mountainRegionThreshold = 0.08f;

    HYP_FIELD(Property = "MountainRegionFalloff")
    float mountainRegionFalloff = 0.42f;

    // mountains
    HYP_FIELD(Property = "MountainAmplitude")
    float mountainAmplitude = 160.0f;

    HYP_FIELD(Property = "MountainFrequency")
    float mountainFrequency = 1.0f / 480.0f;

    HYP_FIELD(Property = "MountainOctaves")
    uint32 mountainOctaves = 6;

    HYP_FIELD(Property = "MountainGain")
    float mountainGain = 0.45f;

    HYP_FIELD(Property = "MountainSharpness")
    float mountainSharpness = 1.5f;

    // domain warping
    HYP_FIELD(Property = "WarpStrength")
    float warpStrength = 40.0f;

    HYP_FIELD(Property = "WarpFrequency")
    float warpFrequency = 1.0f / 900.0f;

    // gully
    HYP_FIELD(Property = "GullyDepth")
    float gullyDepth = 22.0f;

    HYP_FIELD(Property = "GullyFrequency")
    float gullyFrequency = 1.0f / 150.0f;

    HYP_FIELD(Property = "GullyOctaves")
    uint32 gullyOctaves = 5;

    HYP_FIELD(Property = "GullyGain")
    float gullyGain = 0.45f;

    // erosion
    HYP_FIELD(Property = "ThermalErosionIterations")
    uint32 thermalErosionIterations = 8;

    HYP_FIELD(Property = "TalusAngle")
    float talusAngle = 40.0f;

    HYP_FIELD(Property = "ThermalErosionRate")
    float thermalErosionRate = 0.5f;

    HYP_FIELD(Property = "HydraulicSmoothingIterations")
    uint32 hydraulicSmoothingIterations = 3;

    HYP_FIELD(Property = "CarveThreshold")
    float carveThreshold = 1.1f;

    HYP_FIELD(Property = "CarveStrength")
    float carveStrength = 0.04f;

    HYP_FIELD(Property = "DepositStrength")
    float depositStrength = 0.25f;

    // auto splat
    HYP_FIELD(Property = "AutoPaintSplats")
    bool autoPaintSplats = true;

    HYP_FIELD(Property = "SnowLineFraction")
    float snowLineFraction = 0.62f;

    HYP_FORCE_INLINE bool operator==(const TerrainGenerationParams& other) const = default;
    HYP_FORCE_INLINE bool operator!=(const TerrainGenerationParams& other) const = default;
};

class ENGINE_API TerrainGenerator
{
public:
    TerrainGenerator() = default;
    virtual ~TerrainGenerator() = default;

    void Configure(const TerrainGenerationParams& params);

    HYP_FORCE_INLINE const TerrainGenerationParams& GetParams() const
    {
        return m_params;
    }

    HYP_FORCE_INLINE float GetMaxHeightEstimate() const
    {
        return m_params.baseAmplitude + m_params.mountainAmplitude + m_params.gullyDepth * 0.5f;
    }

    static HYP_FORCE_INLINE Vec3f ComputeGridNormal(float heightLeft, float heightRight, float heightDown, float heightUp)
    {
        const Vec3f tangentX(2.0f, heightRight - heightLeft, 0.0f);
        const Vec3f tangentZ(0.0f, heightUp - heightDown, 2.0f);

        return tangentZ.Cross(tangentX).Normalized();
    }

    ///padding each side of GeneratePaddedCellHeights - one ring per erosion iteration, plus one so border normals see real neighbors
    uint32 GetErosionMargin() const;

    virtual float SampleAnalyticHeight(const Vec2f& worldXZ) const;

    void GenerateCellHeights(
        const Vec2f& cellWorldMinXZ,
        const Vec2f& scaleXZ,
        uint32 cellSize,
        Array<float>& outHeights) const;

    void GenerateCellHeightsAndNormals(
        const Vec2f& cellWorldMinXZ,
        const Vec2f& scaleXZ,
        uint32 cellSize,
        Array<float>& outHeights,
        Array<Vec3f>& outNormals) const;

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
    virtual void ApplyErosion(Span<float> paddedHeights, uint32 size, const Vec2f& scaleXZ) const;

    uint32 ApplyThermalErosion(Span<float> heights, Span<float> previousHeights, uint32 size, uint32 ring, const Vec2f& scaleXZ) const;
    uint32 ApplyHydraulicPasses(Span<float> heights, Span<float> previousHeights, uint32 size, uint32 ring, const Vec2f& scaleXZ) const;

    TerrainGenerationParams m_params;
};

} // namespace Hyperion
